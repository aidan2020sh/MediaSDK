// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_LOG_H
#define __UMC_H264_LOG_H

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_heap.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_frame.h"
#include "umc_h264_frame_info.h"

#include "vm_file.h"

//#define ENABLE_LOGGING
#define H264_LOG_FILE_NAME VM_STRING(".\\h264dec.log")

namespace UMC
{

class Logging
{
#ifdef ENABLE_LOGGING
public:

    Logging();
    ~Logging();

    void LogFrame(H264DecoderFrame * pFrame);

private:

    void Trace(vm_char * format, ...);

    void LogRefFrame(H264DecoderFrame * pFrame);

    H264DecoderFrame * m_pFrame;

    struct Stat
    {
        int32_t mbtypes[20];

        int32_t subtype_8x8_Count;
        int32_t subtype_8x4_Count;
        int32_t subtype_4x8_Count;
        int32_t subtype_4x4_Count;

        int32_t zeroMVCount;

        Stat()
            : zeroMVCount(0)
            , subtype_8x8_Count(0)
            , subtype_8x4_Count(0)
            , subtype_4x8_Count(0)
            , subtype_4x4_Count(0)
        {
            memset(mbtypes, 0, sizeof(mbtypes));
        }

        void Add(Stat & stat)
        {
            for (int32_t i = 0; i < sizeof(mbtypes)/sizeof(mbtypes[0]); i++)
            {
                mbtypes[i] += stat.mbtypes[i];
            }

            subtype_8x8_Count += stat.subtype_8x8_Count;
            subtype_8x4_Count += stat.subtype_8x4_Count;
            subtype_4x8_Count += stat.subtype_4x8_Count;
            subtype_4x4_Count += stat.subtype_4x4_Count;

            zeroMVCount += stat.zeroMVCount;
        }
    };

    struct PictureInfo
    {
        Stat stat;
        int32_t count;
        int32_t bitSize;
        int32_t type;
        int32_t mbsCount;

        PictureInfo()
            : count(0)
            , bitSize(0)
            , mbsCount(1)
        {
        }
    };

    PictureInfo picturesStat[3];

    void MBLayerStat(H264DecoderGlobalMacroblocksDescriptor &pGlobalInfo, int32_t mbNumber, Stat & stat);
    void SBtype(Stat & stat, int32_t sbtype);

    int32_t CalculateFrameSize(H264DecoderFrame *m_pFrame);
    void PrintStat(Stat &stat, int32_t type);

    void PrintPictureStat(PictureInfo & picStat);

    vm_file * m_File;
#endif
};

Logging * GetLogging();

} // namespace UMC

#endif // __UMC_H264_LOG_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
