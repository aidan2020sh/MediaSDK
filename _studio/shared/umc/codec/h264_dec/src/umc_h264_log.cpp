// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_log.h"

#ifdef ENABLE_LOGGING
#include <stdarg.h>
#endif

namespace UMC
{
#ifdef ENABLE_LOGGING
    Logging * GetLogging()
    {
        static Logging log;
        return &log;
    }
#else
    Logging * GetLogging()
    {
        return 0;
    }
#endif

#ifdef ENABLE_LOGGING

static const vm_char * frameTypes[] =
{
    VM_STRING("None"),
    VM_STRING("I_PICTURE"),
    VM_STRING("P_PICTURE"),
    VM_STRING("B_PICTURE")
};

Logging::Logging()
{
    m_File = vm_file_fopen(H264_LOG_FILE_NAME, VM_STRING("rw+"));

    picturesStat[0].type = 1;
    picturesStat[1].type = 2;
    picturesStat[2].type = 3;
}

Logging::~Logging()
{
    Trace(VM_STRING("\n"));
    PrintPictureStat(picturesStat[0]);
    PrintPictureStat(picturesStat[1]);
    PrintPictureStat(picturesStat[2]);
    vm_file_close(m_File);
}

void Logging::PrintPictureStat(PictureInfo & picStat)
{
    if (picStat.count)
    {
        Trace(VM_STRING("\n"));
        Trace(VM_STRING("%s frames - %d, size in bits - %d, avg size - %d\n"),
            frameTypes[picStat.type],
            picStat.count,
            picStat.bitSize, picStat.bitSize / picStat.count);
        PrintStat(picStat.stat, picStat.type);
    }
}

void Logging::Trace(vm_char * format, ...)
{
    va_list arglist;
    va_start(arglist, format);

    vm_char cStr[256];
    vm_string_vsprintf(cStr, format, arglist);

    vm_string_fprintf(m_File, VM_STRING("%s"), cStr);
    va_end(arglist);
}

void Logging::SBtype(Stat & stat, int32_t sbtype)
{
    switch(sbtype)
    {
    case SBTYPE_8x8:
        stat.subtype_8x8_Count++;
        break;
    case SBTYPE_8x4:
        stat.subtype_8x4_Count++;
        break;
    case SBTYPE_4x8:
        stat.subtype_4x8_Count++;
        break;

    case SBTYPE_DIRECT:
    case SBTYPE_4x4:
    default:
        stat.subtype_4x4_Count++;
        break;
    }
}

void Logging::MBLayerStat(H264DecoderGlobalMacroblocksDescriptor &pGlobalInfo, int32_t mbNumber, Stat & stat)
{
    H264DecoderMacroblockGlobalInfo &pMBInfo = pGlobalInfo.mbs[mbNumber];

    if (pMBInfo.mbflags.isSkipped)
    {
        stat.mbtypes[MBTYPE_SKIPPED]++;
    }else
    {
        if (pMBInfo.mbflags.isDirect)
        {
            stat.mbtypes[MBTYPE_DIRECT]++;
        }
        else
        {
            if (pMBInfo.mbtype == MBTYPE_FORWARD ||
                pMBInfo.mbtype == MBTYPE_BACKWARD ||
                pMBInfo.mbtype == MBTYPE_BIDIR)
            {
                stat.mbtypes[MBTYPE_INTER]++;
            }
            else
            {
                if (pMBInfo.mbtype == MBTYPE_INTER_8x8 ||
                    pMBInfo.mbtype == MBTYPE_INTER_8x8_REF0)
                {
                    stat.mbtypes[pMBInfo.mbtype]++;
                    SBtype(stat, pMBInfo.sbtype[0]);
                    SBtype(stat, pMBInfo.sbtype[1]);
                    SBtype(stat, pMBInfo.sbtype[2]);
                    SBtype(stat, pMBInfo.sbtype[3]);
                }
                else
                {
                    stat.mbtypes[pMBInfo.mbtype]++;
                }
            }
        }
    }

    // mbtype stat
    stat.zeroMVCount++;

    for (int32_t i = 0; i < 16; i++)
    {
        if (pGlobalInfo.MV[0][mbNumber].MotionVectors[i].mvx || pGlobalInfo.MV[0][mbNumber].MotionVectors[i].mvy)
        {
            stat.zeroMVCount--;
            break;
        }

        if (pGlobalInfo.MV[1][mbNumber].MotionVectors[i].mvx || pGlobalInfo.MV[1][mbNumber].MotionVectors[i].mvy)
        {
            stat.zeroMVCount--;
            break;
        }
    }
}

int32_t Logging::CalculateFrameSize(H264DecoderFrame *m_pFrame)
{
    H264DecoderFrameInfo * slicesInfo = m_pFrame->GetAU(0);

    int32_t size = 0;

    for (uint32_t i = 0; i < slicesInfo->GetSliceCount(); i++)
    {
        H264Slice * pSlice = slicesInfo->GetSlice(i);
        size += (int32_t)pSlice->GetBitStream()->GetAllBitsCount();
    }

    return size;
}

void Logging::LogFrame(H264DecoderFrame * pFrame)
{
    m_pFrame = pFrame;

    Trace(VM_STRING("\n"));
    int32_t bitSize = CalculateFrameSize(m_pFrame);
    Trace(VM_STRING("frame type - %s, uid - %d, size - %d\n"), frameTypes[m_pFrame->m_FrameType], m_pFrame->m_UID, bitSize);

    LogRefFrame(m_pFrame);

#ifdef UMC_RESTRICTED_CODE_VA
    H264DecoderFrameInfo * slicesInfo = m_pFrame->GetAU(0);
    const H264SeqParamSet *pSeqParam = slicesInfo->GetSlice(0)->GetSeqParam();

    int32_t iMBCount = pSeqParam->frame_width_in_mbs * pSeqParam->frame_height_in_mbs;

    Stat stat;
    for (int32_t x = 0; x < iMBCount; ++x)
    {
        MBLayerStat(m_pFrame->m_mbinfo, x, stat);
    }

    PictureInfo & currentPicture = picturesStat[m_pFrame->m_FrameType - 1];
    PrintStat(stat, currentPicture.type);

    currentPicture.count++;
    currentPicture.stat.Add(stat);
    currentPicture.bitSize += bitSize;
    currentPicture.mbsCount = iMBCount;
#endif
}

void Logging::LogRefFrame(H264DecoderFrame * pFrame)
{
    H264DecoderRefPicList* refPicList = pFrame->GetRefPicList(1, 0);

    if (refPicList->m_RefPicList[0])
    {
        Trace(VM_STRING("\n ref list 0 \n"));
        for (int32_t i = 0; refPicList->m_RefPicList[i]; i++)
        {
            Trace(VM_STRING("ref frame uid - %d\n"), refPicList->m_RefPicList[i]->m_UID);
        }
    }

    refPicList = pFrame->GetRefPicList(1, 1);

    if (refPicList->m_RefPicList[0])
    {
        Trace(VM_STRING("\n ref list 1 \n"));
        for (int32_t i = 0; refPicList->m_RefPicList[i]; i++)
        {
            Trace(VM_STRING("ref frame uid - %d\n"), refPicList->m_RefPicList[i]->m_UID);
        }
        Trace(VM_STRING("\n"));
    }
}

void Logging::PrintStat(Stat &stat, int32_t type)
{
    Trace(VM_STRING("I16x16 - %d\n"), stat.mbtypes[MBTYPE_INTRA_16x16]);
    Trace(VM_STRING("I4x4 - %d\n"), stat.mbtypes[MBTYPE_INTRA]);

    if (type == I_PICTURE)
        return;

    Trace(VM_STRING("Inter16x16 - %d\n"), stat.mbtypes[MBTYPE_INTER]);
    Trace(VM_STRING("Inter16x8 - %d\n"), stat.mbtypes[MBTYPE_INTER_16x8]);
    Trace(VM_STRING("Inter8x16 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x16]);
    Trace(VM_STRING("Inter8x8 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x8]);
    Trace(VM_STRING("Inter8x8_Ref0 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x8_REF0]);
    Trace(VM_STRING(" subtype 8x8 - %d\n"), stat.subtype_8x8_Count);
    Trace(VM_STRING(" subtype 8x4 - %d\n"), stat.subtype_8x4_Count);
    Trace(VM_STRING(" subtype 4x8 - %d\n"), stat.subtype_4x8_Count);
    Trace(VM_STRING(" subtype 4x4 - %d\n"), stat.subtype_4x4_Count);

    if (type == B_PICTURE)
        Trace(VM_STRING("Direct - %d\n"), stat.mbtypes[MBTYPE_DIRECT]);

    Trace(VM_STRING("Skipped - %d\n"), stat.mbtypes[MBTYPE_SKIPPED]);
    Trace(VM_STRING("Zero MVs count - %d\n"), stat.zeroMVCount);
}

#endif

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER