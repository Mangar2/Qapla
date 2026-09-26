"""Turns a trained model into the net file the engine reads.

The quantization is the contract, and it is the one thing that cannot be checked by
the training: every scale here has a counterpart in src/nnue/nnue-arch.h. An
activation is an integer of scale QA, a weight of a dense layer one of scale QB, a
bias one of scale QA*QB - which is what lets the engine shift a layer's sum right
by log2(QB) and get the scale the next activation expects.

After exporting, compare the value of a position with the one the engine computes:

    python3 verify.py <net file>
    printf 'stat\\nnew\\nnnueeval net <net file>\\nquit\\n' | ./build/Release/Qapla

Both numbers have to be equal. They are integers, so equal means equal.
"""

from array import array
import sys

import numpy as np
import torch

import netfile


def _quantized(values, scale, dtype):
    limits = np.iinfo(dtype)
    rounded = np.rint(np.asarray(values, dtype=np.float64) * scale)
    return np.clip(rounded, limits.min, limits.max).astype(dtype)


def _as_array(values, code):
    result = array(code)
    result.frombytes(values.tobytes())
    return result


def quantize(model):
    """The weights of a model as the net file holds them."""
    network = netfile.Network()
    weights = model.feature_transformer.weight.detach().cpu().numpy()[:netfile.FEATURE_COUNT]
    network.feature_weight = _as_array(_quantized(weights, netfile.QA, np.int16), 'h')
    network.feature_bias = _as_array(
        _quantized(model.feature_bias.detach().cpu().numpy(), netfile.QA, np.int16), 'h')

    scale = netfile.QA * netfile.QB
    for layer, weight_member, bias_member in (
            (model.l1, 'l1_weight', 'l1_bias'), (model.l2, 'l2_weight', 'l2_bias')):
        setattr(network, weight_member, _as_array(
            _quantized(layer.weight.detach().cpu().numpy(), netfile.QB, np.int8), 'b'))
        setattr(network, bias_member, _as_array(
            _quantized(layer.bias.detach().cpu().numpy(), scale, np.int32), 'i'))

    network.output_weight = _as_array(
        _quantized(model.output.weight.detach().cpu().numpy(), netfile.QB, np.int8), 'b')
    network.output_bias = int(_quantized(model.output.bias.detach().cpu().numpy(), scale,
                                         np.int32)[0])
    return network


def export(model, path):
    netfile.write(path, quantize(model))
    print('written %s' % path)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('usage: export.py <checkpoint> <net file>')
        raise SystemExit(1)
    from model import HalfKaNet
    trained = HalfKaNet()
    trained.load_state_dict(torch.load(sys.argv[1], map_location='cpu'))
    export(trained, sys.argv[2])
