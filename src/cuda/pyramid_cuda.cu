// CUDA implementation of Gaussian reduce (5x5 kernel [1 4 6 4 1] separable)

#include <cuda_runtime.h>
#include <stdint.h>

#include "pyramid_cuda.h"

namespace {

__device__ __forceinline__ int wrap_index(int i, int n) {
    i %= n;
    if (i < 0) i += n;
    return i;
}

template <bool WRAP>
__device__ __forceinline__ int clamp_or_wrap(int i, int n) {
    if constexpr (WRAP) {
        return wrap_index(i, n);
    } else {
        return max(0, min(n - 1, i));
    }
}

template <bool WRAP>
__global__ void reduce5x5_f32_kernel(const float* __restrict__ src, int sw, int sh, int sstride,
                                     float* __restrict__ dst, int dw, int dh, int dstride)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x; // dest x
    int y = blockIdx.y * blockDim.y + threadIdx.y; // dest y
    if (x >= dw || y >= dh) return;

    // Corresponding source center at (2x, 2y)
    int cx = 2 * x;
    int cy = 2 * y;

    // Separable 5-tap weights: [1 4 6 4 1]
    // Compute horizontal 1D at rows cy-2..cy+2, then vertical combine
    float tmp[5];
    #pragma unroll
    for (int ky = -2; ky <= 2; ++ky) {
        int sy = clamp_or_wrap<WRAP>(cy + ky, sh);
        const float* row = src + sy * sstride;
        int base = cx;
        float v0 = row[clamp_or_wrap<WRAP>(base - 2, sw)];
        float v1 = row[clamp_or_wrap<WRAP>(base - 1, sw)];
        float v2 = row[clamp_or_wrap<WRAP>(base + 0, sw)];
        float v3 = row[clamp_or_wrap<WRAP>(base + 1, sw)];
        float v4 = row[clamp_or_wrap<WRAP>(base + 2, sw)];
        float h = (v0 + 4.0f * v1 + 6.0f * v2 + 4.0f * v3 + v4);
        tmp[ky + 2] = h; // store unnormalized
    }
    float out = (tmp[0] + 4.0f * tmp[1] + 6.0f * tmp[2] + 4.0f * tmp[3] + tmp[4]) / 256.0f;
    dst[y * dstride + x] = out;
}

template <bool WRAP>
__global__ void reduce5x5_f32_u8alpha_kernel(const float* __restrict__ src, const uint8_t* __restrict__ alpha,
                                             int sw, int sh, int sstride,
                                             float* __restrict__ dst, uint8_t* __restrict__ alpha_out,
                                             int dw, int dh, int dstride)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x; // dest x
    int y = blockIdx.y * blockDim.y + threadIdx.y; // dest y
    if (x >= dw || y >= dh) return;

    int cx = 2 * x;
    int cy = 2 * y;

    float Ih[5] = {0,0,0,0,0};
    float Ah[5] = {0,0,0,0,0};
    #pragma unroll
    for (int ky = -2; ky <= 2; ++ky) {
        int sy = clamp_or_wrap<WRAP>(cy + ky, sh);
        const float* rowI = src + sy * sstride;
        const uint8_t* rowA = alpha + sy * sstride;
        int base = cx;
        float a0 = rowA[clamp_or_wrap<WRAP>(base - 2, sw)] ? 1.0f : 0.0f;
        float a1 = rowA[clamp_or_wrap<WRAP>(base - 1, sw)] ? 1.0f : 0.0f;
        float a2 = rowA[clamp_or_wrap<WRAP>(base + 0, sw)] ? 1.0f : 0.0f;
        float a3 = rowA[clamp_or_wrap<WRAP>(base + 1, sw)] ? 1.0f : 0.0f;
        float a4 = rowA[clamp_or_wrap<WRAP>(base + 2, sw)] ? 1.0f : 0.0f;
        float w0 = (float)(a0);
        float w1 = (float)(a1 * 4.0f);
        float w2 = (float)(a2 * 6.0f);
        float w3 = (float)(a3 * 4.0f);
        float w4 = (float)(a4);
        float V0 = a0 ? rowI[clamp_or_wrap<WRAP>(base - 2, sw)] : 0.0f;
        float V1 = a1 ? rowI[clamp_or_wrap<WRAP>(base - 1, sw)] : 0.0f;
        float V2 = a2 ? rowI[clamp_or_wrap<WRAP>(base + 0, sw)] : 0.0f;
        float V3 = a3 ? rowI[clamp_or_wrap<WRAP>(base + 1, sw)] : 0.0f;
        float V4 = a4 ? rowI[clamp_or_wrap<WRAP>(base + 2, sw)] : 0.0f;
        Ih[ky + 2] = (V0 + 4.0f * V1 + 6.0f * V2 + 4.0f * V3 + V4);
        Ah[ky + 2] = (w0 + w1 + w2 + w3 + w4);
    }
    float Ihv = (Ih[0] + 4.0f * Ih[1] + 6.0f * Ih[2] + 4.0f * Ih[3] + Ih[4]);
    float Ahv = (Ah[0] + 4.0f * Ah[1] + 6.0f * Ah[2] + 4.0f * Ah[3] + Ah[4]);
    if (Ahv > 0.0f) {
        dst[y * dstride + x] = Ihv / Ahv; // normalize by mask sum
        alpha_out[y * dstride + x] = 255;
    } else {
        dst[y * dstride + x] = 0.0f;
        alpha_out[y * dstride + x] = 0;
    }
}

inline bool launch_reduce(const float* src, int sw, int sh, int sstride,
                          float* dst, int dw, int dh, int dstride,
                          bool wrap, cudaStream_t stream)
{
    dim3 block(16, 16);
    dim3 grid((dw + block.x - 1) / block.x, (dh + block.y - 1) / block.y);
    if (wrap) {
        reduce5x5_f32_kernel<true><<<grid, block, 0, stream>>>(src, sw, sh, sstride, dst, dw, dh, dstride);
    } else {
        reduce5x5_f32_kernel<false><<<grid, block, 0, stream>>>(src, sw, sh, sstride, dst, dw, dh, dstride);
    }
    return cudaGetLastError() == cudaSuccess;
}

inline bool launch_reduce_alpha(const float* src, const uint8_t* alpha,
                                int sw, int sh, int sstride,
                                float* dst, uint8_t* alpha_out,
                                int dw, int dh, int dstride,
                                bool wrap, cudaStream_t stream)
{
    dim3 block(16, 16);
    dim3 grid((dw + block.x - 1) / block.x, (dh + block.y - 1) / block.y);
    if (wrap) {
        reduce5x5_f32_u8alpha_kernel<true><<<grid, block, 0, stream>>>(src, alpha, sw, sh, sstride, dst, alpha_out, dw, dh, dstride);
    } else {
        reduce5x5_f32_u8alpha_kernel<false><<<grid, block, 0, stream>>>(src, alpha, sw, sh, sstride, dst, alpha_out, dw, dh, dstride);
    }
    return cudaGetLastError() == cudaSuccess;
}

} // namespace

extern "C" bool enblend_cuda_reduce_f32(const float* src, int src_w, int src_h, int src_stride,
                                         float* dst, int dst_w, int dst_h, int dst_stride,
                                         bool wraparound)
{
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return false;
    // Expect dst_w = ceil(src_w/2), dst_h = ceil(src_h/2)
    cudaStream_t stream = nullptr;
    bool ok = launch_reduce(src, src_w, src_h, src_stride, dst, dst_w, dst_h, dst_stride, wraparound, stream);
    if (!ok) return false;
    return cudaDeviceSynchronize() == cudaSuccess;
}

extern "C" bool enblend_cuda_reduce_f32_u8alpha(const float* src, const uint8_t* alpha,
                                                 int src_w, int src_h, int src_stride,
                                                 float* dst, uint8_t* alpha_out,
                                                 int dst_w, int dst_h, int dst_stride,
                                                 bool wraparound)
{
    if (!src || !dst || !alpha || !alpha_out || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return false;
    cudaStream_t stream = nullptr;
    bool ok = launch_reduce_alpha(src, alpha, src_w, src_h, src_stride, dst, alpha_out, dst_w, dst_h, dst_stride, wraparound, stream);
    if (!ok) return false;
    return cudaDeviceSynchronize() == cudaSuccess;
}
