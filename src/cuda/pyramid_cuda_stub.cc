// CPU fallback for CUDA pyramid reduce API
#include <cstdint>
#include <cstring>

#include "pyramid_cuda.h"

namespace {

template <typename T>
inline T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline int wrap_index(int i, int n) {
    i %= n; if (i < 0) i += n; return i;
}

inline int idx(int x, int w, bool wrap) {
    return wrap ? wrap_index(x, w) : clamp(x, 0, w - 1);
}

} // namespace

extern "C" bool enblend_cuda_reduce_f32(const float* src, int src_w, int src_h, int src_stride,
                                         float* dst, int dst_w, int dst_h, int dst_stride,
                                         bool wraparound)
{
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return false;
    for (int y = 0; y < dst_h; ++y) {
        for (int x = 0; x < dst_w; ++x) {
            int cx = 2 * x;
            int cy = 2 * y;
            float tmp[5];
            for (int ky = -2; ky <= 2; ++ky) {
                int sy = wraparound ? wrap_index(cy + ky, src_h) : clamp(cy + ky, 0, src_h - 1);
                const float* row = src + sy * src_stride;
                float v0 = row[idx(cx - 2, src_w, wraparound)];
                float v1 = row[idx(cx - 1, src_w, wraparound)];
                float v2 = row[idx(cx + 0, src_w, wraparound)];
                float v3 = row[idx(cx + 1, src_w, wraparound)];
                float v4 = row[idx(cx + 2, src_w, wraparound)];
                tmp[ky + 2] = v0 + 4.0f * v1 + 6.0f * v2 + 4.0f * v3 + v4;
            }
            float out = (tmp[0] + 4.0f * tmp[1] + 6.0f * tmp[2] + 4.0f * tmp[3] + tmp[4]) / 256.0f;
            dst[y * dst_stride + x] = out;
        }
    }
    return true;
}

extern "C" bool enblend_cuda_reduce_f32_u8alpha(const float* src, const uint8_t* alpha,
                                                 int src_w, int src_h, int src_stride,
                                                 float* dst, uint8_t* alpha_out,
                                                 int dst_w, int dst_h, int dst_stride,
                                                 bool wraparound)
{
    if (!src || !dst || !alpha || !alpha_out || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return false;
    for (int y = 0; y < dst_h; ++y) {
        for (int x = 0; x < dst_w; ++x) {
            int cx = 2 * x;
            int cy = 2 * y;
            float Ih[5] = {0,0,0,0,0};
            float Ah[5] = {0,0,0,0,0};
            for (int ky = -2; ky <= 2; ++ky) {
                int sy = wraparound ? wrap_index(cy + ky, src_h) : clamp(cy + ky, 0, src_h - 1);
                const float* rowI = src + sy * src_stride;
                const uint8_t* rowA = alpha + sy * src_stride;
                float a0 = rowA[idx(cx - 2, src_w, wraparound)] ? 1.0f : 0.0f;
                float a1 = rowA[idx(cx - 1, src_w, wraparound)] ? 1.0f : 0.0f;
                float a2 = rowA[idx(cx + 0, src_w, wraparound)] ? 1.0f : 0.0f;
                float a3 = rowA[idx(cx + 1, src_w, wraparound)] ? 1.0f : 0.0f;
                float a4 = rowA[idx(cx + 2, src_w, wraparound)] ? 1.0f : 0.0f;
                float w0 = a0;
                float w1 = 4.0f * a1;
                float w2 = 6.0f * a2;
                float w3 = 4.0f * a3;
                float w4 = a4;
                float V0 = a0 ? rowI[idx(cx - 2, src_w, wraparound)] : 0.0f;
                float V1 = a1 ? rowI[idx(cx - 1, src_w, wraparound)] : 0.0f;
                float V2 = a2 ? rowI[idx(cx + 0, src_w, wraparound)] : 0.0f;
                float V3 = a3 ? rowI[idx(cx + 1, src_w, wraparound)] : 0.0f;
                float V4 = a4 ? rowI[idx(cx + 2, src_w, wraparound)] : 0.0f;
                Ih[ky + 2] = (V0 + 4.0f * V1 + 6.0f * V2 + 4.0f * V3 + V4);
                Ah[ky + 2] = (w0 + w1 + w2 + w3 + w4);
            }
            float Ihv = (Ih[0] + 4.0f * Ih[1] + 6.0f * Ih[2] + 4.0f * Ih[3] + Ih[4]);
            float Ahv = (Ah[0] + 4.0f * Ah[1] + 6.0f * Ah[2] + 4.0f * Ah[3] + Ah[4]);
            if (Ahv > 0.0f) {
                dst[y * dst_stride + x] = Ihv / Ahv;
                alpha_out[y * dst_stride + x] = 255;
            } else {
                dst[y * dst_stride + x] = 0.0f;
                alpha_out[y * dst_stride + x] = 0;
            }
        }
    }
    return true;
}
