/*******************************************************************************

Copyright (C) 2013-2019 Intel Corporation.  All rights reserved.

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

File Name: mfxcommon.h

*******************************************************************************/
#ifndef __MFXCOMMON_H__
#define __MFXCOMMON_H__
#include "mfxdefs.h"

#if !defined (__GNUC__)
#pragma warning(disable: 4201)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define MFX_MAKEFOURCC(A,B,C,D)    ((((int)A))+(((int)B)<<8)+(((int)C)<<16)+(((int)D)<<24))

/* Extended Configuration Header Structure */
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxU32  BufferId;
    mfxU32  BufferSz;
} mfxExtBuffer;
MFX_PACK_END()

/* Library initialization and deinitialization */
typedef mfxI32 mfxIMPL;
#define MFX_IMPL_BASETYPE(x) (0x00ff & (x))

enum  {
    MFX_IMPL_AUTO         = 0x0000,  /* Auto Selection/In or Not Supported/Out */
    MFX_IMPL_SOFTWARE     = 0x0001,  /* Pure Software Implementation */
    MFX_IMPL_HARDWARE     = 0x0002,  /* Hardware Accelerated Implementation (default device) */
    MFX_IMPL_AUTO_ANY     = 0x0003,  /* Auto selection of any hardware/software implementation */
    MFX_IMPL_HARDWARE_ANY = 0x0004,  /* Auto selection of any hardware implementation */
    MFX_IMPL_HARDWARE2    = 0x0005,  /* Hardware accelerated implementation (2nd device) */
    MFX_IMPL_HARDWARE3    = 0x0006,  /* Hardware accelerated implementation (3rd device) */
    MFX_IMPL_HARDWARE4    = 0x0007,  /* Hardware accelerated implementation (4th device) */
    MFX_IMPL_RUNTIME      = 0x0008,
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    MFX_IMPL_SINGLE_THREAD= 0x0009,
#endif
    MFX_IMPL_VIA_ANY      = 0x0100,
    MFX_IMPL_VIA_D3D9     = 0x0200,
    MFX_IMPL_VIA_D3D11    = 0x0300,
    MFX_IMPL_VIA_VAAPI    = 0x0400,

    MFX_IMPL_AUDIO                     = 0x8000,
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    MFX_IMPL_EXTERNAL_THREADING        = 0x10000,
#endif

    MFX_IMPL_UNSUPPORTED  = 0x0000  /* One of the MFXQueryIMPL returns */
};

/* Version Info */
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef union {
    struct {
        mfxU16  Minor;
        mfxU16  Major;
    };
    mfxU32  Version;
} mfxVersion;
MFX_PACK_END()

/* session priority */
typedef enum
{
    MFX_PRIORITY_LOW = 0,
    MFX_PRIORITY_NORMAL = 1,
    MFX_PRIORITY_HIGH = 2

} mfxPriority;

typedef struct _mfxEncryptedData mfxEncryptedData;
MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
typedef struct {
     union {
        struct {
            mfxEncryptedData* EncryptedData;
            mfxExtBuffer **ExtParam;
            mfxU16  NumExtParam;
        };
         mfxU32  reserved[6];
     };
    mfxI64  DecodeTimeStamp;
    mfxU64  TimeStamp;
    mfxU8*  Data;
    mfxU32  DataOffset;
    mfxU32  DataLength;
    mfxU32  MaxLength;

    mfxU16  PicStruct;
    mfxU16  FrameType;
    mfxU16  DataFlag;
    mfxU16  reserved2;
} mfxBitstream;
MFX_PACK_END()

typedef struct _mfxSyncPoint *mfxSyncPoint;

/* GPUCopy */
enum {
    MFX_GPUCOPY_DEFAULT = 0,
    MFX_GPUCOPY_ON      = 1,
    MFX_GPUCOPY_OFF     = 2
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxIMPL     Implementation;
    mfxVersion  Version;
    mfxU16      ExternalThreads;
    union {
        struct {
            mfxExtBuffer **ExtParam;
            mfxU16  NumExtParam;
        };
        mfxU16  reserved2[5];
    };
    mfxU16      GPUCopy;
#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(OPEN_SOURCE)
    mfxU16      AltDependencies;
    mfxU16      reserved[20];
#else
    mfxU16      reserved[21];
#endif
} mfxInitParam;
MFX_PACK_END()

enum {
    MFX_EXTBUFF_THREADS_PARAM = MFX_MAKEFOURCC('T','H','D','P')
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer Header;

    mfxU16       NumThread;
    mfxI32       SchedulingType;
    mfxI32       Priority;
    mfxU16       reserved[55];
} mfxExtThreadsParam;
MFX_PACK_END()

/* PlatformCodeName */
enum {
    MFX_PLATFORM_UNKNOWN        = 0,
    MFX_PLATFORM_SANDYBRIDGE    = 1,
    MFX_PLATFORM_IVYBRIDGE      = 2,
    MFX_PLATFORM_HASWELL        = 3,
    MFX_PLATFORM_BAYTRAIL       = 4,
    MFX_PLATFORM_BROADWELL      = 5,
    MFX_PLATFORM_CHERRYTRAIL    = 6,
    MFX_PLATFORM_SKYLAKE        = 7,
    MFX_PLATFORM_APOLLOLAKE     = 8,
    MFX_PLATFORM_KABYLAKE       = 9,
#if (MFX_VERSION >= 1025)
    MFX_PLATFORM_GEMINILAKE     = 10,
    MFX_PLATFORM_COFFEELAKE     = 11,
    MFX_PLATFORM_CANNONLAKE     = 20,
#endif
#if (MFX_VERSION >= 1027)
    MFX_PLATFORM_ICELAKE = 30,
#endif
#ifndef STRIP_EMBARGO
    MFX_PLATFORM_LAKEFIELD      = 31,
    MFX_PLATFORM_JASPERLAKE     = 32,
#endif
    MFX_PLATFORM_TIGERLAKE      = 40,
#ifndef STRIP_EMBARGO
    MFX_PLATFORM_ROCKETLAKE     = 42,
    MFX_PLATFORM_ALDERLAKE_S    = 43,
    MFX_PLATFORM_ALDERLAKE_UH   = 44,
#endif
};

#if (MFX_VERSION >= 1031)
typedef enum
{
    MFX_MEDIA_UNKNOWN           = 0xffff,
    MFX_MEDIA_INTEGRATED        = 0,
    MFX_MEDIA_DISCRETE          = 1
} mfxMediaAdapterType;
#endif

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxU16 CodeName;
    mfxU16 DeviceId;
#if (MFX_VERSION >= 1031)
    mfxU16 MediaAdapterType;
    mfxU16 reserved[13];
#else
    mfxU16 reserved[14];
#endif
} mfxPlatform;
MFX_PACK_END()

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

