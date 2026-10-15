"""The feature transformer on the gpu of a Mac, with two Metal kernels of its own.

torch's EmbeddingBag is the slow part of a training step on MPS: measured on the M4 (2026-10-15) its
backward pass took 11.3 ms a call for a batch of 16384, the forward pass 7.3 ms, two calls a step - more
than half of the step. The backward pass of EmbeddingBag scatters into the weight rows and fights over
them. Here the loader hands over the features of the batch grouped by feature (batcher --csr, a counting
sort on the cpu, where it costs next to nothing), and the backward pass becomes a plain sum: one thread
per weight, over the outputs of the positions that hold its feature - no atomics, every weight written
once. Both perspectives go through one call. Measured for both perspectives together: 18.4 ms against
38.8 ms.

The forward pass is the same sum EmbeddingBag makes, in the same order, and gives the same bits. The
backward pass adds the same terms in another order, so a net trained with it is not bit-identical to
one trained with EmbeddingBag - which the gpu never gave anyway (its atomics have no fixed order).

Only for MPS. The cpu keeps EmbeddingBag, so the reproducibility test is untouched.
"""

import torch

_SOURCE = r'''
#include <metal_stdlib>
using namespace metal;

// out[b][j] = sum of w[f][j] over the features f of row b; the padding feature ends a row.
kernel void ft_forward(device const int* features [[buffer(0)]], device const float* w [[buffer(1)]],
                       device float* out [[buffer(2)]], constant uint& slots [[buffer(3)]],
                       constant uint& width [[buffer(4)]], constant uint& padding [[buffer(5)]],
                       uint tid [[thread_position_in_grid]]) {
    const uint b = tid / width, j = tid % width;
    float sum = 0.0f;
    for (uint k = 0; k < slots; ++k) {
        const uint f = uint(features[b * slots + k]);
        if (f == padding) break;
        sum += w[f * width + j];
    }
    out[tid] = sum;
}

// grad[f][j] = sum of out_grad[r][j] over the rows r that hold feature f (start/rows from the loader).
kernel void ft_backward(device const int* start [[buffer(0)]], device const int* rows [[buffer(1)]],
                        device const float* out_grad [[buffer(2)]], device float* grad [[buffer(3)]],
                        constant uint& width [[buffer(4)]], uint tid [[thread_position_in_grid]]) {
    const uint f = tid / width, j = tid % width;
    float sum = 0.0f;
    for (int i = start[f]; i < start[f + 1]; ++i) sum += out_grad[uint(rows[i]) * width + j];
    grad[tid] = sum;
}
'''

_library = None


def _kernels():
    global _library
    if _library is None:
        _library = torch.mps.compile_shader(_SOURCE)
    return _library


class _FeatureTransform(torch.autograd.Function):
    @staticmethod
    def forward(ctx, features, weight, start, rows):
        lib = _kernels()
        count, slots = features.shape
        width = weight.shape[1]
        out = torch.empty(count, width, device=weight.device, dtype=weight.dtype)
        lib.ft_forward(features, weight, out, slots, width, weight.shape[0] - 1, threads=count * width)
        ctx.save_for_backward(start, rows)
        ctx.shape = weight.shape
        return out

    @staticmethod
    def backward(ctx, out_grad):
        start, rows = ctx.saved_tensors
        lib = _kernels()
        grad = torch.empty(ctx.shape, device=out_grad.device, dtype=out_grad.dtype)
        lib.ft_backward(start, rows, out_grad.contiguous(), grad, ctx.shape[1],
                        threads=ctx.shape[0] * ctx.shape[1])
        return None, grad, None, None


def transform(own, opponent, weight, csr):
    """The sums of both perspectives, own and opponent features (int32, padded with the last row of
    weight), csr = (start, rows) of the batch from the loader. Returns (own sums, opponent sums)."""
    count = own.shape[0]
    both = _FeatureTransform.apply(torch.cat((own, opponent)), weight, csr[0], csr[1])
    return both[:count], both[count:]
