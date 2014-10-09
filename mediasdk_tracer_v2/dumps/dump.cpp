#include "dump.h"

std::string StringToHexString(std::string data)
{
    std::ostringstream result;
    result << std::setw(2) << std::setfill('0') << std::hex << std::uppercase;
    std::copy(data.begin(), data.end(), std::ostream_iterator<unsigned int>(result, " "));
    return result.str();
}

struct BufferIdTable
{
    mfxU32 bufferid;
    const char* str;
};

#define TABLE_ENTRY(_name) \
    { _name, #_name }

static BufferIdTable g_BufferIdTable[] =
{
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),

    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION2),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION3),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_SPSPPS),

    TABLE_ENTRY(MFX_EXTBUFF_DEC_VIDEO_PROCESSING),

    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_CAPABILITY),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_RESET_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_ROI),

    TABLE_ENTRY(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION),

    TABLE_ENTRY(MFX_EXTBUFF_PICTURE_TIMING_SEI),

    TABLE_ENTRY(MFX_EXTBUFF_VPP_AUXDATA),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_COMPOSITE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DEINTERLACING),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DENOISE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DETAIL),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DONOTUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DOUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_PROCAMP),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_ANALYSIS),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_CHANGE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO),

    TABLE_ENTRY(MFX_EXTBUFF_VIDEO_SIGNAL_INFO),
};

const char* DumpContext::get_bufferid_str(mfxU32 bufferid)
{
    for (size_t i = 0; i < GET_ARRAY_SIZE(g_BufferIdTable); ++i)
    {
        if (g_BufferIdTable[i].bufferid == bufferid)
        {
            return g_BufferIdTable[i].str;
        }
    }
    return NULL;
}


//mfxcommon
DEFINE_GET_TYPE_DEF(mfxBitstream);
DEFINE_GET_TYPE_DEF(mfxExtBuffer);
DEFINE_GET_TYPE_DEF(mfxIMPL);
DEFINE_GET_TYPE_DEF(mfxPriority);
DEFINE_GET_TYPE_DEF(mfxVersion);
DEFINE_GET_TYPE_DEF(mfxSyncPoint);

//mfxenc
DEFINE_GET_TYPE_DEF(mfxENCInput);
DEFINE_GET_TYPE_DEF(mfxENCOutput);

//mfxplugin
DEFINE_GET_TYPE_DEF(mfxPlugin);

//mfxstructures
DEFINE_GET_TYPE_DEF(mfxDecodeStat);
DEFINE_GET_TYPE_DEF(mfxEncodeCtrl);
DEFINE_GET_TYPE_DEF(mfxEncodeStat);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption2);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption3);
DEFINE_GET_TYPE_DEF(mfxExtEncoderResetOption);
DEFINE_GET_TYPE_DEF(mfxExtVppAuxData);
DEFINE_GET_TYPE_DEF(mfxFrameAllocRequest);
DEFINE_GET_TYPE_DEF(mfxFrameData);
DEFINE_GET_TYPE_DEF(mfxFrameId);
DEFINE_GET_TYPE_DEF(mfxFrameInfo);
DEFINE_GET_TYPE_DEF(mfxFrameSurface1);
DEFINE_GET_TYPE_DEF(mfxHandleType);
DEFINE_GET_TYPE_DEF(mfxInfoMFX);
DEFINE_GET_TYPE_DEF(mfxInfoVPP);
DEFINE_GET_TYPE_DEF(mfxPayload);
DEFINE_GET_TYPE_DEF(mfxSkipMode);
DEFINE_GET_TYPE_DEF(mfxVideoParam);
DEFINE_GET_TYPE_DEF(mfxVPPStat);
DEFINE_GET_TYPE_DEF(mfxExtVPPDoNotUse);

//mfxsession
DEFINE_GET_TYPE_DEF(mfxSession);

//mfxvideo
DEFINE_GET_TYPE_DEF(mfxBufferAllocator);
DEFINE_GET_TYPE_DEF(mfxFrameAllocator);

