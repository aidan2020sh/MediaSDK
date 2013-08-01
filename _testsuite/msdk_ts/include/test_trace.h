#pragma once
#include <iostream>
#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxsvc.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxpcp.h"
#include "mfxaudio.h"

enum print_flags{
    PRINT_OPT_ENC   = 0x00000001,
    PRINT_OPT_DEC   = 0x00000002,
    PRINT_OPT_VPP   = 0x00000004,
    PRINT_OPT_AVC   = 0x00000010,
    PRINT_OPT_MPEG2 = 0x00000020,
    PRINT_OPT_VC1   = 0x00000040,
    PRINT_OPT_VP8   = 0x00000080,
    PRINT_OPT_JPEG  = 0x00000100,
};

struct _print_param{
    mfxU32 flags;
    std::string padding;
};
extern _print_param print_param;
#define INC_PADDING() print_param.padding += "  ";
#define DEC_PADDING() print_param.padding.erase(print_param.padding.size() - 2, 2);

typedef struct{
    unsigned char* data;
    unsigned int size;
} rawdata;

rawdata hexstr(void* d, unsigned int s);

#define DECL_STRUCT_TRACE(name) \
    std::ostream &operator << (std::ostream &os, name &p); \
    std::ostream &operator << (std::ostream &os, name* &p);

std::ostream &operator << (std::ostream &os, rawdata p);
std::ostream &operator << (std::ostream &os, std::string &p);
std::ostream &operator << (std::ostream &os, mfxU8* &p);
std::ostream &operator << (std::ostream &os, mfxStatus &st);
std::ostream &operator << (std::ostream &os, mfxInfoMFX &p);
std::ostream &operator << (std::ostream &os, mfxInfoVPP &p);
std::ostream &operator << (std::ostream &os, mfxFrameInfo &p);
std::ostream &operator << (std::ostream &os, mfxFrameId &p);
std::ostream &operator << (std::ostream &os, mfxSession* &p);
std::ostream &operator << (std::ostream &os, mfxSyncPoint* &p);
std::ostream &operator << (std::ostream &os, mfxFrameSurface1** &p);
std::ostream &operator << (std::ostream &os, mfxFrameData &p);

DECL_STRUCT_TRACE(mfxVideoParam);
DECL_STRUCT_TRACE(mfxVersion);
DECL_STRUCT_TRACE(mfxFrameAllocRequest);
DECL_STRUCT_TRACE(mfxBitstream);
DECL_STRUCT_TRACE(mfxFrameSurface1);
DECL_STRUCT_TRACE(mfxExtBuffer);
DECL_STRUCT_TRACE(mfxExtSVCSeqDesc);
DECL_STRUCT_TRACE(mfxExtSVCRateControl);
DECL_STRUCT_TRACE(mfxExtVPPDoNotUse);
DECL_STRUCT_TRACE(mfxExtVPPDenoise);
DECL_STRUCT_TRACE(mfxExtVPPDetail);
DECL_STRUCT_TRACE(mfxExtCodingOption2);
DECL_STRUCT_TRACE(mfxExtVPPProcAmp);
DECL_STRUCT_TRACE(mfxExtVppAuxData);
DECL_STRUCT_TRACE(mfxExtVPPDoUse);
DECL_STRUCT_TRACE(mfxExtVPPFrameRateConversion);
DECL_STRUCT_TRACE(mfxExtVPPImageStab);
DECL_STRUCT_TRACE(mfxExtMVCSeqDesc);
DECL_STRUCT_TRACE(mfxExtCodingOption);
DECL_STRUCT_TRACE(mfxExtOpaqueSurfaceAlloc);
DECL_STRUCT_TRACE(mfxPayload);
DECL_STRUCT_TRACE(mfxEncodeCtrl);
DECL_STRUCT_TRACE(mfxAES128CipherCounter);
DECL_STRUCT_TRACE(mfxExtPAVPOption);
DECL_STRUCT_TRACE(mfxEncryptedData);

#ifdef __MFXAUDIO_H__
DECL_STRUCT_TRACE(mfxAudioStreamInfo);
DECL_STRUCT_TRACE(mfxAudioInfoMFX);
DECL_STRUCT_TRACE(mfxAudioParam);
DECL_STRUCT_TRACE(mfxAudioAllocRequest);
#endif //#ifdef __MFXAUDIO_H__

#if ((MFX_VERSION_MAJOR >= 1) && (MFX_VERSION_MINOR >= 7))
DECL_STRUCT_TRACE(mfxExtEncoderCapability);
DECL_STRUCT_TRACE(mfxExtEncoderResetOption);
DECL_STRUCT_TRACE(mfxExtAVCEncodedFrameInfo);
#endif //#if ((MFX_VERSION_MAJOR >= 1) && (MFX_VERSION_MINOR >= 7))

DECL_STRUCT_TRACE(mfxExtAvcTemporalLayers);
DECL_STRUCT_TRACE(mfxExtCodingOptionSPSPPS);
DECL_STRUCT_TRACE(mfxExtVideoSignalInfo);
DECL_STRUCT_TRACE(mfxExtAVCRefListCtrl);

void allow_debug_output();
