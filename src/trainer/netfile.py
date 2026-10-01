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


def layer_stack_of(piece_count):
    """The stack a position is judged by, layerStackOf() in nnue-arch.h."""
    return 0 if piece_count == 0 else min((piece_count - 1) // 4, LAYER_STACKS - 1)


class Head:
    """The dense layers of one layer stack, quantized."""

    __slots__ = ('l1_bias', 'l1_weight', 'l2_bias', 'l2_weight', 'output_bias', 'output_weight')

    def __init__(self):
        self.l1_bias = array('i', [0]) * L1_SIZE
        self.l1_weight = array('b', [0]) * (L1_SIZE * L1_INPUT_SIZE)
        self.l2_bias = array('i', [0]) * L2_SIZE
        self.l2_weight = array('b', [0]) * (L2_SIZE * L1_SIZE)
        self.output_bias = 0
        self.output_weight = array('b', [0]) * L2_SIZE


class Network:
    """The quantized weights, in the order they stand in the file."""

    __slots__ = ('feature_bias', 'feature_weight', 'heads', 'stacked')

    def __init__(self):
        self.feature_bias = array('h', [0]) * ACCUMULATOR_SIZE
        self.feature_weight = array('h', [0]) * (FEATURE_COUNT * ACCUMULATOR_SIZE)
        self.heads = [Head() for _ in range(LAYER_STACKS)]
        self.stacked = True


# The arrays of a head in their order: name, type code and length. The output bias, a single int32,
# stands between l2_weight and output_weight.
_HEAD_MEMBERS = (
    ('l1_bias', 'i', L1_SIZE),
    ('l1_weight', 'b', L1_SIZE * L1_INPUT_SIZE),
    ('l2_bias', 'i', L2_SIZE),
    ('l2_weight', 'b', L2_SIZE * L1_SIZE),
)
assert array('h').itemsize == 2 and array('i').itemsize == 4 and array('b').itemsize == 1


def _read_array(stream, code, count):
    # fromfile appends, so the array has to start out empty.
    target = array(code)
    target.fromfile(stream, count)
    return target


def _read_head(stream):
    head = Head()
    for member, code, count in _HEAD_MEMBERS:
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
        if magic not in (MAGIC, MAGIC_SINGLE_HEAD):
            raise ValueError('%s is not a net file' % path)
        wanted = architecture_id() if magic == MAGIC else single_head_architecture_id()
        if identifier != wanted:
            raise ValueError('%s has shape %d, this code wants %d' % (path, identifier, wanted))
        network.feature_bias = _read_array(stream, 'h', ACCUMULATOR_SIZE)
        network.feature_weight = _read_array(stream, 'h', FEATURE_COUNT * ACCUMULATOR_SIZE)
        if magic == MAGIC:
            network.heads = [_read_head(stream) for _ in range(LAYER_STACKS)]
        else:
            head = _read_head(stream)
            network.heads = [head] * LAYER_STACKS
    return network


def write(path, network, single_head=False):
    """Writes a net file. This is what the exporter of the trainer produces.

    single_head writes the old format with the first head alone - for a net whose heads are all the
    same, which every engine binary can read, also the ones built before there were stacks.
    """
    with open(path, 'wb') as stream:
        stream.write(MAGIC_SINGLE_HEAD if single_head else MAGIC)
        stream.write(struct.pack('<I', single_head_architecture_id() if single_head
                                 else architecture_id()))
        network.feature_bias.tofile(stream)
        network.feature_weight.tofile(stream)
        for head in network.heads[:1] if single_head else network.heads:
            for member, _code, _count in _HEAD_MEMBERS:
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
    head = network.heads[layer_stack_of(len(own_features) + 1)]
    inputs = [_clipped_relu(value) for value in _accumulator(network, own_features)]
    inputs += [_clipped_relu(value) for value in _accumulator(network, opponent_features)]
    hidden = _affine_relu(inputs, head.l1_weight, head.l1_bias, L1_SIZE)
    hidden = _affine_relu(hidden, head.l2_weight, head.l2_bias, L2_SIZE)
    total = head.output_bias
    for index in range(L2_SIZE):
        total += head.output_weight[index] * hidden[index]
    return _truncating_division(total * NET_VALUE_SCALE, QA * QB)
