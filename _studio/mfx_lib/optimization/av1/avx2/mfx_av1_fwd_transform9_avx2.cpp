// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

__pragma(warning(disable:4127))

#include "assert.h"
#include "immintrin.h"
#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_transform9_common_avx2.h"

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3 };

namespace details {
    using namespace AV1PP;

    static inline __m256i mult_round_shift(const __m256i *pin0, const __m256i *pin1,
                                    const __m256i *pmultiplier, const __m256i *prounding, const int shift)
    {
        const __m256i u0 = _mm256_madd_epi16(*pin0, *pmultiplier);
        const __m256i u1 = _mm256_madd_epi16(*pin1, *pmultiplier);
        const __m256i v0 = _mm256_add_epi32(u0, *prounding);
        const __m256i v1 = _mm256_add_epi32(u1, *prounding);
        const __m256i w0 = _mm256_srai_epi32(v0, shift);
        const __m256i w1 = _mm256_srai_epi32(v1, shift);
        return _mm256_packs_epi32(w0, w1);
    }
    /*
    static inline __m256i k_madd_epi32(__m256i a, __m256i b) {
        __m256i buf0, buf1;
        buf0 = _mm256_mul_epu32(a, b);
        a = _mm256_srli_epi64(a, 32);
        b = _mm256_srli_epi64(b, 32);
        buf1 = _mm256_mul_epu32(a, b);
        return _mm256_add_epi64(buf0, buf1);
    }

    static inline __m256i k_packs_epi64(__m256i a, __m256i b) {
        __m256i buf0 = _mm256_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 2, 0));
        __m256i buf1 = _mm256_shuffle_epi32(b, _MM_SHUFFLE(0, 0, 2, 0));
        return _mm256_unpacklo_epi64(buf0, buf1);
    }
    */
    template <bool isFirstPassOf32x32> static inline void transpose_and_output16x16(const __m256i *in, short *out_ptr, int pitch)
    {
        // 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
        // 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
        // 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F
        // 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F
        // 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F
        // 50 51 52 53 54 55 56 57 58 59 5A 5B 5C 5D 5E 5F
        // 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F
        // 70 71 72 73 74 75 76 77 78 79 7A 7B 7C 7D 7E 7F
        // 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F
        // 90 91 92 93 94 95 96 97 98 99 9A 9B 9C 9D 9E 9F
        // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF
        // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF
        // C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF
        // D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF
        // E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF
        // F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF
        const __m256i tr0_0  = _mm256_unpacklo_epi16(in[ 0], in[ 1]);
        const __m256i tr0_1  = _mm256_unpacklo_epi16(in[ 2], in[ 3]);
        const __m256i tr0_2  = _mm256_unpacklo_epi16(in[ 4], in[ 5]);
        const __m256i tr0_3  = _mm256_unpacklo_epi16(in[ 6], in[ 7]);
        const __m256i tr0_4  = _mm256_unpacklo_epi16(in[ 8], in[ 9]);
        const __m256i tr0_5  = _mm256_unpacklo_epi16(in[10], in[11]);
        const __m256i tr0_6  = _mm256_unpacklo_epi16(in[12], in[13]);
        const __m256i tr0_7  = _mm256_unpacklo_epi16(in[14], in[15]);
        const __m256i tr0_8  = _mm256_unpackhi_epi16(in[ 0], in[ 1]);
        const __m256i tr0_9  = _mm256_unpackhi_epi16(in[ 2], in[ 3]);
        const __m256i tr0_10 = _mm256_unpackhi_epi16(in[ 4], in[ 5]);
        const __m256i tr0_11 = _mm256_unpackhi_epi16(in[ 6], in[ 7]);
        const __m256i tr0_12 = _mm256_unpackhi_epi16(in[ 8], in[ 9]);
        const __m256i tr0_13 = _mm256_unpackhi_epi16(in[10], in[11]);
        const __m256i tr0_14 = _mm256_unpackhi_epi16(in[12], in[13]);
        const __m256i tr0_15 = _mm256_unpackhi_epi16(in[14], in[15]);
        // 00 10 01 11 02 12 03 13 08 18 09 19 0A 1A 0B 1B
        // 20 30 21 31 22 32 23 33 28 38 29 39 2A 3A 2B 3B
        // 40 50 41 51 42 52 43 53 48 58 49 59 4A 5A 4B 5B
        // 60 70 61 71 62 72 63 73 68 78 69 79 6A 7A 6B 7B
        // 80 90 81 91 82 92 83 93 88 98 89 99 8A 9A 8B 9B
        // A0 B0 A1 B1 A2 B2 A3 B3 A8 B8 A9 B9 AA BA AB BB
        // C0 D0 C1 D1 C2 D2 C3 D3 C8 D8 C9 D9 CA DA CB DB
        // E0 F0 E1 F1 E2 F2 E3 F3 E8 F8 E9 F9 EA FA EB FB
        // 04 14 05 15 06 16 07 17 0C 1C 0D 1D 0E 1E 0F 1F
        // 24 34 25 35 26 36 27 37 2C 3C 2D 3D 2E 3E 2F 3F
        // 44 54 45 55 46 56 47 57 4C 5C 4D 5D 4E 5E 4F 5F
        // 64 74 65 75 66 76 67 77 6C 7C 6D 7D 6E 7E 6F 7F
        // 84 94 85 95 86 96 87 97 8C 9C 8D 9D 8E 9E 8F 9F
        // A4 B4 A5 B5 A6 B6 A7 B7 AC BC AD BD AE BE AF BF
        // C4 D4 C5 D5 C6 D6 C7 D7 CC DC CD DD CE DE CF DF
        // E4 F4 E5 F5 E6 F6 E7 F7 EC FC ED FD EE FE EF FF
        const __m256i tr1_0  = _mm256_unpacklo_epi32(tr0_0,  tr0_1);
        const __m256i tr1_1  = _mm256_unpacklo_epi32(tr0_2,  tr0_3);
        const __m256i tr1_2  = _mm256_unpacklo_epi32(tr0_4,  tr0_5);
        const __m256i tr1_3  = _mm256_unpacklo_epi32(tr0_6,  tr0_7);
        const __m256i tr1_4  = _mm256_unpacklo_epi32(tr0_8,  tr0_9);
        const __m256i tr1_5  = _mm256_unpacklo_epi32(tr0_10, tr0_11);
        const __m256i tr1_6  = _mm256_unpacklo_epi32(tr0_12, tr0_13);
        const __m256i tr1_7  = _mm256_unpacklo_epi32(tr0_14, tr0_15);
        const __m256i tr1_8  = _mm256_unpackhi_epi32(tr0_0,  tr0_1);
        const __m256i tr1_9  = _mm256_unpackhi_epi32(tr0_2,  tr0_3);
        const __m256i tr1_10 = _mm256_unpackhi_epi32(tr0_4,  tr0_5);
        const __m256i tr1_11 = _mm256_unpackhi_epi32(tr0_6,  tr0_7);
        const __m256i tr1_12 = _mm256_unpackhi_epi32(tr0_8,  tr0_9);
        const __m256i tr1_13 = _mm256_unpackhi_epi32(tr0_10, tr0_11);
        const __m256i tr1_14 = _mm256_unpackhi_epi32(tr0_12, tr0_13);
        const __m256i tr1_15 = _mm256_unpackhi_epi32(tr0_14, tr0_15);
        // 00 10 20 30 01 11 01 11 08 18 28 38 09 19 29 39 
        // 40 50 60 70 41 51 61 71 48 58 68 78 49 59 69 79 
        // 80 90 A0 B0 81 91 A1 B1 88 98 A8 B8 89 99 A9 B9 
        // C0 D0 E0 F0 C1 D1 E1 F1 C8 D8 E8 F8 C9 D9 E9 F9 
        // 04 14 24 34 05 15 25 35 0C 1C 2C 3C 0D 1D 2D 3D 
        // 44 54 64 74 45 55 65 75 4C 5C 6C 7C 4D 5D 6D 7D 
        // 84 94 A4 B4 85 95 A5 B5 8C 9C AC BC 8D 9D AD BD 
        // C4 D4 E4 F4 C5 D5 E5 F5 CC DC EC FC CD DD ED FD 
        // 02 12 22 32 03 13 03 13 0A 1A 2A 3A 0B 1B 2B 3B 
        // 42 52 62 72 43 53 63 73 4A 5A 6A 7A 4B 5B 6B 7B 
        // 82 92 A2 B2 83 93 A3 B3 8A 9A AA BA 8B 9B AB BB  
        // C2 D2 E2 F2 C3 D3 E3 F3 CA DA EA FA CB DB EB FB 
        // 06 16 26 36 07 17 27 37 0E 1E 2E 3E 0F 1F 2F 3F 
        // 46 56 66 76 47 57 67 77 4E 5E 6E 7E 4F 5F 6F 7F 
        // 86 96 A6 B6 87 97 A7 B7 8E 9E AE BE 8F 9F AF BF 
        // C6 D6 E6 F6 C7 D7 E7 F7 CE DE EE FE CF DF EF FF 
        __m256i tr2_0  = _mm256_unpacklo_epi64(tr1_0,  tr1_1);
        __m256i tr2_1  = _mm256_unpacklo_epi64(tr1_2,  tr1_3);
        __m256i tr2_2  = _mm256_unpacklo_epi64(tr1_4,  tr1_5);
        __m256i tr2_3  = _mm256_unpacklo_epi64(tr1_6,  tr1_7);
        __m256i tr2_4  = _mm256_unpacklo_epi64(tr1_8,  tr1_9);
        __m256i tr2_5  = _mm256_unpacklo_epi64(tr1_10, tr1_11);
        __m256i tr2_6  = _mm256_unpacklo_epi64(tr1_12, tr1_13);
        __m256i tr2_7  = _mm256_unpacklo_epi64(tr1_14, tr1_15);
        __m256i tr2_8  = _mm256_unpackhi_epi64(tr1_0,  tr1_1);
        __m256i tr2_9  = _mm256_unpackhi_epi64(tr1_2,  tr1_3);
        __m256i tr2_10 = _mm256_unpackhi_epi64(tr1_4,  tr1_5);
        __m256i tr2_11 = _mm256_unpackhi_epi64(tr1_6,  tr1_7);
        __m256i tr2_12 = _mm256_unpackhi_epi64(tr1_8,  tr1_9);
        __m256i tr2_13 = _mm256_unpackhi_epi64(tr1_10, tr1_11);
        __m256i tr2_14 = _mm256_unpackhi_epi64(tr1_12, tr1_13);
        __m256i tr2_15 = _mm256_unpackhi_epi64(tr1_14, tr1_15);
        // 00 10 20 30 40 50 60 70 08 18 28 38 48 58 68 78
        // 80 90 A0 B0 C0 D0 E0 F0 88 98 A8 B8 C8 D8 E8 F8 
        // 04 14 24 34 44 54 64 74 0C 1C 2C 3C 4C 5C 6C 7C 
        // 84 94 A4 B4 C4 D4 E4 F4 8C 9C AC BC CC DC EC FC 
        // 02 12 22 32 42 52 62 72 0A 1A 2A 3A 4A 5A 6A 7A 
        // 82 92 A2 B2 C2 D2 E2 F2 8A 9A AA BA CA DA EA FA 
        // 06 16 26 36 46 56 66 76 0E 1E 2E 3E 4E 5E 6E 7E 
        // 86 96 A6 B6 C6 D6 E6 F6 8E 9E AE BE CE DE EE FE 
        // 01 11 21 31 41 51 61 71 09 19 29 39 49 59 69 79
        // 81 91 A1 B1 C1 D1 E1 F1 89 99 A9 B9 C9 D9 E9 F9 
        // 05 15 25 35 45 55 65 75 0D 1D 2D 3D 4D 5D 6D 7D 
        // 85 95 A5 B5 C5 D5 E5 F5 8D 9D AD BD CD DD ED FD 
        // 03 13 23 33 43 53 63 73 0B 1B 2B 3B 4B 5B 6B 7B 
        // 83 93 A3 B3 C3 D3 E3 F3 8B 9B AB BB CB DB EB FB 
        // 07 17 27 37 47 57 67 77 0F 1F 2F 3F 4F 5F 6F 7F 
        // 87 97 A7 B7 C7 D7 E7 F7 8F 9F AF BF CF DF EF FF
        if (isFirstPassOf32x32) {
            __m256i kZero = _mm256_setzero_si256();
            __m256i kOne  = _mm256_set1_epi16(1);
            __m256i tr2_0_sign  = _mm256_cmpgt_epi16(tr2_0,  kZero);
            __m256i tr2_1_sign  = _mm256_cmpgt_epi16(tr2_1,  kZero);
            __m256i tr2_2_sign  = _mm256_cmpgt_epi16(tr2_2,  kZero);
            __m256i tr2_3_sign  = _mm256_cmpgt_epi16(tr2_3,  kZero);
            __m256i tr2_4_sign  = _mm256_cmpgt_epi16(tr2_4,  kZero);
            __m256i tr2_5_sign  = _mm256_cmpgt_epi16(tr2_5,  kZero);
            __m256i tr2_6_sign  = _mm256_cmpgt_epi16(tr2_6,  kZero);
            __m256i tr2_7_sign  = _mm256_cmpgt_epi16(tr2_7,  kZero);
            __m256i tr2_8_sign  = _mm256_cmpgt_epi16(tr2_8,  kZero);
            __m256i tr2_9_sign  = _mm256_cmpgt_epi16(tr2_9,  kZero);
            __m256i tr2_10_sign = _mm256_cmpgt_epi16(tr2_10, kZero);
            __m256i tr2_11_sign = _mm256_cmpgt_epi16(tr2_11, kZero);
            __m256i tr2_12_sign = _mm256_cmpgt_epi16(tr2_12, kZero);
            __m256i tr2_13_sign = _mm256_cmpgt_epi16(tr2_13, kZero);
            __m256i tr2_14_sign = _mm256_cmpgt_epi16(tr2_14, kZero);
            __m256i tr2_15_sign = _mm256_cmpgt_epi16(tr2_15, kZero);
            tr2_0  = _mm256_sub_epi16(tr2_0,  tr2_0_sign);
            tr2_1  = _mm256_sub_epi16(tr2_1,  tr2_1_sign);
            tr2_2  = _mm256_sub_epi16(tr2_2,  tr2_2_sign);
            tr2_3  = _mm256_sub_epi16(tr2_3,  tr2_3_sign);
            tr2_4  = _mm256_sub_epi16(tr2_4,  tr2_4_sign);
            tr2_5  = _mm256_sub_epi16(tr2_5,  tr2_5_sign);
            tr2_6  = _mm256_sub_epi16(tr2_6,  tr2_6_sign);
            tr2_7  = _mm256_sub_epi16(tr2_7,  tr2_7_sign);
            tr2_8  = _mm256_sub_epi16(tr2_8,  tr2_8_sign);
            tr2_9  = _mm256_sub_epi16(tr2_9,  tr2_9_sign);
            tr2_10 = _mm256_sub_epi16(tr2_10, tr2_10_sign);
            tr2_11 = _mm256_sub_epi16(tr2_11, tr2_11_sign);
            tr2_12 = _mm256_sub_epi16(tr2_12, tr2_12_sign);
            tr2_13 = _mm256_sub_epi16(tr2_13, tr2_13_sign);
            tr2_14 = _mm256_sub_epi16(tr2_14, tr2_14_sign);
            tr2_15 = _mm256_sub_epi16(tr2_15, tr2_15_sign);
            tr2_0  = _mm256_add_epi16(tr2_0,  kOne);
            tr2_1  = _mm256_add_epi16(tr2_1,  kOne);
            tr2_2  = _mm256_add_epi16(tr2_2,  kOne);
            tr2_3  = _mm256_add_epi16(tr2_3,  kOne);
            tr2_4  = _mm256_add_epi16(tr2_4,  kOne);
            tr2_5  = _mm256_add_epi16(tr2_5,  kOne);
            tr2_6  = _mm256_add_epi16(tr2_6,  kOne);
            tr2_7  = _mm256_add_epi16(tr2_7,  kOne);
            tr2_8  = _mm256_add_epi16(tr2_8,  kOne);
            tr2_9  = _mm256_add_epi16(tr2_9,  kOne);
            tr2_10 = _mm256_add_epi16(tr2_10, kOne);
            tr2_11 = _mm256_add_epi16(tr2_11, kOne);
            tr2_12 = _mm256_add_epi16(tr2_12, kOne);
            tr2_13 = _mm256_add_epi16(tr2_13, kOne);
            tr2_14 = _mm256_add_epi16(tr2_14, kOne);
            tr2_15 = _mm256_add_epi16(tr2_15, kOne);
            tr2_0  = _mm256_srai_epi16(tr2_0,  2);
            tr2_1  = _mm256_srai_epi16(tr2_1,  2);
            tr2_2  = _mm256_srai_epi16(tr2_2,  2);
            tr2_3  = _mm256_srai_epi16(tr2_3,  2);
            tr2_4  = _mm256_srai_epi16(tr2_4,  2);
            tr2_5  = _mm256_srai_epi16(tr2_5,  2);
            tr2_6  = _mm256_srai_epi16(tr2_6,  2);
            tr2_7  = _mm256_srai_epi16(tr2_7,  2);
            tr2_8  = _mm256_srai_epi16(tr2_8,  2);
            tr2_9  = _mm256_srai_epi16(tr2_9,  2);
            tr2_10 = _mm256_srai_epi16(tr2_10, 2);
            tr2_11 = _mm256_srai_epi16(tr2_11, 2);
            tr2_12 = _mm256_srai_epi16(tr2_12, 2);
            tr2_13 = _mm256_srai_epi16(tr2_13, 2);
            tr2_14 = _mm256_srai_epi16(tr2_14, 2);
            tr2_15 = _mm256_srai_epi16(tr2_15, 2);
        }
        storea_si256(out_ptr +  0 * pitch, _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(0,2)));
        storea_si256(out_ptr +  1 * pitch, _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(0,2)));
        storea_si256(out_ptr +  2 * pitch, _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(0,2)));
        storea_si256(out_ptr +  3 * pitch, _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(0,2)));
        storea_si256(out_ptr +  4 * pitch, _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(0,2)));
        storea_si256(out_ptr +  5 * pitch, _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(0,2)));
        storea_si256(out_ptr +  6 * pitch, _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(0,2)));
        storea_si256(out_ptr +  7 * pitch, _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(0,2)));
        storea_si256(out_ptr +  8 * pitch, _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(1,3)));
        storea_si256(out_ptr +  9 * pitch, _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(1,3)));
        storea_si256(out_ptr + 10 * pitch, _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(1,3)));
        storea_si256(out_ptr + 11 * pitch, _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(1,3)));
        storea_si256(out_ptr + 12 * pitch, _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(1,3)));
        storea_si256(out_ptr + 13 * pitch, _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(1,3)));
        storea_si256(out_ptr + 14 * pitch, _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(1,3)));
        storea_si256(out_ptr + 15 * pitch, _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(1,3)));
    }

    static inline void load_buffer_16x16(const short *input, int pitch, __m256i *in)
    {
        for (int i = 0; i < 16; i++)
            in[i] = _mm256_slli_epi16(loada_si256(input +  i * pitch), 2);
    }

    static inline void right_shift_16x16(__m256i *res)
    {
        const __m256i const_rounding = _mm256_set1_epi16(1);
        for (int i = 0; i < 16; i++) {
            const __m256i sign0 = _mm256_srai_epi16(res[i], 15);
            res[i] = _mm256_add_epi16(res[i], const_rounding);
            res[i] = _mm256_sub_epi16(res[i], sign0);
            res[i] = _mm256_srai_epi16(res[i], 2);
        }
    }

    static void write_buffer_16x16(short *dst, int pitch, __m256i *out) {
        for (int i = 0; i < 16; i++)
            storea_si256(dst + i * pitch, out[i]);
    }

    static void fdct8x8_avx2(const short *input, short *output, int stride)
    {
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        // Load input
        __m256i in01 = loada2_m128i(input + 0 * stride, input + 1 * stride);
        __m256i in32 = loada2_m128i(input + 3 * stride, input + 2 * stride);
        __m256i in45 = loada2_m128i(input + 4 * stride, input + 5 * stride);
        __m256i in76 = loada2_m128i(input + 7 * stride, input + 6 * stride);
        // Pre-condition input (shift by two)
        in01 = _mm256_slli_epi16(in01, 2);
        in32 = _mm256_slli_epi16(in32, 2);
        in45 = _mm256_slli_epi16(in45, 2);
        in76 = _mm256_slli_epi16(in76, 2);

        // We do two passes, first the columns, then the rows. The results of the
        // first pass are transposed so that the same column code can be reused. The
        // results of the second pass are also transposed so that the rows (processed
        // as columns) are put back in row positions.
        for (int pass = 0; pass < 2; pass++) {
            // To store results of each pass before the transpose.
            __m256i res04, res15, res26, res37;
            // Add/subtract
            const __m256i q01 = _mm256_add_epi16(in01, in76); // q0 q1
            const __m256i q32 = _mm256_add_epi16(in32, in45); // q2 q3
            const __m256i q45 = _mm256_sub_epi16(in32, in45); // q5 q4
            const __m256i q76 = _mm256_sub_epi16(in01, in76); // q7 q6
            // Work on first four results
            {
                // Add/subtract
                const __m256i r01 = _mm256_add_epi16(q01, q32);
                const __m256i r23 = _mm256_sub_epi16(q01, q32);
                // Interleave to do the multiply by constants which gets us into 32bits
                {
                    const __m256i tmp0 = _mm256_permute4x64_epi64(r01, PERM4x64(0,2,1,3));
                    const __m256i tmp1 = _mm256_bsrli_epi128(tmp0, 8);
                    const __m256i tmp2 = _mm256_permute4x64_epi64(r23, PERM4x64(2,0,3,1));
                    const __m256i tmp3 = _mm256_bsrli_epi128(tmp2, 8);
                    const __m256i t01 = _mm256_unpacklo_epi16(tmp0, tmp1);
                    const __m256i t23 = _mm256_unpacklo_epi16(tmp2, tmp3);
                    const __m256i u01 = _mm256_madd_epi16(t01, k__cospi_p16_p16);
                    const __m256i u23 = _mm256_madd_epi16(t01, k__cospi_p16_m16);
                    const __m256i u45 = _mm256_madd_epi16(t23, k__cospi_p24_p08);
                    const __m256i u67 = _mm256_madd_epi16(t23, k__cospi_m08_p24);
                    // dct_const_round_shift
                    const __m256i v01 = _mm256_add_epi32(u01, k__DCT_CONST_ROUNDING);
                    const __m256i v23 = _mm256_add_epi32(u23, k__DCT_CONST_ROUNDING);
                    const __m256i v45 = _mm256_add_epi32(u45, k__DCT_CONST_ROUNDING);
                    const __m256i v67 = _mm256_add_epi32(u67, k__DCT_CONST_ROUNDING);
                    const __m256i w01 = _mm256_srai_epi32(v01, DCT_CONST_BITS);
                    const __m256i w23 = _mm256_srai_epi32(v23, DCT_CONST_BITS);
                    const __m256i w45 = _mm256_srai_epi32(v45, DCT_CONST_BITS);
                    const __m256i w67 = _mm256_srai_epi32(v67, DCT_CONST_BITS);
                    // Combine
                    res04 = _mm256_packs_epi32(w01, w23);   // r00 ... r03 r40 ... r43 r04 ... r07 r44 ... r47
                    res26 = _mm256_packs_epi32(w45, w67);
                    res04 = _mm256_permute4x64_epi64(res04, PERM4x64(0,2,1,3));
                    res26 = _mm256_permute4x64_epi64(res26, PERM4x64(0,2,1,3));
                }
            }
            // Work on next four results
            {
                // Interleave to do the multiply by constants which gets us into 32bits
                const __m256i tmp00 = _mm256_permute4x64_epi64(q76, PERM4x64(2,2,3,3));
                const __m256i tmp10 = _mm256_permute4x64_epi64(q45, PERM4x64(2,2,3,3));
                const __m256i d0 = _mm256_unpacklo_epi16(tmp00, tmp10);
                const __m256i e01 = _mm256_madd_epi16(d0, k__cospi_p16_m16);
                const __m256i e23 = _mm256_madd_epi16(d0, k__cospi_p16_p16);
                // dct_const_round_shift
                const __m256i f01 = _mm256_add_epi32(e01, k__DCT_CONST_ROUNDING);
                const __m256i f23 = _mm256_add_epi32(e23, k__DCT_CONST_ROUNDING);
                const __m256i s01 = _mm256_srai_epi32(f01, DCT_CONST_BITS);
                const __m256i s23 = _mm256_srai_epi32(f23, DCT_CONST_BITS);
                // Combine
                __m256i r01 = _mm256_packs_epi32(s01, s23);
                r01 = _mm256_permute4x64_epi64(r01, PERM4x64(0,2,1,3));
                {
                    // Add/subtract
                    const __m256i q47 = _mm256_permute2x128_si256(q45, q76, PERM2x128(0,2));
                    const __m256i x03 = _mm256_add_epi16(q47, r01);
                    const __m256i x12 = _mm256_sub_epi16(q47, r01);
                    // Interleave to do the multiply by constants which gets us into 32bits
                    {
                        const __m256i tmp0 = _mm256_permute4x64_epi64(x03, PERM4x64(0,2,1,3));
                        const __m256i tmp1 = _mm256_bsrli_epi128(tmp0, 8);
                        const __m256i tmp2 = _mm256_permute4x64_epi64(x12, PERM4x64(0,2,1,3));
                        const __m256i tmp3 = _mm256_bsrli_epi128(tmp2, 8);
                        const __m256i t01 = _mm256_unpacklo_epi16(tmp0, tmp1);
                        const __m256i t23 = _mm256_unpacklo_epi16(tmp2, tmp3);

                        const __m256i u01 = _mm256_madd_epi16(t01, k__cospi_p28_p04);
                        const __m256i u23 = _mm256_madd_epi16(t01, k__cospi_m04_p28);
                        const __m256i u45 = _mm256_madd_epi16(t23, k__cospi_p12_p20);
                        const __m256i u67 = _mm256_madd_epi16(t23, k__cospi_m20_p12);
                        // dct_const_round_shift
                        const __m256i v01 = _mm256_add_epi32(u01, k__DCT_CONST_ROUNDING);
                        const __m256i v23 = _mm256_add_epi32(u23, k__DCT_CONST_ROUNDING);
                        const __m256i v45 = _mm256_add_epi32(u45, k__DCT_CONST_ROUNDING);
                        const __m256i v67 = _mm256_add_epi32(u67, k__DCT_CONST_ROUNDING);
                        const __m256i w01 = _mm256_srai_epi32(v01, DCT_CONST_BITS);
                        const __m256i w23 = _mm256_srai_epi32(v23, DCT_CONST_BITS);
                        const __m256i w45 = _mm256_srai_epi32(v45, DCT_CONST_BITS);
                        const __m256i w67 = _mm256_srai_epi32(v67, DCT_CONST_BITS);
                        // Combine
                        res15 = _mm256_packs_epi32(w01, w45);
                        res37 = _mm256_packs_epi32(w67, w23);
                        res15 = _mm256_permute4x64_epi64(res15, PERM4x64(0,2,1,3));
                        res37 = _mm256_permute4x64_epi64(res37, PERM4x64(0,2,1,3));
                    }
                }
            }
            // Transpose the 8x8.
            {
                // 00 01 02 03 04 05 06 07 40 41 42 43 44 45 46 47
                // 10 11 12 13 14 15 16 17 50 51 52 53 54 55 56 57
                // 20 21 22 23 24 25 26 27 60 61 62 63 64 65 66 67
                // 30 31 32 33 34 35 36 37 70 71 72 73 74 75 76 77
                const __m256i tr0_0 = _mm256_unpacklo_epi16(res04, res15);
                const __m256i tr0_1 = _mm256_unpacklo_epi16(res26, res37);
                const __m256i tr0_2 = _mm256_unpackhi_epi16(res04, res15);
                const __m256i tr0_3 = _mm256_unpackhi_epi16(res26, res37);
                // 00 10 01 11 02 12 03 13 40 50 41 51 42 52 43 53
                // 20 30 21 31 22 32 23 33 60 70 61 71 62 72 63 73
                // 04 14 05 15 06 16 07 17 44 54 45 55 46 56 47 57
                // 24 34 25 35 26 36 27 37 64 74 65 75 66 76 67 77
                const __m256i tr1_0 = _mm256_unpacklo_epi32(tr0_0, tr0_1);
                const __m256i tr1_1 = _mm256_unpacklo_epi32(tr0_2, tr0_3);
                const __m256i tr1_2 = _mm256_unpackhi_epi32(tr0_0, tr0_1);
                const __m256i tr1_3 = _mm256_unpackhi_epi32(tr0_2, tr0_3);
                // 00 10 20 30 01 11 21 31 40 50 60 70 41 51 61 71
                // 04 14 24 34 05 15 25 35 44 54 64 74 45 55 65 75
                // 02 12 22 32 03 13 23 33 42 52 62 72 43 53 63 73
                // 06 16 26 36 07 17 27 37 46 56 66 76 47 57 67 77
                in01 = _mm256_permute4x64_epi64(tr1_0, PERM4x64(0,2,1,3));
                in32 = _mm256_permute4x64_epi64(tr1_2, PERM4x64(1,3,0,2));
                in45 = _mm256_permute4x64_epi64(tr1_1, PERM4x64(0,2,1,3));
                in76 = _mm256_permute4x64_epi64(tr1_3, PERM4x64(1,3,0,2));
                // 00 10 20 30 40 50 60 70 01 11 21 31 41 51 61 71
                // 02 12 22 32 42 52 62 72 03 13 23 33 43 53 63 73
                // 04 14 24 34 44 54 64 74 05 15 25 35 45 55 65 75
                // 06 16 26 36 46 56 66 76 07 17 27 37 47 57 67 77
            }
        }
        // Post-condition output and store it
        {
            // Post-condition (division by two)
            //    division of two 16 bits signed numbers using shifts
            //    n / 2 = (n - (n >> 15)) >> 1
            const __m256i sign_in01 = _mm256_srai_epi16(in01, 15);
            const __m256i sign_in32 = _mm256_srai_epi16(in32, 15);
            const __m256i sign_in45 = _mm256_srai_epi16(in45, 15);
            const __m256i sign_in76 = _mm256_srai_epi16(in76, 15);
            in01 = _mm256_sub_epi16(in01, sign_in01);
            in32 = _mm256_sub_epi16(in32, sign_in32);
            in45 = _mm256_sub_epi16(in45, sign_in45);
            in76 = _mm256_sub_epi16(in76, sign_in76);
            in01 = _mm256_srai_epi16(in01, 1);
            in32 = _mm256_srai_epi16(in32, 1);
            in45 = _mm256_srai_epi16(in45, 1);
            in76 = _mm256_srai_epi16(in76, 1);
            // store results
            storea_si256(output + 0 * 8, in01);
            storea_si256(output + 2 * 8, _mm256_permute4x64_epi64(in32, PERM4x64(2,3,0,1)));
            storea_si256(output + 4 * 8, in45);
            storea_si256(output + 6 * 8, _mm256_permute4x64_epi64(in76, PERM4x64(2,3,0,1)));
        }
    }

    template <int tx_type> static void fht8x8_avx2(const short *input, short *output, int pitchInput)
    {
        //__m256i in[4];

        if (tx_type == DCT_DCT) {
            fdct8x8_avx2(input, output, pitchInput);
        //} else if (tx_type == ADST_DCT) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fadst8_avx2(in);
        //    fdct8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        //} else if (tx_type == DCT_ADST) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fdct8_avx2(in);
        //    fadst8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        //} else if (tx_type == ADST_ADST) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fadst8_avx2(in);
        //    fadst8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        } else {
            assert(0);
        }
    }

    static void fdct16x16_avx2(const short *input, short *output, int stride)
    {
        alignas(32) short intermediate[256];

        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);

        short *out = intermediate;
        for (int pass = 0; pass < 2; ++pass) {
            __m256i in00, in01, in02, in03, in04, in05, in06, in07;
            __m256i in08, in09, in10, in11, in12, in13, in14, in15;
            __m256i input0, input1, input2, input3, input4, input5, input6, input7;
            __m256i step1_0, step1_1, step1_2, step1_3;
            __m256i step1_4, step1_5, step1_6, step1_7;
            __m256i step2_1, step2_2, step2_3, step2_4, step2_5, step2_6;
            __m256i step3_0, step3_1, step3_2, step3_3;
            __m256i step3_4, step3_5, step3_6, step3_7;
            __m256i res[16];
            if (0 == pass) {
                in00 = loada_si256(input + 0 * stride);
                in01 = loada_si256(input + 1 * stride);
                in02 = loada_si256(input + 2 * stride);
                in03 = loada_si256(input + 3 * stride);
                in04 = loada_si256(input + 4 * stride);
                in05 = loada_si256(input + 5 * stride);
                in06 = loada_si256(input + 6 * stride);
                in07 = loada_si256(input + 7 * stride);
                in08 = loada_si256(input + 8 * stride);
                in09 = loada_si256(input + 9 * stride);
                in10 = loada_si256(input + 10 * stride);
                in11 = loada_si256(input + 11 * stride);
                in12 = loada_si256(input + 12 * stride);
                in13 = loada_si256(input + 13 * stride);
                in14 = loada_si256(input + 14 * stride);
                in15 = loada_si256(input + 15 * stride);
                // x = x << 2
                in00 = _mm256_slli_epi16(in00, 2);
                in01 = _mm256_slli_epi16(in01, 2);
                in02 = _mm256_slli_epi16(in02, 2);
                in03 = _mm256_slli_epi16(in03, 2);
                in04 = _mm256_slli_epi16(in04, 2);
                in05 = _mm256_slli_epi16(in05, 2);
                in06 = _mm256_slli_epi16(in06, 2);
                in07 = _mm256_slli_epi16(in07, 2);
                in08 = _mm256_slli_epi16(in08, 2);
                in09 = _mm256_slli_epi16(in09, 2);
                in10 = _mm256_slli_epi16(in10, 2);
                in11 = _mm256_slli_epi16(in11, 2);
                in12 = _mm256_slli_epi16(in12, 2);
                in13 = _mm256_slli_epi16(in13, 2);
                in14 = _mm256_slli_epi16(in14, 2);
                in15 = _mm256_slli_epi16(in15, 2);
            } else {
                in00 = loada_si256(intermediate + 0 * 16);
                in01 = loada_si256(intermediate + 1 * 16);
                in02 = loada_si256(intermediate + 2 * 16);
                in03 = loada_si256(intermediate + 3 * 16);
                in04 = loada_si256(intermediate + 4 * 16);
                in05 = loada_si256(intermediate + 5 * 16);
                in06 = loada_si256(intermediate + 6 * 16);
                in07 = loada_si256(intermediate + 7 * 16);
                in08 = loada_si256(intermediate + 8 * 16);
                in09 = loada_si256(intermediate + 9 * 16);
                in10 = loada_si256(intermediate + 10 * 16);
                in11 = loada_si256(intermediate + 11 * 16);
                in12 = loada_si256(intermediate + 12 * 16);
                in13 = loada_si256(intermediate + 13 * 16);
                in14 = loada_si256(intermediate + 14 * 16);
                in15 = loada_si256(intermediate + 15 * 16);
                // x = (x + 1) >> 2
                __m256i one = _mm256_set1_epi16(1);
                in00 = _mm256_add_epi16(in00, one);
                in01 = _mm256_add_epi16(in01, one);
                in02 = _mm256_add_epi16(in02, one);
                in03 = _mm256_add_epi16(in03, one);
                in04 = _mm256_add_epi16(in04, one);
                in05 = _mm256_add_epi16(in05, one);
                in06 = _mm256_add_epi16(in06, one);
                in07 = _mm256_add_epi16(in07, one);
                in08 = _mm256_add_epi16(in08, one);
                in09 = _mm256_add_epi16(in09, one);
                in10 = _mm256_add_epi16(in10, one);
                in11 = _mm256_add_epi16(in11, one);
                in12 = _mm256_add_epi16(in12, one);
                in13 = _mm256_add_epi16(in13, one);
                in14 = _mm256_add_epi16(in14, one);
                in15 = _mm256_add_epi16(in15, one);
                in00 = _mm256_srai_epi16(in00, 2);
                in01 = _mm256_srai_epi16(in01, 2);
                in02 = _mm256_srai_epi16(in02, 2);
                in03 = _mm256_srai_epi16(in03, 2);
                in04 = _mm256_srai_epi16(in04, 2);
                in05 = _mm256_srai_epi16(in05, 2);
                in06 = _mm256_srai_epi16(in06, 2);
                in07 = _mm256_srai_epi16(in07, 2);
                in08 = _mm256_srai_epi16(in08, 2);
                in09 = _mm256_srai_epi16(in09, 2);
                in10 = _mm256_srai_epi16(in10, 2);
                in11 = _mm256_srai_epi16(in11, 2);
                in12 = _mm256_srai_epi16(in12, 2);
                in13 = _mm256_srai_epi16(in13, 2);
                in14 = _mm256_srai_epi16(in14, 2);
                in15 = _mm256_srai_epi16(in15, 2);
            }
            // Calculate input for the first 8 results.
            {
                input0 = _mm256_add_epi16(in00, in15);
                input1 = _mm256_add_epi16(in01, in14);
                input2 = _mm256_add_epi16(in02, in13);
                input3 = _mm256_add_epi16(in03, in12);
                input4 = _mm256_add_epi16(in04, in11);
                input5 = _mm256_add_epi16(in05, in10);
                input6 = _mm256_add_epi16(in06, in09);
                input7 = _mm256_add_epi16(in07, in08);
            }
            // Calculate input for the next 8 results.
            {
                step1_0 = _mm256_sub_epi16(in07, in08);
                step1_1 = _mm256_sub_epi16(in06, in09);
                step1_2 = _mm256_sub_epi16(in05, in10);
                step1_3 = _mm256_sub_epi16(in04, in11);
                step1_4 = _mm256_sub_epi16(in03, in12);
                step1_5 = _mm256_sub_epi16(in02, in13);
                step1_6 = _mm256_sub_epi16(in01, in14);
                step1_7 = _mm256_sub_epi16(in00, in15);
            }
            // Work on the first eight values; fdct8(input, even_results);
            {
                // Add/subtract
                const __m256i q0 = _mm256_add_epi16(input0, input7);
                const __m256i q1 = _mm256_add_epi16(input1, input6);
                const __m256i q2 = _mm256_add_epi16(input2, input5);
                const __m256i q3 = _mm256_add_epi16(input3, input4);
                const __m256i q4 = _mm256_sub_epi16(input3, input4);
                const __m256i q5 = _mm256_sub_epi16(input2, input5);
                const __m256i q6 = _mm256_sub_epi16(input1, input6);
                const __m256i q7 = _mm256_sub_epi16(input0, input7);
                // Work on first four results
                {
                    // Add/subtract
                    const __m256i r0 = _mm256_add_epi16(q0, q3);
                    const __m256i r1 = _mm256_add_epi16(q1, q2);
                    const __m256i r2 = _mm256_sub_epi16(q1, q2);
                    const __m256i r3 = _mm256_sub_epi16(q0, q3);
                    // Interleave to do the multiply by constants which gets us
                    // into 32 bits.
                    {
                        const __m256i t0 = _mm256_unpacklo_epi16(r0, r1);
                        const __m256i t1 = _mm256_unpackhi_epi16(r0, r1);
                        const __m256i t2 = _mm256_unpacklo_epi16(r2, r3);
                        const __m256i t3 = _mm256_unpackhi_epi16(r2, r3);
                        res[ 0] = mult_round_shift(&t0, &t1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[ 8] = mult_round_shift(&t0, &t1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[ 4] = mult_round_shift(&t2, &t3, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[12] = mult_round_shift(&t2, &t3, &k__cospi_m08_p24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    }
                }
                // Work on next four results
                {
                    // Interleave to do the multiply by constants which gets us
                    // into 32 bits.
                    const __m256i d0 = _mm256_unpacklo_epi16(q6, q5);
                    const __m256i d1 = _mm256_unpackhi_epi16(q6, q5);
                    const __m256i r0 = mult_round_shift(&d0, &d1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    const __m256i r1 = mult_round_shift(&d0, &d1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    {
                        // Add/subtract
                        const __m256i x0 = _mm256_add_epi16(q4, r0);
                        const __m256i x1 = _mm256_sub_epi16(q4, r0);
                        const __m256i x2 = _mm256_sub_epi16(q7, r1);
                        const __m256i x3 = _mm256_add_epi16(q7, r1);
                        // Interleave to do the multiply by constants which gets us
                        // into 32 bits.
                        {
                            const __m256i t0 = _mm256_unpacklo_epi16(x0, x3);
                            const __m256i t1 = _mm256_unpackhi_epi16(x0, x3);
                            const __m256i t2 = _mm256_unpacklo_epi16(x1, x2);
                            const __m256i t3 = _mm256_unpackhi_epi16(x1, x2);
                            res[ 2] = mult_round_shift(&t0, &t1, &k__cospi_p28_p04, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[14] = mult_round_shift(&t0, &t1, &k__cospi_m04_p28, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[10] = mult_round_shift(&t2, &t3, &k__cospi_p12_p20, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[ 6] = mult_round_shift(&t2, &t3, &k__cospi_m20_p12, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        }
                    }
                }
            }
            // Work on the next eight values; step1 -> odd_results
            {
                // step 2
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_5, step1_2);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_5, step1_2);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_4, step1_3);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_4, step1_3);
                    step2_2 = mult_round_shift(&t0, &t1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_3 = mult_round_shift(&t2, &t3, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_5 = mult_round_shift(&t0, &t1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_4 = mult_round_shift(&t2, &t3, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                // step 3
                {
                    step3_0 = _mm256_add_epi16(step1_0, step2_3);
                    step3_1 = _mm256_add_epi16(step1_1, step2_2);
                    step3_2 = _mm256_sub_epi16(step1_1, step2_2);
                    step3_3 = _mm256_sub_epi16(step1_0, step2_3);
                    step3_4 = _mm256_sub_epi16(step1_7, step2_4);
                    step3_5 = _mm256_sub_epi16(step1_6, step2_5);
                    step3_6 = _mm256_add_epi16(step1_6, step2_5);
                    step3_7 = _mm256_add_epi16(step1_7, step2_4);
                }
                // step 4
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step3_1, step3_6);
                    const __m256i t1 = _mm256_unpackhi_epi16(step3_1, step3_6);
                    const __m256i t2 = _mm256_unpacklo_epi16(step3_2, step3_5);
                    const __m256i t3 = _mm256_unpackhi_epi16(step3_2, step3_5);
                    step2_1 = mult_round_shift(&t0, &t1, &k__cospi_m08_p24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_2 = mult_round_shift(&t2, &t3, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_6 = mult_round_shift(&t0, &t1, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_5 = mult_round_shift(&t2, &t3, &k__cospi_p08_m24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                // step 5
                {
                    step1_0 = _mm256_add_epi16(step3_0, step2_1);
                    step1_1 = _mm256_sub_epi16(step3_0, step2_1);
                    step1_2 = _mm256_add_epi16(step3_3, step2_2);
                    step1_3 = _mm256_sub_epi16(step3_3, step2_2);
                    step1_4 = _mm256_sub_epi16(step3_4, step2_5);
                    step1_5 = _mm256_add_epi16(step3_4, step2_5);
                    step1_6 = _mm256_sub_epi16(step3_7, step2_6);
                    step1_7 = _mm256_add_epi16(step3_7, step2_6);
                }
                // step 6
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_0, step1_7);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_0, step1_7);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_1, step1_6);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_1, step1_6);
                    res[ 1] = mult_round_shift(&t0, &t1, &k__cospi_p30_p02, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 9] = mult_round_shift(&t2, &t3, &k__cospi_p14_p18, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[15] = mult_round_shift(&t0, &t1, &k__cospi_m02_p30, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 7] = mult_round_shift(&t2, &t3, &k__cospi_m18_p14, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_2, step1_5);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_2, step1_5);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_3, step1_4);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_3, step1_4);
                    res[ 5] = mult_round_shift(&t0, &t1, &k__cospi_p22_p10, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[13] = mult_round_shift(&t2, &t3, &k__cospi_p06_p26, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[11] = mult_round_shift(&t0, &t1, &k__cospi_m10_p22, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 3] = mult_round_shift(&t2, &t3, &k__cospi_m26_p06, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
            }
            // Transpose the results, do it as two 8x8 transposes.
            transpose_and_output16x16<false>(res, out, 16);
            out = output;
        }
    }

    static void fadst16(__m256i *in) {
        // perform 16x16 1-D ADST for 8 columns
        __m256i s[16], x[16], u[32], v[32];
        const __m256i k__cospi_p01_p31 = pair_set_epi16(cospi_1_64, cospi_31_64);
        const __m256i k__cospi_p31_m01 = pair_set_epi16(cospi_31_64, -cospi_1_64);
        const __m256i k__cospi_p05_p27 = pair_set_epi16(cospi_5_64, cospi_27_64);
        const __m256i k__cospi_p27_m05 = pair_set_epi16(cospi_27_64, -cospi_5_64);
        const __m256i k__cospi_p09_p23 = pair_set_epi16(cospi_9_64, cospi_23_64);
        const __m256i k__cospi_p23_m09 = pair_set_epi16(cospi_23_64, -cospi_9_64);
        const __m256i k__cospi_p13_p19 = pair_set_epi16(cospi_13_64, cospi_19_64);
        const __m256i k__cospi_p19_m13 = pair_set_epi16(cospi_19_64, -cospi_13_64);
        const __m256i k__cospi_p17_p15 = pair_set_epi16(cospi_17_64, cospi_15_64);
        const __m256i k__cospi_p15_m17 = pair_set_epi16(cospi_15_64, -cospi_17_64);
        const __m256i k__cospi_p21_p11 = pair_set_epi16(cospi_21_64, cospi_11_64);
        const __m256i k__cospi_p11_m21 = pair_set_epi16(cospi_11_64, -cospi_21_64);
        const __m256i k__cospi_p25_p07 = pair_set_epi16(cospi_25_64, cospi_7_64);
        const __m256i k__cospi_p07_m25 = pair_set_epi16(cospi_7_64, -cospi_25_64);
        const __m256i k__cospi_p29_p03 = pair_set_epi16(cospi_29_64, cospi_3_64);
        const __m256i k__cospi_p03_m29 = pair_set_epi16(cospi_3_64, -cospi_29_64);
        const __m256i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
        const __m256i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
        const __m256i k__cospi_m28_p04 = pair_set_epi16(-cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m12_p20 = pair_set_epi16(-cospi_12_64, cospi_20_64);
        const __m256i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
        const __m256i k__cospi_m16_m16 = _mm256_set1_epi16(-cospi_16_64);
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        const __m256i kZero = _mm256_set1_epi16(0);

        u[0] = _mm256_unpacklo_epi16(in[15], in[0]);
        u[1] = _mm256_unpackhi_epi16(in[15], in[0]);
        u[2] = _mm256_unpacklo_epi16(in[13], in[2]);
        u[3] = _mm256_unpackhi_epi16(in[13], in[2]);
        u[4] = _mm256_unpacklo_epi16(in[11], in[4]);
        u[5] = _mm256_unpackhi_epi16(in[11], in[4]);
        u[6] = _mm256_unpacklo_epi16(in[9], in[6]);
        u[7] = _mm256_unpackhi_epi16(in[9], in[6]);
        u[8] = _mm256_unpacklo_epi16(in[7], in[8]);
        u[9] = _mm256_unpackhi_epi16(in[7], in[8]);
        u[10] = _mm256_unpacklo_epi16(in[5], in[10]);
        u[11] = _mm256_unpackhi_epi16(in[5], in[10]);
        u[12] = _mm256_unpacklo_epi16(in[3], in[12]);
        u[13] = _mm256_unpackhi_epi16(in[3], in[12]);
        u[14] = _mm256_unpacklo_epi16(in[1], in[14]);
        u[15] = _mm256_unpackhi_epi16(in[1], in[14]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p01_p31);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p01_p31);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p31_m01);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p31_m01);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p05_p27);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p05_p27);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p27_m05);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p27_m05);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p09_p23);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p09_p23);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p23_m09);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p23_m09);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_p13_p19);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_p13_p19);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p19_m13);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p19_m13);
        v[16] = _mm256_madd_epi16(u[8], k__cospi_p17_p15);
        v[17] = _mm256_madd_epi16(u[9], k__cospi_p17_p15);
        v[18] = _mm256_madd_epi16(u[8], k__cospi_p15_m17);
        v[19] = _mm256_madd_epi16(u[9], k__cospi_p15_m17);
        v[20] = _mm256_madd_epi16(u[10], k__cospi_p21_p11);
        v[21] = _mm256_madd_epi16(u[11], k__cospi_p21_p11);
        v[22] = _mm256_madd_epi16(u[10], k__cospi_p11_m21);
        v[23] = _mm256_madd_epi16(u[11], k__cospi_p11_m21);
        v[24] = _mm256_madd_epi16(u[12], k__cospi_p25_p07);
        v[25] = _mm256_madd_epi16(u[13], k__cospi_p25_p07);
        v[26] = _mm256_madd_epi16(u[12], k__cospi_p07_m25);
        v[27] = _mm256_madd_epi16(u[13], k__cospi_p07_m25);
        v[28] = _mm256_madd_epi16(u[14], k__cospi_p29_p03);
        v[29] = _mm256_madd_epi16(u[15], k__cospi_p29_p03);
        v[30] = _mm256_madd_epi16(u[14], k__cospi_p03_m29);
        v[31] = _mm256_madd_epi16(u[15], k__cospi_p03_m29);

        u[0] = _mm256_add_epi32(v[0], v[16]);
        u[1] = _mm256_add_epi32(v[1], v[17]);
        u[2] = _mm256_add_epi32(v[2], v[18]);
        u[3] = _mm256_add_epi32(v[3], v[19]);
        u[4] = _mm256_add_epi32(v[4], v[20]);
        u[5] = _mm256_add_epi32(v[5], v[21]);
        u[6] = _mm256_add_epi32(v[6], v[22]);
        u[7] = _mm256_add_epi32(v[7], v[23]);
        u[8] = _mm256_add_epi32(v[8], v[24]);
        u[9] = _mm256_add_epi32(v[9], v[25]);
        u[10] = _mm256_add_epi32(v[10], v[26]);
        u[11] = _mm256_add_epi32(v[11], v[27]);
        u[12] = _mm256_add_epi32(v[12], v[28]);
        u[13] = _mm256_add_epi32(v[13], v[29]);
        u[14] = _mm256_add_epi32(v[14], v[30]);
        u[15] = _mm256_add_epi32(v[15], v[31]);
        u[16] = _mm256_sub_epi32(v[0], v[16]);
        u[17] = _mm256_sub_epi32(v[1], v[17]);
        u[18] = _mm256_sub_epi32(v[2], v[18]);
        u[19] = _mm256_sub_epi32(v[3], v[19]);
        u[20] = _mm256_sub_epi32(v[4], v[20]);
        u[21] = _mm256_sub_epi32(v[5], v[21]);
        u[22] = _mm256_sub_epi32(v[6], v[22]);
        u[23] = _mm256_sub_epi32(v[7], v[23]);
        u[24] = _mm256_sub_epi32(v[8], v[24]);
        u[25] = _mm256_sub_epi32(v[9], v[25]);
        u[26] = _mm256_sub_epi32(v[10], v[26]);
        u[27] = _mm256_sub_epi32(v[11], v[27]);
        u[28] = _mm256_sub_epi32(v[12], v[28]);
        u[29] = _mm256_sub_epi32(v[13], v[29]);
        u[30] = _mm256_sub_epi32(v[14], v[30]);
        u[31] = _mm256_sub_epi32(v[15], v[31]);

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);
        v[16] = _mm256_add_epi32(u[16], k__DCT_CONST_ROUNDING);
        v[17] = _mm256_add_epi32(u[17], k__DCT_CONST_ROUNDING);
        v[18] = _mm256_add_epi32(u[18], k__DCT_CONST_ROUNDING);
        v[19] = _mm256_add_epi32(u[19], k__DCT_CONST_ROUNDING);
        v[20] = _mm256_add_epi32(u[20], k__DCT_CONST_ROUNDING);
        v[21] = _mm256_add_epi32(u[21], k__DCT_CONST_ROUNDING);
        v[22] = _mm256_add_epi32(u[22], k__DCT_CONST_ROUNDING);
        v[23] = _mm256_add_epi32(u[23], k__DCT_CONST_ROUNDING);
        v[24] = _mm256_add_epi32(u[24], k__DCT_CONST_ROUNDING);
        v[25] = _mm256_add_epi32(u[25], k__DCT_CONST_ROUNDING);
        v[26] = _mm256_add_epi32(u[26], k__DCT_CONST_ROUNDING);
        v[27] = _mm256_add_epi32(u[27], k__DCT_CONST_ROUNDING);
        v[28] = _mm256_add_epi32(u[28], k__DCT_CONST_ROUNDING);
        v[29] = _mm256_add_epi32(u[29], k__DCT_CONST_ROUNDING);
        v[30] = _mm256_add_epi32(u[30], k__DCT_CONST_ROUNDING);
        v[31] = _mm256_add_epi32(u[31], k__DCT_CONST_ROUNDING);

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
        u[16] = _mm256_srai_epi32(v[16], DCT_CONST_BITS);
        u[17] = _mm256_srai_epi32(v[17], DCT_CONST_BITS);
        u[18] = _mm256_srai_epi32(v[18], DCT_CONST_BITS);
        u[19] = _mm256_srai_epi32(v[19], DCT_CONST_BITS);
        u[20] = _mm256_srai_epi32(v[20], DCT_CONST_BITS);
        u[21] = _mm256_srai_epi32(v[21], DCT_CONST_BITS);
        u[22] = _mm256_srai_epi32(v[22], DCT_CONST_BITS);
        u[23] = _mm256_srai_epi32(v[23], DCT_CONST_BITS);
        u[24] = _mm256_srai_epi32(v[24], DCT_CONST_BITS);
        u[25] = _mm256_srai_epi32(v[25], DCT_CONST_BITS);
        u[26] = _mm256_srai_epi32(v[26], DCT_CONST_BITS);
        u[27] = _mm256_srai_epi32(v[27], DCT_CONST_BITS);
        u[28] = _mm256_srai_epi32(v[28], DCT_CONST_BITS);
        u[29] = _mm256_srai_epi32(v[29], DCT_CONST_BITS);
        u[30] = _mm256_srai_epi32(v[30], DCT_CONST_BITS);
        u[31] = _mm256_srai_epi32(v[31], DCT_CONST_BITS);

        s[0] = _mm256_packs_epi32(u[0], u[1]);
        s[1] = _mm256_packs_epi32(u[2], u[3]);
        s[2] = _mm256_packs_epi32(u[4], u[5]);
        s[3] = _mm256_packs_epi32(u[6], u[7]);
        s[4] = _mm256_packs_epi32(u[8], u[9]);
        s[5] = _mm256_packs_epi32(u[10], u[11]);
        s[6] = _mm256_packs_epi32(u[12], u[13]);
        s[7] = _mm256_packs_epi32(u[14], u[15]);
        s[8] = _mm256_packs_epi32(u[16], u[17]);
        s[9] = _mm256_packs_epi32(u[18], u[19]);
        s[10] = _mm256_packs_epi32(u[20], u[21]);
        s[11] = _mm256_packs_epi32(u[22], u[23]);
        s[12] = _mm256_packs_epi32(u[24], u[25]);
        s[13] = _mm256_packs_epi32(u[26], u[27]);
        s[14] = _mm256_packs_epi32(u[28], u[29]);
        s[15] = _mm256_packs_epi32(u[30], u[31]);

        // stage 2
        u[0] = _mm256_unpacklo_epi16(s[8], s[9]);
        u[1] = _mm256_unpackhi_epi16(s[8], s[9]);
        u[2] = _mm256_unpacklo_epi16(s[10], s[11]);
        u[3] = _mm256_unpackhi_epi16(s[10], s[11]);
        u[4] = _mm256_unpacklo_epi16(s[12], s[13]);
        u[5] = _mm256_unpackhi_epi16(s[12], s[13]);
        u[6] = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm256_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p04_p28);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p04_p28);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p28_m04);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p28_m04);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p20_p12);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p20_p12);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p12_m20);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p12_m20);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_m28_p04);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_m28_p04);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p04_p28);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p04_p28);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m12_p20);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m12_p20);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p20_p12);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p20_p12);

        u[0] = _mm256_add_epi32(v[0], v[8]);
        u[1] = _mm256_add_epi32(v[1], v[9]);
        u[2] = _mm256_add_epi32(v[2], v[10]);
        u[3] = _mm256_add_epi32(v[3], v[11]);
        u[4] = _mm256_add_epi32(v[4], v[12]);
        u[5] = _mm256_add_epi32(v[5], v[13]);
        u[6] = _mm256_add_epi32(v[6], v[14]);
        u[7] = _mm256_add_epi32(v[7], v[15]);
        u[8] = _mm256_sub_epi32(v[0], v[8]);
        u[9] = _mm256_sub_epi32(v[1], v[9]);
        u[10] = _mm256_sub_epi32(v[2], v[10]);
        u[11] = _mm256_sub_epi32(v[3], v[11]);
        u[12] = _mm256_sub_epi32(v[4], v[12]);
        u[13] = _mm256_sub_epi32(v[5], v[13]);
        u[14] = _mm256_sub_epi32(v[6], v[14]);
        u[15] = _mm256_sub_epi32(v[7], v[15]);

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

        x[0] = _mm256_add_epi16(s[0], s[4]);
        x[1] = _mm256_add_epi16(s[1], s[5]);
        x[2] = _mm256_add_epi16(s[2], s[6]);
        x[3] = _mm256_add_epi16(s[3], s[7]);
        x[4] = _mm256_sub_epi16(s[0], s[4]);
        x[5] = _mm256_sub_epi16(s[1], s[5]);
        x[6] = _mm256_sub_epi16(s[2], s[6]);
        x[7] = _mm256_sub_epi16(s[3], s[7]);
        x[8] = _mm256_packs_epi32(u[0], u[1]);
        x[9] = _mm256_packs_epi32(u[2], u[3]);
        x[10] = _mm256_packs_epi32(u[4], u[5]);
        x[11] = _mm256_packs_epi32(u[6], u[7]);
        x[12] = _mm256_packs_epi32(u[8], u[9]);
        x[13] = _mm256_packs_epi32(u[10], u[11]);
        x[14] = _mm256_packs_epi32(u[12], u[13]);
        x[15] = _mm256_packs_epi32(u[14], u[15]);

        // stage 3
        u[0] = _mm256_unpacklo_epi16(x[4], x[5]);
        u[1] = _mm256_unpackhi_epi16(x[4], x[5]);
        u[2] = _mm256_unpacklo_epi16(x[6], x[7]);
        u[3] = _mm256_unpackhi_epi16(x[6], x[7]);
        u[4] = _mm256_unpacklo_epi16(x[12], x[13]);
        u[5] = _mm256_unpackhi_epi16(x[12], x[13]);
        u[6] = _mm256_unpacklo_epi16(x[14], x[15]);
        u[7] = _mm256_unpackhi_epi16(x[14], x[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p08_p24);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p08_p24);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p24_m08);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p24_m08);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_m24_p08);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_m24_p08);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p08_p24);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p08_p24);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p08_p24);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p08_p24);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p24_m08);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p24_m08);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m24_p08);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m24_p08);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p08_p24);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p08_p24);

        u[0] = _mm256_add_epi32(v[0], v[4]);
        u[1] = _mm256_add_epi32(v[1], v[5]);
        u[2] = _mm256_add_epi32(v[2], v[6]);
        u[3] = _mm256_add_epi32(v[3], v[7]);
        u[4] = _mm256_sub_epi32(v[0], v[4]);
        u[5] = _mm256_sub_epi32(v[1], v[5]);
        u[6] = _mm256_sub_epi32(v[2], v[6]);
        u[7] = _mm256_sub_epi32(v[3], v[7]);
        u[8] = _mm256_add_epi32(v[8], v[12]);
        u[9] = _mm256_add_epi32(v[9], v[13]);
        u[10] = _mm256_add_epi32(v[10], v[14]);
        u[11] = _mm256_add_epi32(v[11], v[15]);
        u[12] = _mm256_sub_epi32(v[8], v[12]);
        u[13] = _mm256_sub_epi32(v[9], v[13]);
        u[14] = _mm256_sub_epi32(v[10], v[14]);
        u[15] = _mm256_sub_epi32(v[11], v[15]);

        u[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        s[0] = _mm256_add_epi16(x[0], x[2]);
        s[1] = _mm256_add_epi16(x[1], x[3]);
        s[2] = _mm256_sub_epi16(x[0], x[2]);
        s[3] = _mm256_sub_epi16(x[1], x[3]);
        s[4] = _mm256_packs_epi32(v[0], v[1]);
        s[5] = _mm256_packs_epi32(v[2], v[3]);
        s[6] = _mm256_packs_epi32(v[4], v[5]);
        s[7] = _mm256_packs_epi32(v[6], v[7]);
        s[8] = _mm256_add_epi16(x[8], x[10]);
        s[9] = _mm256_add_epi16(x[9], x[11]);
        s[10] = _mm256_sub_epi16(x[8], x[10]);
        s[11] = _mm256_sub_epi16(x[9], x[11]);
        s[12] = _mm256_packs_epi32(v[8], v[9]);
        s[13] = _mm256_packs_epi32(v[10], v[11]);
        s[14] = _mm256_packs_epi32(v[12], v[13]);
        s[15] = _mm256_packs_epi32(v[14], v[15]);

        // stage 4
        u[0] = _mm256_unpacklo_epi16(s[2], s[3]);
        u[1] = _mm256_unpackhi_epi16(s[2], s[3]);
        u[2] = _mm256_unpacklo_epi16(s[6], s[7]);
        u[3] = _mm256_unpackhi_epi16(s[6], s[7]);
        u[4] = _mm256_unpacklo_epi16(s[10], s[11]);
        u[5] = _mm256_unpackhi_epi16(s[10], s[11]);
        u[6] = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm256_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_m16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_m16);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p16_m16);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p16_m16);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p16_p16);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p16_p16);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_m16_p16);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_m16_p16);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m16_m16);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m16_m16);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p16_m16);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p16_m16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        in[0] = s[0];
        in[1] = _mm256_sub_epi16(kZero, s[8]);
        in[2] = s[12];
        in[3] = _mm256_sub_epi16(kZero, s[4]);
        in[4] = _mm256_packs_epi32(v[4], v[5]);
        in[5] = _mm256_packs_epi32(v[12], v[13]);
        in[6] = _mm256_packs_epi32(v[8], v[9]);
        in[7] = _mm256_packs_epi32(v[0], v[1]);
        in[8] = _mm256_packs_epi32(v[2], v[3]);
        in[9] = _mm256_packs_epi32(v[10], v[11]);
        in[10] = _mm256_packs_epi32(v[14], v[15]);
        in[11] = _mm256_packs_epi32(v[6], v[7]);
        in[12] = s[5];
        in[13] = _mm256_sub_epi16(kZero, s[13]);
        in[14] = s[9];
        in[15] = _mm256_sub_epi16(kZero, s[1]);
    }

    static void fdct16(__m256i *in) {
        // perform 16x16 1-D DCT for 8 columns
        __m256i i[8], s[8], p[8], t[8], u[16], v[16];
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);

        // stage 1
        i[0] = _mm256_add_epi16(in[0], in[15]);
        i[1] = _mm256_add_epi16(in[1], in[14]);
        i[2] = _mm256_add_epi16(in[2], in[13]);
        i[3] = _mm256_add_epi16(in[3], in[12]);
        i[4] = _mm256_add_epi16(in[4], in[11]);
        i[5] = _mm256_add_epi16(in[5], in[10]);
        i[6] = _mm256_add_epi16(in[6], in[9]);
        i[7] = _mm256_add_epi16(in[7], in[8]);

        s[0] = _mm256_sub_epi16(in[7], in[8]);
        s[1] = _mm256_sub_epi16(in[6], in[9]);
        s[2] = _mm256_sub_epi16(in[5], in[10]);
        s[3] = _mm256_sub_epi16(in[4], in[11]);
        s[4] = _mm256_sub_epi16(in[3], in[12]);
        s[5] = _mm256_sub_epi16(in[2], in[13]);
        s[6] = _mm256_sub_epi16(in[1], in[14]);
        s[7] = _mm256_sub_epi16(in[0], in[15]);

        p[0] = _mm256_add_epi16(i[0], i[7]);
        p[1] = _mm256_add_epi16(i[1], i[6]);
        p[2] = _mm256_add_epi16(i[2], i[5]);
        p[3] = _mm256_add_epi16(i[3], i[4]);
        p[4] = _mm256_sub_epi16(i[3], i[4]);
        p[5] = _mm256_sub_epi16(i[2], i[5]);
        p[6] = _mm256_sub_epi16(i[1], i[6]);
        p[7] = _mm256_sub_epi16(i[0], i[7]);

        u[0] = _mm256_add_epi16(p[0], p[3]);
        u[1] = _mm256_add_epi16(p[1], p[2]);
        u[2] = _mm256_sub_epi16(p[1], p[2]);
        u[3] = _mm256_sub_epi16(p[0], p[3]);

        v[0] = _mm256_unpacklo_epi16(u[0], u[1]);
        v[1] = _mm256_unpackhi_epi16(u[0], u[1]);
        v[2] = _mm256_unpacklo_epi16(u[2], u[3]);
        v[3] = _mm256_unpackhi_epi16(u[2], u[3]);

        u[0] = _mm256_madd_epi16(v[0], k__cospi_p16_p16);
        u[1] = _mm256_madd_epi16(v[1], k__cospi_p16_p16);
        u[2] = _mm256_madd_epi16(v[0], k__cospi_p16_m16);
        u[3] = _mm256_madd_epi16(v[1], k__cospi_p16_m16);
        u[4] = _mm256_madd_epi16(v[2], k__cospi_p24_p08);
        u[5] = _mm256_madd_epi16(v[3], k__cospi_p24_p08);
        u[6] = _mm256_madd_epi16(v[2], k__cospi_m08_p24);
        u[7] = _mm256_madd_epi16(v[3], k__cospi_m08_p24);

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

        in[0] = _mm256_packs_epi32(u[0], u[1]);
        in[4] = _mm256_packs_epi32(u[4], u[5]);
        in[8] = _mm256_packs_epi32(u[2], u[3]);
        in[12] = _mm256_packs_epi32(u[6], u[7]);

        u[0] = _mm256_unpacklo_epi16(p[5], p[6]);
        u[1] = _mm256_unpackhi_epi16(p[5], p[6]);
        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);

        u[0] = _mm256_packs_epi32(v[0], v[1]);
        u[1] = _mm256_packs_epi32(v[2], v[3]);

        t[0] = _mm256_add_epi16(p[4], u[0]);
        t[1] = _mm256_sub_epi16(p[4], u[0]);
        t[2] = _mm256_sub_epi16(p[7], u[1]);
        t[3] = _mm256_add_epi16(p[7], u[1]);

        u[0] = _mm256_unpacklo_epi16(t[0], t[3]);
        u[1] = _mm256_unpackhi_epi16(t[0], t[3]);
        u[2] = _mm256_unpacklo_epi16(t[1], t[2]);
        u[3] = _mm256_unpackhi_epi16(t[1], t[2]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p28_p04);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p28_p04);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p12_p20);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p12_p20);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_m20_p12);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_m20_p12);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_m04_p28);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_m04_p28);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        in[2] = _mm256_packs_epi32(v[0], v[1]);
        in[6] = _mm256_packs_epi32(v[4], v[5]);
        in[10] = _mm256_packs_epi32(v[2], v[3]);
        in[14] = _mm256_packs_epi32(v[6], v[7]);

        // stage 2
        u[0] = _mm256_unpacklo_epi16(s[2], s[5]);
        u[1] = _mm256_unpackhi_epi16(s[2], s[5]);
        u[2] = _mm256_unpacklo_epi16(s[3], s[4]);
        u[3] = _mm256_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        t[2] = _mm256_packs_epi32(v[0], v[1]);
        t[3] = _mm256_packs_epi32(v[2], v[3]);
        t[4] = _mm256_packs_epi32(v[4], v[5]);
        t[5] = _mm256_packs_epi32(v[6], v[7]);

        // stage 3
        p[0] = _mm256_add_epi16(s[0], t[3]);
        p[1] = _mm256_add_epi16(s[1], t[2]);
        p[2] = _mm256_sub_epi16(s[1], t[2]);
        p[3] = _mm256_sub_epi16(s[0], t[3]);
        p[4] = _mm256_sub_epi16(s[7], t[4]);
        p[5] = _mm256_sub_epi16(s[6], t[5]);
        p[6] = _mm256_add_epi16(s[6], t[5]);
        p[7] = _mm256_add_epi16(s[7], t[4]);

        // stage 4
        u[0] = _mm256_unpacklo_epi16(p[1], p[6]);
        u[1] = _mm256_unpackhi_epi16(p[1], p[6]);
        u[2] = _mm256_unpacklo_epi16(p[2], p[5]);
        u[3] = _mm256_unpackhi_epi16(p[2], p[5]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m08_p24);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m08_p24);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p24_p08);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p24_p08);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p08_m24);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p08_m24);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_p24_p08);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_p24_p08);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        t[1] = _mm256_packs_epi32(v[0], v[1]);
        t[2] = _mm256_packs_epi32(v[2], v[3]);
        t[5] = _mm256_packs_epi32(v[4], v[5]);
        t[6] = _mm256_packs_epi32(v[6], v[7]);

        // stage 5
        s[0] = _mm256_add_epi16(p[0], t[1]);
        s[1] = _mm256_sub_epi16(p[0], t[1]);
        s[2] = _mm256_add_epi16(p[3], t[2]);
        s[3] = _mm256_sub_epi16(p[3], t[2]);
        s[4] = _mm256_sub_epi16(p[4], t[5]);
        s[5] = _mm256_add_epi16(p[4], t[5]);
        s[6] = _mm256_sub_epi16(p[7], t[6]);
        s[7] = _mm256_add_epi16(p[7], t[6]);

        // stage 6
        u[0] = _mm256_unpacklo_epi16(s[0], s[7]);
        u[1] = _mm256_unpackhi_epi16(s[0], s[7]);
        u[2] = _mm256_unpacklo_epi16(s[1], s[6]);
        u[3] = _mm256_unpackhi_epi16(s[1], s[6]);
        u[4] = _mm256_unpacklo_epi16(s[2], s[5]);
        u[5] = _mm256_unpackhi_epi16(s[2], s[5]);
        u[6] = _mm256_unpacklo_epi16(s[3], s[4]);
        u[7] = _mm256_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p30_p02);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p30_p02);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p14_p18);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p14_p18);
        v[4] = _mm256_madd_epi16(u[4], k__cospi_p22_p10);
        v[5] = _mm256_madd_epi16(u[5], k__cospi_p22_p10);
        v[6] = _mm256_madd_epi16(u[6], k__cospi_p06_p26);
        v[7] = _mm256_madd_epi16(u[7], k__cospi_p06_p26);
        v[8] = _mm256_madd_epi16(u[6], k__cospi_m26_p06);
        v[9] = _mm256_madd_epi16(u[7], k__cospi_m26_p06);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_m10_p22);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_m10_p22);
        v[12] = _mm256_madd_epi16(u[2], k__cospi_m18_p14);
        v[13] = _mm256_madd_epi16(u[3], k__cospi_m18_p14);
        v[14] = _mm256_madd_epi16(u[0], k__cospi_m02_p30);
        v[15] = _mm256_madd_epi16(u[1], k__cospi_m02_p30);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        in[1] = _mm256_packs_epi32(v[0], v[1]);
        in[9] = _mm256_packs_epi32(v[2], v[3]);
        in[5] = _mm256_packs_epi32(v[4], v[5]);
        in[13] = _mm256_packs_epi32(v[6], v[7]);
        in[3] = _mm256_packs_epi32(v[8], v[9]);
        in[11] = _mm256_packs_epi32(v[10], v[11]);
        in[7] = _mm256_packs_epi32(v[12], v[13]);
        in[15] = _mm256_packs_epi32(v[14], v[15]);
    }

    template <int tx_type> static void fht16x16_avx2(const short *input, short *output, int stride)
    {
        __m256i in[16];

        if (tx_type == DCT_DCT) {
            fdct16x16_avx2(input, output, stride);
        } else if (tx_type == ADST_DCT) {
            load_buffer_16x16(input, stride, in);
            fadst16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fdct16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else if (tx_type == DCT_ADST) {
            load_buffer_16x16(input, stride, in);
            fdct16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fadst16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else if (tx_type == ADST_ADST) {
            load_buffer_16x16(input, stride, in);
            fadst16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fadst16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else {
            assert(0);
        }
    }
};  // namespace details

namespace AV1PP {
    template <int size, int type> void ftransform_vp9_avx2(const short *src, short *dst, int pitchSrc);

    template <> void ftransform_vp9_avx2<TX_8X8, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, ADST_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, DCT_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, ADST_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_avx2<TX_16X16, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, ADST_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, DCT_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, ADST_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<ADST_ADST>(src, dst, pitchSrc);
    }
};  // namespace AV1PP
