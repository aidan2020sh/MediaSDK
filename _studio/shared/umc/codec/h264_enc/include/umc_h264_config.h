// Copyright (c) 2004-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

#ifndef __UMC_H264_CONFIG__
#define __UMC_H264_CONFIG__

//#define H264_LOG
#undef H264_LOG

#define H264_RECODE_PCM
//#undef H264_RECODE_PCM

#define H264_INSERT_CABAC_ZERO_WORDS
//#undef H264_INSERT_CABAC_ZERO_WORDS

#define H264_INTRA_REFRESH
//#undef H264_INTRA_REFRESH

//#define H264_DUMP_RECON
#undef H264_DUMP_RECON

//#define BITDEPTH_9_12
#undef BITDEPTH_9_12

#define ALPHA_BLENDING_H264
//#undef ALPHA_BLENDING_H264

//#define FRAME_QP_FROM_FILE "fqp.txt"
#undef FRAME_QP_FROM_FILE

// enables 1 frame ahead analysis, perception correlated quantization. Not optimized, worse PSNR.
//#define USE_PSYINFO
#undef USE_PSYINFO

// use temporal template for GOP encoding control with temporal scalability. Not finished.
//#define USE_TEMPTEMP
#undef USE_TEMPTEMP

#define TABLE_FUNC
//#undef TABLE_FUNC

#define NO_PADDING
//#undef NO_PADDING

#define NV12_INTERPOLATION
//#undef NV12_INTERPOLATION

#define USE_NV12
//#undef USE_NV12

#define FRAME_INTERPOLATION
//#undef FRAME_INTERPOLATION

//#define FRAME_TYPE_DETECT_DS
#undef FRAME_TYPE_DETECT_DS

//#define ALT_BITSTREAM_ALLOC
#undef ALT_BITSTREAM_ALLOC

#if defined(FRAME_INTERPOLATION) && defined(NO_PADDING)
    #undef NO_PADDING
#endif // FRAME_INTERPOLATION && NO_PADDING

#ifndef UMC_RESTRICTED_CODE_MBT

#define MB_THREADING
//#undef MB_THREADING
#ifdef MB_THREADING
//#define MB_THREADING_TW
//#undef MB_THREADING_TW
#define MB_THREADING_VM
//#undef MB_THREADING_VM
#endif // MB_THREADING
#ifdef MB_THREADING_VM
#include <thread>
#include <mutex>
#include "vm_thread.h"
#endif // // MB_THREADING_VM

#endif // UMC_RESTRICTED_CODE_MBT


#if defined _OPENMP
#include "omp.h"

#ifndef MB_THREADING
#define FRAMETYPE_DETECT_ST  //Threaded frame type detector for slice threading
//#undef FRAMETYPE_DETECT_ST
#define DEBLOCK_THREADING     //Threaded deblocking for slice threading
//#undef DEBLOCK_THREADING
#define EXPAND_PLANE_THREAD   //Parallel expand plane for chroma
//#undef EXPAND_PLANE_THREAD
#define INTERPOLATE_FRAME_THREAD //Parallel frame pre interpolation
//#undef INTERPOLATE_FRAME_THREAD
#endif
#endif // _OPENMP
#define SLICE_THREADING_LOAD_BALANCING // Redistribute macroblocks between slices based on previously encoded frame timings
//#undef SLICE_THREADING_LOAD_BALANCING

#undef INTRINSIC_OPT
#if !defined(WIN64) && (defined (WIN32) || defined (_WIN32) || defined (WIN32E) || defined (_WIN32E) || defined(__i386__) || defined(__x86_64__))
    #if defined(__INTEL_COMPILER) || (_MSC_VER >= 1300) || (defined(__GNUC__) && defined(__SSE2__) && (__GNUC__ > 3))
        #define INTRINSIC_OPT
        #include "emmintrin.h"
    #endif
#endif

//#undef INTRINSIC_OPT

#endif /* __UMC_H264_CONFIG__ */

#endif //MFX_ENABLE_H264_VIDEO_ENCODE