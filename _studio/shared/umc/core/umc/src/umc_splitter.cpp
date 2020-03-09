// Copyright (c) 2003-2020 Intel Corporation
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

#include <string.h>
#include "umc_splitter.h"
#include "mfxstructures.h"

#if defined (__ICL)
// multicharacter character literal (potential portability problem)
// we support Win platform only
#pragma warning(disable:1899)
#endif

#define MAKEFOURCC_INVERT(D,C,B,A)    ((((int)A))+(((int)B)<<8)+(((int)C)<<16)+(((int)D)<<24))

namespace UMC
{

SplitterParams::SplitterParams()
{
   m_lFlags = 0;
   m_pDataReader = NULL;
   m_uiSelectedVideoPID = SELECT_ANY_VIDEO_PID;
   m_uiSelectedAudioPID = SELECT_ANY_AUDIO_PID;
   m_pMemoryAllocator = NULL;
} // SplitterParams::SplitterParams()

SplitterParams::~SplitterParams()
{
} // SplitterParams::~SplitterParams()

SplitterInfo::SplitterInfo()
{
    m_splitter_flags = 0;
    m_SystemType = UNDEF_STREAM;
    m_nOfTracks = 0;
    m_dRate = 1;
    m_dDuration = -1.0;
    m_ppTrackInfo = NULL;
    DeprecatedSplitterInfo();
} // SplitterInfo::SplitterInfo()

SplitterInfo::~SplitterInfo()
{
} // SplitterInfo::~SplitterInfo()

Splitter::Splitter():
  m_pDataReader(NULL)
{
    DeprecatedSplitter(); // Init deprecated variables
}

SystemStreamType Splitter::GetStreamType(DataReader* dr)
{
    uint32_t long_code;
    Status umcSts = UMC_OK;

    if (NULL == dr)
        return UNDEF_STREAM;
    else
        dr->Reset();

    umcSts = dr->Check32u(&long_code, 0);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    // it can be either avs or mpeg4 format
    if (long_code == 0x000001B0)
    {
        uint8_t oneByte;

        // the header of avs standard is 18 bytes long.
        // the one of mpeg4 standard is only one byte long.
        umcSts = dr->Check8u(&oneByte, 5);
        if (UMC_OK != umcSts)
            return UNDEF_STREAM;

        if (oneByte)
            return AVS_PURE_VIDEO_STREAM;
    }

    if (0x80000001 == long_code)
    {
        uint8_t oneByte;

        // it is known bug of avs reference encoder -
        // it adds extra 0x080 byte at the beginning.
        umcSts = dr->Check8u(&oneByte, 4);
        if (UMC_OK != umcSts)
            return UNDEF_STREAM;
        if (oneByte == 0x0B0)
        {
            return AVS_PURE_VIDEO_STREAM;
        }
    }

    if (long_code == 0x0000010F || (long_code&0xFF) == 0xC5)
    {
        return VC1_PURE_VIDEO_STREAM;
    }

    if (long_code == 0x3026b275)
    {
        return ASF_STREAM;
    }

    if (long_code == MAKEFOURCC_INVERT('R','I','F','F')) // RIFF
    {
        umcSts = dr->Check32u(&long_code, 8);
        if (long_code == MAKEFOURCC_INVERT('A','V','I',' '))
        {
            //avi RIFF container
            return AVI_STREAM;
        }
    }
    if (long_code == 0x464c5601) // "FLV 0x01"  FLV version1
    {
        return FLV_STREAM;
    }
    //IVF : http://wiki.multimedia.cx/index.php?title=IVF
    if (long_code == MAKEFOURCC_INVERT('D','K','I','F'))
    {
        return IVF_STREAM;
    }

    if (long_code == 0x1a45dfa3) // EBML file ID
    {
        uint8_t len_mask = 0x80, Element_size;
        size_t size = 1;
        uint16_t ElementID;
        uint32_t DocType;

        umcSts = dr->Check8u(&Element_size, 4);
        if (UMC_OK != umcSts)
            return UNDEF_STREAM;
        while (size <= 8 && !(Element_size & len_mask))
        {
            size++;
            len_mask >>= 1;
        }
        if (size <= 8)
        {
            // per 4 byte for EBML file ID, Version, ReadVersion, MaxIDLength, MaxSizeLength
            umcSts = dr->Check16u(&ElementID, 20 + size);
            if (UMC_OK != umcSts)
                return UNDEF_STREAM;
            if (ElementID == 0x4282) // DocType Element ID
            {
                umcSts = dr->Check32u(&DocType, 23 + size);
                if (UMC_OK != umcSts)
                    return UNDEF_STREAM;
                if (DocType == MAKEFOURCC_INVERT('w', 'e', 'b', 'm')) // 0x7765626d
                    return WEBM_STREAM;
            }
        }
    }

    umcSts = dr->Check32u(&long_code, 4);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    if (long_code == MAKEFOURCC_INVERT('f','t','y','p'))
    {
        umcSts = dr->Check32u(&long_code,8);
        if (UMC_OK != umcSts) return UNDEF_STREAM;

        // mp42
        if (long_code == MAKEFOURCC_INVERT('m','p','4','2') ||
            long_code == MAKEFOURCC_INVERT('m','p','4','1') ||
            long_code == MAKEFOURCC_INVERT('i','s','o','m') ||
            long_code == MAKEFOURCC_INVERT('i','s','o','4') ||
            long_code == MAKEFOURCC_INVERT('M','S','N','V') ||
            long_code == MAKEFOURCC_INVERT('M','4','V',' ') ||
            long_code == MAKEFOURCC_INVERT('M','4','A',' ') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','6') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','4') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','5') ||
            long_code == MAKEFOURCC_INVERT('a','v','c','1') ||
            long_code == MAKEFOURCC_INVERT('a','v','s','2') ||
            long_code == MAKEFOURCC_INVERT('q','t',' ',' ') ||
            long_code == MAKEFOURCC_INVERT('X','A','V','C'))
        {
            //MP4 container
            return MP4_ATOM_STREAM;
        }
    }

    umcSts = dr->Check32u(&long_code,4);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    if (long_code == MAKEFOURCC_INVERT('m','o','o','v'))
    {
        return MP4_ATOM_STREAM;
    }

    return MPEGx_SYSTEM_STREAM;
} // SystemStreamType Splitter::GetStreamType(DataReader* dr)

} // namespace UMC
