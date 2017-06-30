/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include <assert.h>
#include "mfxstructures.h"
#include "sample_hevc_fei_defs.h"

template<class T> inline void Zero(std::vector<T> & vec) { memset(&vec[0], 0, sizeof(T) * vec.size()); }
template<class T> inline void Zero(T * first, size_t cnt) { memset(first, 0, sizeof(T) * cnt); }
template<class T> inline void Fill(T & obj, int val) { memset(&obj, val, sizeof(obj)); }
template<class T> inline T Clip3(T min, T max, T x) { return std::min(std::max(min, x), max); }

enum
{
    MAX_DPB_SIZE = 15,
    IDX_INVALID = 0xFF,
};

typedef struct _HevcDpbFrame
{
    mfxI32   m_poc;
    mfxU32   m_fo;    // FrameOrder
    mfxU32   m_eo;    // Encoded order
    mfxU32   m_bpo;   // Bpyramid order
    mfxU32   m_level; // pyramid level
    mfxU8    m_tid;
    bool     m_ltr;   // is "long-term"
    bool     m_ldb;   // is "low-delay B"
    mfxU8    m_codingType;
    mfxU8    m_idxRec;
    mfxFrameSurface1 * m_surf; //input surface
} HevcDpbFrame, HevcDpbArray[MAX_DPB_SIZE];

enum
{
    TASK_DPB_ACTIVE = 0, // available during task execution
    TASK_DPB_AFTER,      // after task execution (ACTIVE + curTask if ref)
    TASK_DPB_NUM
};

typedef struct _HevcTask : HevcDpbFrame
{
    mfxU16       m_frameType;
    mfxU8        m_refPicList[2][MAX_DPB_SIZE];
    mfxU8        m_numRefActive[2]; // L0 and L1 lists
    mfxI32       m_lastIPoc;
    HevcDpbArray m_dpb[TASK_DPB_NUM];

} HevcTask;
