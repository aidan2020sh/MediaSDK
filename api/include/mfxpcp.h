/******************************************************************************* *\

Copyright (C) 2007-2019 Intel Corporation.  All rights reserved.

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

File Name: mfxpcp.h

\* ****************************************************************************** */
#ifndef __MFXPCP_H__
#define __MFXPCP_H__
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if !defined(OPEN_SOURCE)
enum {
    MFX_HANDLE_DECODER_DRIVER_HANDLE            =2,       /* Driver's handle for a video decoder that can be used to configure content protection*/
    MFX_HANDLE_DXVA2_DECODER_DEVICE             =MFX_HANDLE_DECODER_DRIVER_HANDLE,      /* A handle to the DirectX Video Acceleration 2 (DXVA-2) decoder device*/
    MFX_HANDLE_VIDEO_DECODER                    =5       /* Pointer to the IDirectXVideoDecoder or ID3D11VideoDecoder interface*/
}; //mfxHandleType

/* Frame Memory Types */
enum {
    MFX_MEMTYPE_PROTECTED   =   0x0080
};
#endif

#if (MFX_VERSION >= 1030) || !defined(OPEN_SOURCE)
/* Protected in mfxVideoParam */
enum {
#if !defined(OPEN_SOURCE)
    MFX_PROTECTION_PAVP                 = 0x0001,
    MFX_PROTECTION_GPUCP_PAVP           = 0x0002,
    MFX_PROTECTION_GPUCP_AES128_CTR     = 0x0003,
#endif
#if (MFX_VERSION >= 1030)
    MFX_PROTECTION_CENC_WV_CLASSIC      = 0x0004,
    MFX_PROTECTION_CENC_WV_GOOGLE_DASH  = 0x0005,
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(OPEN_SOURCE)
    MFX_PROTECTION_WIDEVINE_CLASSIC     = 0x0006,
    MFX_PROTECTION_WIDEVINE_GOOGLE_DASH = 0x0007,
#endif
};
#endif // #if (MFX_VERSION >= 1030) || !defined(OPEN_SOURCE)

#if !defined(OPEN_SOURCE)
/* EncryptionType in mfxExtPAVPOption */
enum
{
    MFX_PAVP_AES128_CTR = 2,
    MFX_PAVP_AES128_CBC = 4,
    MFX_PAVP_DECE_AES128_CTR = 16
};

/* CounterType in mfxExtPAVPOption */
enum
{
    MFX_PAVP_CTR_TYPE_A = 1,
    MFX_PAVP_CTR_TYPE_B = 2,
    MFX_PAVP_CTR_TYPE_C = 4
};
#endif

#if (MFX_VERSION >= 1030) || !defined(OPEN_SOURCE)
/* Extended Buffer Ids */
enum {
#if !defined(OPEN_SOURCE)
    MFX_EXTBUFF_PAVP_OPTION         = MFX_MAKEFOURCC('P','V','O','P'),
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    MFX_EXTBUFF_DECRYPTED_PARAM     = MFX_MAKEFOURCC('D','C','R','P'),
#endif
#endif
#if (MFX_VERSION >= 1030)
    MFX_EXTBUFF_CENC_PARAM          = MFX_MAKEFOURCC('C','E','N','P')
#endif
};
#endif // #if (MFX_VERSION >= 1030) || !defined(OPEN_SOURCE)

#if !defined(OPEN_SOURCE)
MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
typedef struct _mfxAES128CipherCounter{
    mfxU64  IV;
    mfxU64  Count;
} mfxAES128CipherCounter;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
typedef struct _mfxEncryptedData{
    mfxEncryptedData *Next;
    mfxHDL reserved1;
    mfxU8  *Data;
    mfxU32 DataOffset; /* offset, in bytes, from beginning of buffer to first byte of encrypted data*/
    mfxU32 DataLength; /* size of plain data in bytes */
    mfxU32 MaxLength; /*allocated  buffer size in bytes*/
    mfxAES128CipherCounter CipherCounter;
    mfxU32 AppId;
    mfxU32 reserved2[7];
} mfxEncryptedData;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
typedef struct _mfxExtPAVPOption{
    mfxExtBuffer    Header; /* MFX_EXTBUFF_PAVP_OPTION */
    mfxAES128CipherCounter CipherCounter;
    mfxU32      CounterIncrement;
    mfxU16      EncryptionType;
    mfxU16      CounterType;
    mfxU32      reserved[8];
} mfxExtPAVPOption;
MFX_PACK_END()

#if (MFX_VERSION >= MFX_VERSION_NEXT)
MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct _mfxExtDecryptedParam{
    mfxExtBuffer Header;

    mfxU8  *Data;
    mfxU32 DataLength;
    mfxU32 reserved[11];
} mfxExtDecryptedParam;
MFX_PACK_END()
#endif
#endif

#if (MFX_VERSION >= 1030)
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct _mfxExtCencParam{
    mfxExtBuffer Header;

    mfxU32 StatusReportIndex;
    mfxU32 reserved[15];
} mfxExtCencParam;
MFX_PACK_END()
#endif

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */


#endif

