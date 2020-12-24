/******************************************************************************* *\

Copyright (C) 2013-2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxwidi.h

\* ****************************************************************************** */


/* This file contains declaration of private Media SDK API extension for WiDi support.
IT SHOULD NOT be released as part of public development package. */

#ifndef __MFXWIDI_H__
#define __MFXWIDI_H__
#include "mfxstructures.h"

extern "C" {
enum {
    MFX_EXTBUFF_ENCODER_WIDI_USAGE = MFX_MAKEFOURCC('W','I','D','I')
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer    Header;
    mfxU32          reserved[30];
} mfxExtAVCEncoderWiDiUsage;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif