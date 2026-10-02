"""The net as PyTorch sees it.

The same shape the engine evaluates, in real numbers: the feature transformer as
an EmbeddingBag that sums the columns of the active features, then three dense
layers, every one of them followed by the clipped relu that the quantized
inference uses as well - clamp(x, 0, 1), because an activation of scale QA is an
integer from 0 to QA.

The output is in units of NET_VALUE_SCALE: one is four hundred of the engine's
value units. That is what makes the loss below as simple as sigmoid(prediction) -
the sigmoid scale of the target and the scale of the net are the same number, and
that number lives in netfile.py.

Weights have to survive being quantized into a signed byte, so they are clamped
after every step. A weight that cannot be represented is not a weight the engine
would ever use, and letting the training drift there means training something else
than what will play.
"""

import torch
from torch import nn

import netfile


class HalfKaNet(nn.Module):
    """The net, with one head or with LAYER_STACKS of them.

    With several, the dense layers exist once per phase of the game, chosen by the number of
    pieces, as in the engine's layer stacks. They are computed as one layer with that many times
    the outputs and the head of each position is taken out of it - no loop over the stacks, and the
    gradient reaches only the head a position chose.

    A stacked net starts as the same function as the single headed one drawn from the same seed:
    every head a copy of that one head. Whatever the two runs then end at differently comes from the
    heads being allowed to part, and from nothing else.
    """

    def __init__(self, stacks=1):
        super().__init__()
        self.stacks = stacks
        # One more row than there are features: it is the padding of a position
        # with fewer than 32 pieces, and it stays zero.
        self.feature_transformer = nn.EmbeddingBag(
            netfile.FEATURE_COUNT + 1, netfile.ACCUMULATOR_SIZE, mode='sum',
            padding_idx=netfile.FEATURE_COUNT)
        self.feature_bias = nn.Parameter(torch.zeros(netfile.ACCUMULATOR_SIZE))
        self.l1 = nn.Linear(netfile.L1_INPUT_SIZE, netfile.L1_SIZE)
        self.l2 = nn.Linear(netfile.L1_SIZE, netfile.L2_SIZE)
        self.output = nn.Linear(netfile.L2_SIZE, 1)
        nn.init.normal_(self.feature_transformer.weight, std=0.01)
        with torch.no_grad():
            self.feature_transformer.weight[netfile.FEATURE_COUNT].zero_()
        if stacks > 1:
            # Made after everything above has drawn its numbers, so that the single headed part
            # comes out of the seed exactly as it does without stacks.
            single = (self.l1, self.l2, self.output)
            self.l1 = nn.Linear(netfile.L1_INPUT_SIZE, netfile.L1_SIZE * stacks)
            self.l2 = nn.Linear(netfile.L1_SIZE, netfile.L2_SIZE * stacks)
            self.output = nn.Linear(netfile.L2_SIZE, stacks)
            with torch.no_grad():
                for stacked, one in zip((self.l1, self.l2, self.output), single):
                    stacked.weight.copy_(one.weight.repeat(stacks, 1))
                    stacked.bias.copy_(one.bias.repeat(stacks))

    def forward(self, own_features, opponent_features):
        own = self.feature_transformer(own_features) + self.feature_bias
        opponent = self.feature_transformer(opponent_features) + self.feature_bias
        hidden = torch.cat((own, opponent), dim=1).clamp(0.0, 1.0)
        if self.stacks == 1:
            hidden = self.l1(hidden).clamp(0.0, 1.0)
            hidden = self.l2(hidden).clamp(0.0, 1.0)
            return self.output(hidden).squeeze(1)
        # The own features are every piece but the own king, so their number is the pieces less
        # one - and (pieces - 1) // 4 is the stack, layerStackOf() in nnue-arch.h.
        active = (own_features != netfile.FEATURE_COUNT).sum(dim=1)
        rows = (active // 4).clamp(max=self.stacks - 1).view(-1, 1)
        hidden = self.l1(hidden).view(-1, self.stacks, netfile.L1_SIZE)
        hidden = hidden.gather(1, rows.unsqueeze(2).expand(-1, 1, netfile.L1_SIZE))
        hidden = hidden.squeeze(1).clamp(0.0, 1.0)
        hidden = self.l2(hidden).view(-1, self.stacks, netfile.L2_SIZE)
        hidden = hidden.gather(1, rows.unsqueeze(2).expand(-1, 1, netfile.L2_SIZE))
        hidden = hidden.squeeze(1).clamp(0.0, 1.0)
        return self.output(hidden).gather(1, rows).squeeze(1)

    def all_heads(self, own_features, opponent_features):
        """The output of every head for every position, and the stack each position belongs to.

        The engine only ever evaluates a position with its own head; this is for training the
        neighbours as well. The first dense layer is computed for all heads by forward() too, so
        what this adds is the second layer and the output for every head - small layers, 32 wide.
        """
        own = self.feature_transformer(own_features) + self.feature_bias
        opponent = self.feature_transformer(opponent_features) + self.feature_bias
        hidden = torch.cat((own, opponent), dim=1).clamp(0.0, 1.0)
        active = (own_features != netfile.FEATURE_COUNT).sum(dim=1)
        stack = (active // 4).clamp(max=self.stacks - 1)
        hidden = self.l1(hidden).view(-1, self.stacks, netfile.L1_SIZE).clamp(0.0, 1.0)
        w2 = self.l2.weight.view(self.stacks, netfile.L2_SIZE, netfile.L1_SIZE)
        b2 = self.l2.bias.view(self.stacks, netfile.L2_SIZE)
        hidden = (torch.einsum('bsi,soi->bso', hidden, w2) + b2).clamp(0.0, 1.0)
        w3 = self.output.weight.view(self.stacks, 1, netfile.L2_SIZE)
        b3 = self.output.bias.view(self.stacks, 1)
        return (torch.einsum('bsi,soi->bso', hidden, w3) + b3).squeeze(2), stack

    @torch.no_grad()
    def clamp_weights(self):
        """Keeps every weight inside the range its quantized form can hold."""
        limit = netfile.MAX_DENSE_WEIGHT
        for layer in (self.l1, self.l2, self.output):
            layer.weight.clamp_(-limit, limit)
        # An accumulator of int16 could take far more, but two keeps it away from
        # its limit even with 31 pieces on the board.
        self.feature_transformer.weight.clamp_(-2.0, 2.0)
        self.feature_bias.clamp_(-2.0, 2.0)
        self.feature_transformer.weight[netfile.FEATURE_COUNT].zero_()


def neighbour_loss_of(outputs, stack, value, result, counts, blend, neighbour_weight=1.0):
    """The loss when every position trains its own head and the heads next to it.

    A position of stack i trains heads i-1, i and i+1, where they exist, so head i learns from the
    positions of three stacks and the two at the edges from two. The number of pieces separates the
    phases of a game only roughly - a queen less is nearer to the endgame than two pawns less - so
    the neighbours' positions are often a fair lesson too, and the heads should come out with softer
    transitions between them: less of a jump in the value when a capture moves a position into the
    next stack. The engine still evaluates every position with its own head alone.

    neighbour_weight is what a neighbour counts against the own head; the mean is taken over every
    pair of position and head that takes part, so the scale is that of loss_of.
    """
    stacks = outputs.shape[1]
    offsets = torch.tensor([-1, 0, 1], device=outputs.device)
    heads = stack.view(-1, 1) + offsets.view(1, -1)
    present = ((heads >= 0) & (heads < stacks)).float()
    weights = present * torch.where(offsets == 0, 1.0, neighbour_weight).view(1, -1)
    predicted = torch.sigmoid(outputs.gather(1, heads.clamp(0, stacks - 1)))
    weight = blend + (1.0 - blend) * (1.0 - counts)
    target = (weight * value + (1.0 - weight) * result).view(-1, 1)
    return ((predicted - target).abs() ** 2.5 * weights).sum() / weights.sum()


def loss_of(prediction, value, result, counts, blend):
    """The loss of a batch.

    The target is a win probability, and the two labels of a position are brought into
    it: the value of the search, which the file already holds as a probability, and the
    result of the game as it stands.

    blend is the lambda of the usual formulation. It is never one: the result is the only
    label that can carry what the evaluation the games were played with does not know,
    and its noise is not a reason against it - the net learns feature patterns, not
    positions, so a pattern that is in truth 600 to 400 is labelled 600 times one way and
    400 times the other across the data and comes out at 0.6. Whether 0.7 or 0.9 is
    better is a question for a run, not for an argument.

    counts is zero where the result says nothing about the position - a game between
    players of different strength, where it says something about the players instead -
    and there the value of the search is the whole target, whatever blend says.
    """
    predicted = torch.sigmoid(prediction)
    weight = blend + (1.0 - blend) * (1.0 - counts)
    target = weight * value + (1.0 - weight) * result
    # An exponent above two weighs the positions the net is most wrong about more
    # heavily than a mean square error would.
    return ((predicted - target).abs() ** 2.5).mean()
