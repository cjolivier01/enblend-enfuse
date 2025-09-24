#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>

#include "cuda/pyramid_cuda.h"

static void cpu_reduce_ref(const float* src, int sw, int sh, int sstride,
                           float* dst, int dw, int dh, int dstride,
                           bool wrap)
{
    auto wrap_index = [](int i, int n) { i %= n; if (i < 0) i += n; return i; };
    auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    auto idx = [&](int x, int w) { return wrap ? wrap_index(x, w) : clamp(x, 0, w - 1); };
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            int cx = 2 * x;
            int cy = 2 * y;
            float tmp[5];
            for (int ky = -2; ky <= 2; ++ky) {
                int sy = wrap ? wrap_index(cy + ky, sh) : clamp(cy + ky, 0, sh - 1);
                const float* row = src + sy * sstride;
                float v0 = row[idx(cx - 2, sw)];
                float v1 = row[idx(cx - 1, sw)];
                float v2 = row[idx(cx + 0, sw)];
                float v3 = row[idx(cx + 1, sw)];
                float v4 = row[idx(cx + 2, sw)];
                tmp[ky + 2] = v0 + 4.0f * v1 + 6.0f * v2 + 4.0f * v3 + v4;
            }
            float out = (tmp[0] + 4.0f * tmp[1] + 6.0f * tmp[2] + 4.0f * tmp[3] + tmp[4]) / 256.0f;
            dst[y * dstride + x] = out;
        }
    }
}

static void assert_allclose(const std::vector<float>& a, const std::vector<float>& b, float atol, float rtol)
{
    assert(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = std::fabs(a[i] - b[i]);
        float tol = atol + rtol * std::fabs(b[i]);
        if (diff > tol || std::isnan(a[i]) || std::isnan(b[i])) {
            std::cerr << "Mismatch at " << i << ": " << a[i] << " vs " << b[i] << " (diff=" << diff << ")\n";
            std::abort();
        }
    }
}

int main()
{
    const int W = 31; // odd dims stress wrap and clamp boundaries
    const int H = 27;
    const int dW = (W + 1) / 2;
    const int dH = (H + 1) / 2;
    std::vector<float> src(H * W);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            src[y * W + x] = std::sin(0.1f * x) + std::cos(0.07f * y);
        }
    }

    for (bool wrap : {false, true}) {
        std::vector<float> dst_ref(dH * dW, 0.0f);
        std::vector<float> dst_gpu(dH * dW, 0.0f);
        cpu_reduce_ref(src.data(), W, H, W, dst_ref.data(), dW, dH, dW, wrap);
        bool ok = enblend_cuda_reduce_f32(src.data(), W, H, W, dst_gpu.data(), dW, dH, dW, wrap);
        if (!ok) {
            std::cerr << "CUDA reduce call failed (wrap=" << wrap << ")\n";
            return 1;
        }
        assert_allclose(dst_gpu, dst_ref, 1e-4f, 1e-5f);
    }

    std::cout << "pyramid_cuda_test: OK" << std::endl;
    return 0;
}
