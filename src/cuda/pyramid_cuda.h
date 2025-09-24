// Minimal C API for CUDA pyramid reduce kernels
#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

// Downsample by 2 using separable 5-tap Gaussian [1 4 6 4 1] in both axes.
// wraparound=true uses periodic boundary conditions; otherwise clamp-to-edge.
// src_stride, dst_stride are number of float elements per row (not bytes).
bool enblend_cuda_reduce_f32(const float* src, int src_w, int src_h, int src_stride,
                             float* dst, int dst_w, int dst_h, int dst_stride,
                             bool wraparound);

// Same as above but with 8-bit alpha mask; alpha_out receives 0 when total alpha sum == 0 else 255.
bool enblend_cuda_reduce_f32_u8alpha(const float* src, const uint8_t* alpha,
                                     int src_w, int src_h, int src_stride,
                                     float* dst, uint8_t* alpha_out,
                                     int dst_w, int dst_h, int dst_stride,
                                     bool wraparound);

}
