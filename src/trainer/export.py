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
    stacks = getattr(model, 'stacks', 1)

    def rows(layer, stack, size):
        # The outputs of a stacked layer are stack after stack, size of them each.
        weight = layer.weight.detach().cpu().numpy()[stack * size:(stack + 1) * size]
        bias = layer.bias.detach().cpu().numpy()[stack * size:(stack + 1) * size]
        return weight, bias

    heads = []
    sqr = getattr(model, 'sqr', False)
    for stack in range(stacks):
        head = netfile.Head(sqr)
        for layer, size, weight_member, bias_member in (
                (model.l1, netfile.L1_SIZE, 'l1_weight', 'l1_bias'),
                (model.l2, netfile.L2_SIZE, 'l2_weight', 'l2_bias')):
            weight, bias = rows(layer, stack, size)
            setattr(head, weight_member, _as_array(_quantized(weight, netfile.QB, np.int8), 'b'))
            setattr(head, bias_member, _as_array(_quantized(bias, scale, np.int32), 'i'))
        weight, bias = rows(model.output, stack, 1)
        head.output_weight = _as_array(_quantized(weight, netfile.QB, np.int8), 'b')
        head.output_bias = int(_quantized(bias, scale, np.int32)[0])
        heads.append(head)
    # A single head goes into every stack - the engine reads it that way from the old format too.
    network.heads = heads if stacks > 1 else heads * netfile.LAYER_STACKS
    network.stacked = stacks > 1
    network.sqr = sqr
    if getattr(model, 'psqt', None) is not None:
        values = model.psqt.weight.detach().cpu().numpy()[:netfile.FEATURE_COUNT]
        network.psqt_weight = _as_array(_quantized(values, scale, np.int32), 'i')
    return network


def quantize_moe(model):
    """A mixture-of-experts model as netfile.write_moe() holds it."""
    network = netfile.MoeNetwork()
    scale = netfile.QA * netfile.QB

    def values(tensor):
        return tensor.detach().cpu().numpy()

    network.feature_weight = _as_array(
        _quantized(values(model.feature_transformer.weight)[:netfile.FEATURE_COUNT], netfile.QA, np.int16), 'h')
    network.feature_bias = _as_array(_quantized(values(model.feature_bias), netfile.QA, np.int16), 'h')
    network.router_weight = _as_array(
        _quantized(values(model.router.weight), netfile.MOE_ROUTER_SCALE, np.int16), 'h')
    network.router_bias = _as_array(
        _quantized(values(model.router.bias), netfile.QA * netfile.MOE_ROUTER_SCALE, np.int32), 'i')
    network.l1_weight = _as_array(_quantized(values(model.l1.weight), netfile.QB, np.int8), 'b')
    network.l1_bias = _as_array(_quantized(values(model.l1.bias), scale, np.int32), 'i')
    # torch keeps the second layer as [out][expert * size + j]; the file holds it expert by expert.
    l2 = values(model.l2.weight).reshape(netfile.L2_SIZE, netfile.MOE_EXPERTS, netfile.MOE_EXPERT_SIZE)
    network.l2_weight = _as_array(_quantized(np.ascontiguousarray(l2.transpose(1, 0, 2)), netfile.QB, np.int8), 'b')
    network.l2_bias = _as_array(_quantized(values(model.l2.bias), scale, np.int32), 'i')
    network.output_weight = _as_array(_quantized(values(model.output.weight), netfile.QB, np.int8), 'b')
    network.output_bias = _as_array(_quantized(values(model.output.bias), scale, np.int32), 'i')
    return network


def quantize_moe_layers(model):
    """A three-layer mixture-of-experts model as netfile.write_moe_layers() holds it."""
    network = netfile.MoeLayersNetwork()
    scale = netfile.QA * netfile.QB

    def values(tensor):
        return tensor.detach().cpu().numpy()

    network.feature_weight = _as_array(
        _quantized(values(model.feature_transformer.weight)[:netfile.FEATURE_COUNT], netfile.QA, np.int16), 'h')
    network.feature_bias = _as_array(_quantized(values(model.feature_bias), netfile.QA, np.int16), 'h')
    network.l1_weight = _as_array(_quantized(values(model.l1.weight), netfile.QB, np.int8), 'b')
    network.l1_bias = _as_array(_quantized(values(model.l1.bias), scale, np.int32), 'i')
    network.l2_weight = _as_array(_quantized(values(model.l2_weight), netfile.QB, np.int8), 'b')
    network.l2_bias = _as_array(_quantized(values(model.l2_bias), scale, np.int32), 'i')
    network.output_weight = _as_array(_quantized(values(model.output_weight), netfile.QB, np.int8), 'b')
    network.output_bias = _as_array(_quantized(values(model.output_bias), scale, np.int32), 'i')
    return network


def export(model, path):
    if getattr(model, 'moe', False) and getattr(model, 'moe_mix', 'sum') == 'layers':
        netfile.write_moe_layers(path, quantize_moe_layers(model))
        print('written %s' % path)
        return
    if getattr(model, 'moe', False):
        netfile.write_moe(path, quantize_moe(model))
        print('written %s' % path)
        return
    network = quantize(model)
    # A net with one head is written in the old format, which every engine binary reads - the ones
    # a tournament runs were built before there were stacks. Only a stacked net needs the new one.
    netfile.write(path, network, single_head=not network.stacked)
    print('written %s' % path)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('usage: export.py <checkpoint> <net file>')
        raise SystemExit(1)
    from model import HalfKaNet
    held = torch.load(sys.argv[1], map_location='cpu', weights_only=False)
    state = held['model'] if isinstance(held, dict) and 'model' in held else held
    if 'l2_weight' in state:
        trained = HalfKaNet(moe=True, moe_mix='layers')
        trained.load_state_dict(state)
        export(trained, sys.argv[2])
        raise SystemExit(0)
    moe = 'router.weight' in state
    trained = HalfKaNet(stacks=1 if moe else state['output.weight'].shape[0], psqt='psqt.weight' in state,
                        sqr=not moe and state['l2.weight'].shape[1] != netfile.L1_SIZE, moe=moe)
    trained.load_state_dict(state)
    export(trained, sys.argv[2])
