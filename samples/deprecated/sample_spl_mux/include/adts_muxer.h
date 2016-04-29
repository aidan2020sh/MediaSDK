/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#pragma warning( push )

#pragma warning( disable : 4244 )
#pragma warning( disable : 4204 )

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"

#pragma warning( pop )

#include "mfxsplmux.h"

#define ADTS_HEADER_WITH_CRC_LENGTH 9

typedef struct _adtsMuxer *adtsMuxer;

typedef struct
{
    mfxDataIO                *dataIO;
    AVIOContext              *ioContext;
    AVFormatContext          *formatContext;
    int                       dataLen;
    AVStream                 *adtsStream;
} _adtsMuxer;

mfxStatus InitAdtsMuxer(AVCodecContext *inputCodecContext, adtsMuxer *mux);
mfxStatus CloseAdtsMuxer(adtsMuxer mux);
mfxStatus SetAdtsInfo(AVCodecContext *inputCodecContext, adtsMuxer mux);
mfxStatus GetAdtsBitstream(adtsMuxer mux, AVPacket *pkt, mfxBitstream *bs);
mfxI32    WriteADTS(void* pthis, mfxBitstream *bs);
