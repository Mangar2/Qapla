"""The net file of the engine, written and read in Python.

The counterpart of src/nnue/nnue-arch.h. The layout, the quantization and
NET_VALUE_SCALE are the contract between the trainer and the engine; if one side
changes a number here, the other one has to change it too, and the shape number in
the file header is what makes a mismatch an error instead of noise.

evaluate() below repeats the integer arithmetic of the engine exactly, including
the shifts. It is far too slow to train with and is not meant to be: it is the
test that the features, the file layout and the quantization of the trainer agree
with the engine, down to the value.
"""

from array import array
import struct

MAGIC = b'QAPLANN2'
# The format before layer stacks, a single head. It is still read: its head is copied into every
# stack, and the net evaluates exactly as it did.
MAGIC_SINGLE_HEAD = b'QAPLANN1'
# The format with the piece-square part: the stacked one, and between the feature weights and the
# heads PSQT_BUCKETS int32 values per feature, of the scale QA*QB of the output.
MAGIC_PSQT = b'QAPLANN3'
# The format with the doubled activation of the first layer (model.HalfKaNet.activate_l1): the stacked
# one with a second layer of 2 * L1_SIZE inputs a row, the squares first. No piece-square part.
MAGIC_SQR = b'QAPLANN4'
# The mixture of experts (model.HalfKaNet._moe_heads): an accumulator MOE_ROUTER wider, a router, the experts' first
# layer and a second layer read in expert blocks; no stacks. See write_moe() for the layout.
MAGIC_MOE = b'QAPLANN5'

SQUARE_COUNT = 64
PIECE_PLANES = 11
FEATURE_COUNT = PIECE_PLANES * SQUARE_COUNT * SQUARE_COUNT
ACCUMULATOR_SIZE = 256
L1_INPUT_SIZE = 2 * ACCUMULATOR_SIZE
L1_SIZE = 32
L2_SIZE = 32
# The dense layers exist once per phase, chosen by the number of pieces: see LAYER_STACKS in
# nnue-arch.h. The accumulator is shared.
LAYER_STACKS = 8
PSQT_BUCKETS = LAYER_STACKS
# The mixture of experts: MOE_ROUTER extra accumulator values per perspective feed a router that picks MOE_TOP of
# MOE_EXPERTS experts of MOE_EXPERT_SIZE first-layer neurons each - 2 x 16 = 32 computed, as with the stacks.
MOE_ROUTER = 16
MOE_EXPERTS = 16
MOE_EXPERT_SIZE = 16
MOE_TOP = 2
# The router decides which experts run, and a choice flipped by rounding changes the value by far more than any
# rounded weight: its weights are int16 of scale MOE_ROUTER_SCALE (int8 of scale QB flipped one choice in five on a
# young net), its bias int32 of scale QA * MOE_ROUTER_SCALE. The largest logit stays far inside an int32: 32 inputs of
# at most 8128 times weights of at most MOE_ROUTER_LIMIT * MOE_ROUTER_SCALE = 8192.
MOE_ROUTER_SCALE = 1024
MOE_ROUTER_LIMIT = 8.0

QA = 127                 # scale of an activation, and its largest value
QB = 64                  # scale of a weight of a dense layer
QB_SHIFT = 6             # log2(QB)
NET_VALUE_SCALE = 400    # turns the output into the value unit of the engine

# The largest weight a dense layer may have, so that it survives the quantization
# into a signed byte. The trainer clamps to it after every step.
MAX_DENSE_WEIGHT = 127.0 / QB


def single_head_architecture_id():
    """The shape number of a file of the single head format, singleHeadArchitectureId()."""
    value = (FEATURE_COUNT * 31 + ACCUMULATOR_SIZE * 7 + L1_SIZE * 3 + L2_SIZE
             + QA * 131 + QB * 17)
    return value & 0xFFFFFFFF


def architecture_id():
    """The shape number of the file header, see architectureId() in nnue-arch.h."""
    return (single_head_architecture_id() + LAYER_STACKS * 1009) & 0xFFFFFFFF


def psqt_architecture_id():
    """The shape number of a file with the piece-square part, psqtArchitectureId()."""
    return (architecture_id() + PSQT_BUCKETS * 7919) & 0xFFFFFFFF


def moe_architecture_id():
    """The shape number of a mixture-of-experts file, moeArchitectureId() in nnue-arch.h."""
    return (single_head_architecture_id() + MOE_ROUTER * 7 + MOE_EXPERTS * 7901 + MOE_EXPERT_SIZE * 3
            + MOE_TOP * 911 + MOE_ROUTER_SCALE) & 0xFFFFFFFF


def sqr_architecture_id():
    """The shape number of a file with the doubled activation, sqrArchitectureId()."""
    return (architecture_id() + 2 * L1_SIZE * 7907) & 0xFFFFFFFF


# The values a psqt column starts from: the material of the piece, in output units (one is
# NET_VALUE_SCALE engine units), positive for the own pieces and negative for the opponent's. The
# planes run pawn, knight, bishop, rook, queen with the own piece first; plane 10 is the enemy king.
PSQT_START_MATERIAL = (90, 300, 310, 480, 930)


def psqt_start_values():
    import torch
    values = torch.zeros(FEATURE_COUNT + 1, PSQT_BUCKETS)
    for plane in range(PIECE_PLANES - 1):
        value = PSQT_START_MATERIAL[plane // 2] / NET_VALUE_SCALE * (1 if plane % 2 == 0 else -1)
        for king in range(SQUARE_COUNT):
            start = (king * PIECE_PLANES + plane) * SQUARE_COUNT
            values[start:start + SQUARE_COUNT] = value
    return values


def layer_stack_of(piece_count):
    """The stack a position is judged by, layerStackOf() in nnue-arch.h."""
    return 0 if piece_count == 0 else min((piece_count - 1) // 4, LAYER_STACKS - 1)


class Head:
    """The dense layers of one layer stack, quantized."""

    __slots__ = ('l1_bias', 'l1_weight', 'l2_bias', 'l2_weight', 'output_bias', 'output_weight')

    def __init__(self, sqr=False):
        self.l1_bias = array('i', [0]) * L1_SIZE
        self.l1_weight = array('b', [0]) * (L1_SIZE * L1_INPUT_SIZE)
        self.l2_bias = array('i', [0]) * L2_SIZE
        self.l2_weight = array('b', [0]) * (L2_SIZE * L1_SIZE * (2 if sqr else 1))
        self.output_bias = 0
        self.output_weight = array('b', [0]) * L2_SIZE


class Network:
    """The quantized weights, in the order they stand in the file."""

    __slots__ = ('feature_bias', 'feature_weight', 'psqt_weight', 'heads', 'stacked', 'sqr')

    def __init__(self):
        self.feature_bias = array('h', [0]) * ACCUMULATOR_SIZE
        self.feature_weight = array('h', [0]) * (FEATURE_COUNT * ACCUMULATOR_SIZE)
        self.psqt_weight = None      # array('i'), PSQT_BUCKETS per feature, or None
        self.heads = [Head() for _ in range(LAYER_STACKS)]
        self.stacked = True
        self.sqr = False             # the doubled activation of the first layer, format QAPLANN4


# The arrays of a head in their order: name, type code and length. The output bias, a single int32,
# stands between l2_weight and output_weight.
_HEAD_MEMBERS = (
    ('l1_bias', 'i', L1_SIZE),
    ('l1_weight', 'b', L1_SIZE * L1_INPUT_SIZE),
    ('l2_bias', 'i', L2_SIZE),
    ('l2_weight', 'b', L2_SIZE * L1_SIZE),
)
assert array('h').itemsize == 2 and array('i').itemsize == 4 and array('b').itemsize == 1


def configure(accumulator=256, l1=32):
    """Sets the width of the accumulator and of the first dense layer, before any net is made.

    The defaults are the net as it has been. The engine has the same sizes as compile time constants;
    the shape number of the file follows from them on both sides, so a net of other sizes is refused
    by an engine built for the old ones instead of being misread.
    """
    global ACCUMULATOR_SIZE, L1_INPUT_SIZE, L1_SIZE, _HEAD_MEMBERS
    ACCUMULATOR_SIZE = accumulator
    L1_INPUT_SIZE = 2 * ACCUMULATOR_SIZE
    L1_SIZE = l1
    _HEAD_MEMBERS = (
        ('l1_bias', 'i', L1_SIZE),
        ('l1_weight', 'b', L1_SIZE * L1_INPUT_SIZE),
        ('l2_bias', 'i', L2_SIZE),
        ('l2_weight', 'b', L2_SIZE * L1_SIZE),
    )


def _read_array(stream, code, count):
    # fromfile appends, so the array has to start out empty.
    target = array(code)
    target.fromfile(stream, count)
    return target


def _head_members(sqr):
    """_HEAD_MEMBERS, with the second layer twice as wide in a net of the doubled activation."""
    if not sqr:
        return _HEAD_MEMBERS
    return tuple((member, code, count * 2 if member == 'l2_weight' else count)
                 for member, code, count in _HEAD_MEMBERS)


def _read_head(stream, sqr=False):
    head = Head()
    for member, code, count in _head_members(sqr):
        setattr(head, member, _read_array(stream, code, count))
    head.output_bias = struct.unpack('<i', stream.read(4))[0]
    head.output_weight = _read_array(stream, 'b', L2_SIZE)
    return head


def read(path):
    """Reads a net file of either format, raising ValueError if it is not one of these shapes."""
    network = Network()
    with open(path, 'rb') as stream:
        magic = stream.read(len(MAGIC))
        identifier = struct.unpack('<I', stream.read(4))[0]
        if magic not in (MAGIC, MAGIC_SINGLE_HEAD, MAGIC_PSQT, MAGIC_SQR):
            raise ValueError('%s is not a net file' % path)
        network.sqr = magic == MAGIC_SQR
        wanted = (psqt_architecture_id() if magic == MAGIC_PSQT else
                  sqr_architecture_id() if network.sqr else
                  architecture_id() if magic == MAGIC else single_head_architecture_id())
        if identifier != wanted:
            raise ValueError('%s has shape %d, this code wants %d' % (path, identifier, wanted))
        network.feature_bias = _read_array(stream, 'h', ACCUMULATOR_SIZE)
        network.feature_weight = _read_array(stream, 'h', FEATURE_COUNT * ACCUMULATOR_SIZE)
        if magic == MAGIC_PSQT:
            network.psqt_weight = _read_array(stream, 'i', FEATURE_COUNT * PSQT_BUCKETS)
        if magic in (MAGIC, MAGIC_PSQT, MAGIC_SQR):
            network.heads = [_read_head(stream, network.sqr) for _ in range(LAYER_STACKS)]
        else:
            head = _read_head(stream)
            network.heads = [head] * LAYER_STACKS
    return network


def write(path, network, single_head=False):
    """Writes a net file. This is what the exporter of the trainer produces.

    single_head writes the old format with the first head alone - for a net whose heads are all the
    same, which every engine binary can read, also the ones built before there were stacks.
    """
    psqt = network.psqt_weight is not None
    sqr = network.sqr
    if psqt and sqr:
        raise ValueError('no net format has both the piece-square part and the doubled activation')
    single_head = single_head and not psqt and not sqr
    with open(path, 'wb') as stream:
        stream.write(MAGIC_PSQT if psqt else MAGIC_SQR if sqr else
                     MAGIC_SINGLE_HEAD if single_head else MAGIC)
        stream.write(struct.pack('<I', psqt_architecture_id() if psqt else
                                 sqr_architecture_id() if sqr else
                                 single_head_architecture_id() if single_head
                                 else architecture_id()))
        network.feature_bias.tofile(stream)
        network.feature_weight.tofile(stream)
        if psqt:
            network.psqt_weight.tofile(stream)
        for head in network.heads[:1] if single_head else network.heads:
            for member, _code, _count in _head_members(sqr):
                getattr(head, member).tofile(stream)
            stream.write(struct.pack('<i', head.output_bias))
            head.output_weight.tofile(stream)


def _clipped_relu(value):
    return 0 if value < 0 else (QA if value > QA else value)


def _truncating_division(numerator, denominator):
    """Integer division that cuts towards zero, as C++ does and Python does not."""
    quotient = abs(numerator) // denominator
    return quotient if numerator >= 0 else -quotient


def _accumulator(network, active_features):
    accumulator = list(network.feature_bias)
    weight = network.feature_weight
    for feature in active_features:
        offset = feature * ACCUMULATOR_SIZE
        for index in range(ACCUMULATOR_SIZE):
            accumulator[index] += weight[offset + index]
    return accumulator


def _squared_relu(value):
    """squaredRelu() of nnue-evaluator.cpp: (value^2) >> 7, cut at QA."""
    bounded = min(abs(value), 128)
    return min((bounded * bounded) >> 7, QA)


def _affine_sqr_relu(inputs, weight, bias, output_size):
    """The first layer of the doubled activation: the squares, then the clipped relus."""
    input_size = len(inputs)
    values = []
    for out in range(output_size):
        offset = out * input_size
        total = bias[out]
        for index in range(input_size):
            total += weight[offset + index] * inputs[index]
        values.append(total >> QB_SHIFT)
    return [_squared_relu(v) for v in values] + [_clipped_relu(v) for v in values]


def _affine_relu(inputs, weight, bias, output_size):
    input_size = len(inputs)
    output = []
    for out in range(output_size):
        offset = out * input_size
        total = bias[out]
        for index in range(input_size):
            total += weight[offset + index] * inputs[index]
        output.append(_clipped_relu(total >> QB_SHIFT))
    return output


def evaluate(network, own_features, opponent_features):
    """The value of a position, with the integer arithmetic of the engine.

    The own features are every piece but the own king, so the pieces on the board are one more.
    """
    stack = layer_stack_of(len(own_features) + 1)
    head = network.heads[stack]
    inputs = [_clipped_relu(value) for value in _accumulator(network, own_features)]
    inputs += [_clipped_relu(value) for value in _accumulator(network, opponent_features)]
    first_layer = _affine_sqr_relu if network.sqr else _affine_relu
    hidden = first_layer(inputs, head.l1_weight, head.l1_bias, L1_SIZE)
    hidden = _affine_relu(hidden, head.l2_weight, head.l2_bias, L2_SIZE)
    total = head.output_bias
    for index in range(L2_SIZE):
        total += head.output_weight[index] * hidden[index]
    if network.psqt_weight is not None:
        own = sum(network.psqt_weight[f * PSQT_BUCKETS + stack] for f in own_features)
        opponent = sum(network.psqt_weight[f * PSQT_BUCKETS + stack] for f in opponent_features)
        total += _truncating_division(own - opponent, 2)
    return _truncating_division(total * NET_VALUE_SCALE, QA * QB)


# --- the mixture of experts ------------------------------------------------------------------------------------

class MoeNetwork:
    """A mixture-of-experts net, quantized, in the order the file holds it (write_moe)."""

    __slots__ = ('feature_bias', 'feature_weight', 'router_bias', 'router_weight', 'l1_bias', 'l1_weight',
                 'l2_bias', 'l2_weight', 'output_bias', 'output_weight')


def _moe_members():
    width = ACCUMULATOR_SIZE + MOE_ROUTER
    experts = MOE_EXPERTS * MOE_EXPERT_SIZE
    return (
        ('feature_bias', 'h', width),
        ('feature_weight', 'h', FEATURE_COUNT * width),
        ('router_bias', 'i', MOE_EXPERTS),
        ('router_weight', 'h', MOE_EXPERTS * 2 * MOE_ROUTER),
        ('l1_bias', 'i', experts),
        ('l1_weight', 'b', experts * L1_INPUT_SIZE),
        ('l2_bias', 'i', L2_SIZE),
        # expert by expert: the L2_SIZE x MOE_EXPERT_SIZE block of each, row by row
        ('l2_weight', 'b', MOE_EXPERTS * L2_SIZE * MOE_EXPERT_SIZE),
        ('output_bias', 'i', 1),
        ('output_weight', 'b', L2_SIZE),
    )


def write_moe(path, network):
    """MAGIC_MOE, the shape number, then the arrays of _moe_members() in their order, little endian."""
    with open(path, 'wb') as stream:
        stream.write(MAGIC_MOE)
        stream.write(struct.pack('<I', moe_architecture_id()))
        for member, code, count in _moe_members():
            values = getattr(network, member)
            assert len(values) == count and values.typecode == code, member
            values.tofile(stream)


def read_moe(path):
    network = MoeNetwork()
    with open(path, 'rb') as stream:
        magic = stream.read(len(MAGIC_MOE))
        identifier = struct.unpack('<I', stream.read(4))[0]
        if magic != MAGIC_MOE or identifier != moe_architecture_id():
            raise ValueError('%s is not a mixture-of-experts net of this shape' % path)
        for member, code, count in _moe_members():
            setattr(network, member, _read_array(stream, code, count))
    return network


def is_moe(path):
    with open(path, 'rb') as stream:
        return stream.read(len(MAGIC_MOE)) == MAGIC_MOE


def moe_experts_of(network, own_accumulator, opponent_accumulator):
    """The MOE_TOP experts the router picks: highest logit first, the lower index on a tie."""
    inputs = own_accumulator[ACCUMULATOR_SIZE:] + opponent_accumulator[ACCUMULATOR_SIZE:]
    logits = []
    for expert in range(MOE_EXPERTS):
        offset = expert * 2 * MOE_ROUTER
        total = network.router_bias[expert]
        for index in range(2 * MOE_ROUTER):
            total += network.router_weight[offset + index] * inputs[index]
        logits.append(total)
    return sorted(range(MOE_EXPERTS), key=lambda e: (-logits[e], e))[:MOE_TOP]


def evaluate_moe(network, own_features, opponent_features):
    """The value of a position by a mixture-of-experts net, with the integer arithmetic of the engine."""
    width = ACCUMULATOR_SIZE + MOE_ROUTER

    def accumulator(features):
        values = list(network.feature_bias)
        for feature in features:
            offset = feature * width
            for index in range(width):
                values[index] += network.feature_weight[offset + index]
        return values

    own, opponent = accumulator(own_features), accumulator(opponent_features)
    inputs = [_clipped_relu(v) for v in own[:ACCUMULATOR_SIZE]]
    inputs += [_clipped_relu(v) for v in opponent[:ACCUMULATOR_SIZE]]
    sums = list(network.l2_bias)
    for expert in moe_experts_of(network, own, opponent):
        rows = range(expert * MOE_EXPERT_SIZE, (expert + 1) * MOE_EXPERT_SIZE)
        hidden = []
        for row in rows:
            total = network.l1_bias[row]
            offset = row * L1_INPUT_SIZE
            for index in range(L1_INPUT_SIZE):
                total += network.l1_weight[offset + index] * inputs[index]
            hidden.append(_clipped_relu(total >> QB_SHIFT))
        block = expert * L2_SIZE * MOE_EXPERT_SIZE
        for out in range(L2_SIZE):
            for index in range(MOE_EXPERT_SIZE):
                sums[out] += network.l2_weight[block + out * MOE_EXPERT_SIZE + index] * hidden[index]
    second = [_clipped_relu(v >> QB_SHIFT) for v in sums]
    total = network.output_bias[0]
    for index in range(L2_SIZE):
        total += network.output_weight[index] * second[index]
    return _truncating_division(total * NET_VALUE_SCALE, QA * QB)
