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

    def __init__(self):
        super().__init__()
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

    def forward(self, own_features, opponent_features):
        own = self.feature_transformer(own_features) + self.feature_bias
        opponent = self.feature_transformer(opponent_features) + self.feature_bias
        hidden = torch.cat((own, opponent), dim=1).clamp(0.0, 1.0)
        hidden = self.l1(hidden).clamp(0.0, 1.0)
        hidden = self.l2(hidden).clamp(0.0, 1.0)
        return self.output(hidden).squeeze(1)

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


def loss_of(prediction, value, result, blend):
    """The loss of a batch.

    The target is a win probability, and the two labels of a position are brought
    into it: the value of the search through the sigmoid, and the result of the
    game as it stands. blend is the lambda of the usual formulation - one takes the
    search alone, which is what a first net trained on the games of a hand written
    evaluation wants, because the results of those games are too noisy to learn
    from. Later generations move it down towards 0.7.
    """
    predicted = torch.sigmoid(prediction)
    from_search = torch.sigmoid(value / netfile.NET_VALUE_SCALE)
    target = blend * from_search + (1.0 - blend) * result
    # An exponent above two weighs the positions the net is most wrong about more
    # heavily than a mean square error would.
    return ((predicted - target).abs() ** 2.5).mean()
