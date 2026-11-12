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

    def __init__(self, stacks=1, psqt=False, sqr=False, moe=False):
        super().__init__()
        self.stacks = stacks
        self.psqt = None
        # moe: a mixture of experts in place of the layer stacks - see _moe_heads(). Made at the end,
        # so that a net without it draws every number from the seed as before.
        self.moe = moe
        # sqr: every output of the first dense layer activated twice, as Stockfish does - see
        # activate_l1(). The second layer then reads twice as many inputs; nothing else changes, and
        # without it every number is drawn from the seed as before.
        self.sqr = sqr
        self.l2_input = netfile.L1_SIZE * (2 if sqr else 1)
        # One more row than there are features: it is the padding of a position
        # with fewer than 32 pieces, and it stays zero.
        self.feature_transformer = nn.EmbeddingBag(
            netfile.FEATURE_COUNT + 1, netfile.ACCUMULATOR_SIZE, mode='sum',
            padding_idx=netfile.FEATURE_COUNT)
        self.feature_bias = nn.Parameter(torch.zeros(netfile.ACCUMULATOR_SIZE))
        self.l1 = nn.Linear(netfile.L1_INPUT_SIZE, netfile.L1_SIZE)
        self.l2 = nn.Linear(self.l2_input, netfile.L2_SIZE)
        self.output = nn.Linear(netfile.L2_SIZE, 1)
        nn.init.normal_(self.feature_transformer.weight, std=0.01)
        with torch.no_grad():
            self.feature_transformer.weight[netfile.FEATURE_COUNT].zero_()
        if stacks > 1:
            # Made after everything above has drawn its numbers, so that the single headed part
            # comes out of the seed exactly as it does without stacks.
            single = (self.l1, self.l2, self.output)
            self.l1 = nn.Linear(netfile.L1_INPUT_SIZE, netfile.L1_SIZE * stacks)
            self.l2 = nn.Linear(self.l2_input, netfile.L2_SIZE * stacks)
            self.output = nn.Linear(netfile.L2_SIZE, stacks)
            with torch.no_grad():
                for stacked, one in zip((self.l1, self.l2, self.output), single):
                    stacked.weight.copy_(one.weight.repeat(stacks, 1))
                    stacked.bias.copy_(one.bias.repeat(stacks))
        if psqt:
            # The piece-square part, made last and drawing no random numbers: a run without it comes
            # out of the seed exactly as before. One value per bucket and feature, started from the
            # material, so the net begins as a counter of material and learns the rest.
            self.psqt = nn.EmbeddingBag(netfile.FEATURE_COUNT + 1, netfile.PSQT_BUCKETS, mode='sum',
                                        padding_idx=netfile.FEATURE_COUNT)
            with torch.no_grad():
                self.psqt.weight.copy_(netfile.psqt_start_values())
        if moe:
            # The accumulator gets MOE_ROUTER more columns; a router reads those of both perspectives
            # and picks MOE_TOP of MOE_EXPERTS experts. Each expert is MOE_EXPERT_SIZE neurons of the
            # first layer and its own columns of the second, which is shared by all: the two chosen
            # experts' contributions are summed into it. The layers are replaced as a whole.
            width = netfile.ACCUMULATOR_SIZE + netfile.MOE_ROUTER
            self.feature_transformer = nn.EmbeddingBag(
                netfile.FEATURE_COUNT + 1, width, mode='sum', padding_idx=netfile.FEATURE_COUNT)
            self.feature_bias = nn.Parameter(torch.zeros(width))
            nn.init.normal_(self.feature_transformer.weight, std=0.01)
            with torch.no_grad():
                self.feature_transformer.weight[netfile.FEATURE_COUNT].zero_()
            experts = netfile.MOE_EXPERTS * netfile.MOE_EXPERT_SIZE
            self.router = nn.Linear(2 * netfile.MOE_ROUTER, netfile.MOE_EXPERTS)
            self.l1 = nn.Linear(netfile.L1_INPUT_SIZE, experts)
            self.l2 = nn.Linear(experts, netfile.L2_SIZE)
            self.output = nn.Linear(netfile.L2_SIZE, 1)
            # What the router chose and how sure it was, for the balancing term and the log.
            self.balance = torch.zeros(())
            self.usage = torch.zeros(netfile.MOE_EXPERTS)

    def activate_l1(self, hidden):
        """The activation of the first dense layer's outputs, over the last dimension.

        Without sqr the clipped relu. With it Stockfish's pair: min(x^2, 1) * 127/128 and
        clamp(x, 0, 1), the squares first. The square drops the sign, so an output below zero still
        reaches the next layer. 127/128 is what the engine's integer square comes to: (127 x)^2 / 128
        at the scale 127 (SqrClippedReLU, squaredRelu() in nnue-evaluator.cpp).
        """
        if not self.sqr:
            return hidden.clamp(0.0, 1.0)
        return torch.cat((hidden.pow(2.0) * (127.0 / 128.0), hidden), dim=-1).clamp(0.0, 1.0)

    def forward(self, own_features, opponent_features, csr=None):
        """csr: the batch's features grouped by feature (gamedata csr_batches) - then the feature
        transformer runs on fastft's Metal kernels instead of EmbeddingBag (MPS only, no psqt)."""
        head = self._heads(own_features, opponent_features, csr)
        if self.psqt is None:
            return head
        # Stockfish's psqt: the bucket is chosen as the layer stack, by the number of pieces, and
        # half the difference of the two perspectives' sums goes straight to the output.
        active = (own_features != netfile.FEATURE_COUNT).sum(dim=1)
        bucket = (active // 4).clamp(max=netfile.PSQT_BUCKETS - 1).view(-1, 1)
        own = self.psqt(own_features).gather(1, bucket).squeeze(1)
        opponent = self.psqt(opponent_features).gather(1, bucket).squeeze(1)
        return head + (own - opponent) / 2

    def _heads(self, own_features, opponent_features, csr=None):
        if csr is None:
            own = self.feature_transformer(own_features)
            opponent = self.feature_transformer(opponent_features)
        else:
            import fastft
            own, opponent = fastft.transform(own_features, opponent_features,
                                             self.feature_transformer.weight, csr)
        own = own + self.feature_bias
        opponent = opponent + self.feature_bias
        if self.moe:
            return self._moe_heads(own, opponent)
        hidden = torch.cat((own, opponent), dim=1).clamp(0.0, 1.0)
        if self.stacks == 1:
            hidden = self.activate_l1(self.l1(hidden))
            hidden = self.l2(hidden).clamp(0.0, 1.0)
            return self.output(hidden).squeeze(1)
        # The own features are every piece but the own king, so their number is the pieces less
        # one - and (pieces - 1) // 4 is the stack, layerStackOf() in nnue-arch.h.
        active = (own_features != netfile.FEATURE_COUNT).sum(dim=1)
        rows = (active // 4).clamp(max=self.stacks - 1).view(-1, 1)
        hidden = self.l1(hidden).view(-1, self.stacks, netfile.L1_SIZE)
        hidden = hidden.gather(1, rows.unsqueeze(2).expand(-1, 1, netfile.L1_SIZE))
        hidden = self.activate_l1(hidden.squeeze(1))
        hidden = self.l2(hidden).view(-1, self.stacks, netfile.L2_SIZE)
        hidden = hidden.gather(1, rows.unsqueeze(2).expand(-1, 1, netfile.L2_SIZE))
        hidden = hidden.squeeze(1).clamp(0.0, 1.0)
        return self.output(hidden).gather(1, rows).squeeze(1)

    def _moe_heads(self, own, opponent):
        """The mixture of experts on the two accumulators (each ACCUMULATOR_SIZE + MOE_ROUTER wide).

        The router reads the last MOE_ROUTER values of both perspectives, own first, unclipped - an
        integer of scale QA in the engine - and picks the MOE_TOP experts of the highest logits. In the
        forward pass the chosen experts count one each and the others nothing, exactly what the engine
        computes; the router learns through the backward pass, where the factor is the chosen experts'
        softmax share (straight through: value one, gradient of the share). The balancing term (Switch
        Transformer) pushes against the choice collapsing onto a few experts.
        """
        size = netfile.ACCUMULATOR_SIZE
        hidden = torch.cat((own[:, :size], opponent[:, :size]), dim=1).clamp(0.0, 1.0)
        logits = self.router(torch.cat((own[:, size:], opponent[:, size:]), dim=1))
        chosen = logits.topk(netfile.MOE_TOP, dim=1).indices
        share = torch.softmax(logits, dim=1)
        picked = share.gather(1, chosen)
        picked = picked / picked.sum(dim=1, keepdim=True)
        gate = torch.zeros_like(logits).scatter(1, chosen, picked - picked.detach() + 1.0)
        hidden = self.l1(hidden).clamp(0.0, 1.0).view(-1, netfile.MOE_EXPERTS, netfile.MOE_EXPERT_SIZE)
        hidden = (hidden * gate.unsqueeze(2)).view(-1, netfile.MOE_EXPERTS * netfile.MOE_EXPERT_SIZE)
        hidden = self.l2(hidden).clamp(0.0, 1.0)
        if self.training:
            counts = torch.zeros_like(logits).scatter(1, chosen, 1.0).sum(dim=0)
            fraction = counts / counts.sum()
            self.balance = netfile.MOE_EXPERTS * (fraction.detach() * share.mean(dim=0)).sum()
            self.usage = self.usage.to(counts.device) + counts.detach()
        return self.output(hidden).squeeze(1)

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
        hidden = self.activate_l1(self.l1(hidden).view(-1, self.stacks, netfile.L1_SIZE))
        w2 = self.l2.weight.view(self.stacks, netfile.L2_SIZE, self.l2_input)
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
        if self.moe:
            self.router.weight.clamp_(-netfile.MOE_ROUTER_LIMIT, netfile.MOE_ROUTER_LIMIT)
        # An accumulator of int16 could take far more, but two keeps it away from
        # its limit even with 31 pieces on the board.
        self.feature_transformer.weight.clamp_(-2.0, 2.0)
        self.feature_bias.clamp_(-2.0, 2.0)
        self.feature_transformer.weight[netfile.FEATURE_COUNT].zero_()
        if self.psqt is not None:
            self.psqt.weight[netfile.FEATURE_COUNT].zero_()


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
