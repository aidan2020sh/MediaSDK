#include "dump.h"


std::string pVoidToHexString(void* x)
{
    std::ostringstream result;
    std::ostringstream tmp;
    tmp << std::hex <<std::uppercase << ((mfxU64)x);
    for (int i = 0; i < (16 - tmp.str().length()); i++)
        result << "0";
    result << tmp.str();
    return result.str();
}

struct IdTable
{
    mfxU32 id;
    const char* str;
};

#define TABLE_ENTRY(_name) \
    { _name, #_name }


static IdTable g_BufferIdTable[] =
{
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),

    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION2),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION3),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_SPSPPS),

//    TABLE_ENTRY(MFX_EXTBUFF_DEC_VIDEO_PROCESSING),

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

    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_STAT),

    TABLE_ENTRY(MFX_EXTBUFF_FEI_PARAM),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MV_PRED),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_QP),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MV),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MB),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MV_PRED),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MB),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MV),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MB_STAT),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PAK_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_SPS),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PPS),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_SLICE),
    TABLE_ENTRY(MFX_EXTBUF_CAM_GAMMA_CORRECTION),
    TABLE_ENTRY(MFX_EXTBUF_CAM_WHITE_BALANCE),
    TABLE_ENTRY(MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL),
    TABLE_ENTRY(MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION),
    TABLE_ENTRY(MFX_EXTBUF_CAM_VIGNETTE_CORRECTION),
    TABLE_ENTRY(MFX_EXTBUF_CAM_BAYER_DENOISE),
    TABLE_ENTRY(MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3),
    TABLE_ENTRY(MFX_EXTBUF_CAM_PADDING),
    TABLE_ENTRY(MFX_EXTBUF_CAM_PIPECONTROL),
    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_STAT),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLISTS),
    TABLE_ENTRY(MFX_EXTBUFF_JPEG_QT),
    TABLE_ENTRY(MFX_EXTBUFF_JPEG_HUFFMAN),
    TABLE_ENTRY(MFX_EXTBUFF_MVC_SEQ_DESC),
    TABLE_ENTRY(MFX_EXTBUFF_MVC_TARGET_VIEWS),
};

static IdTable tbl_impl[] = {
    TABLE_ENTRY(MFX_IMPL_SOFTWARE),
    TABLE_ENTRY(MFX_IMPL_HARDWARE),
    TABLE_ENTRY(MFX_IMPL_AUTO_ANY),
    TABLE_ENTRY(MFX_IMPL_HARDWARE_ANY),
    TABLE_ENTRY(MFX_IMPL_HARDWARE2),
    TABLE_ENTRY(MFX_IMPL_HARDWARE3),
    TABLE_ENTRY(MFX_IMPL_HARDWARE4),
    TABLE_ENTRY(MFX_IMPL_RUNTIME),
    TABLE_ENTRY(MFX_IMPL_VIA_ANY),
    TABLE_ENTRY(MFX_IMPL_VIA_D3D9),
    TABLE_ENTRY(MFX_IMPL_VIA_D3D11),
    TABLE_ENTRY(MFX_IMPL_VIA_VAAPI),
    TABLE_ENTRY(MFX_IMPL_AUDIO)
};

static IdTable tbl_fourcc[] = {
    TABLE_ENTRY(MFX_FOURCC_NV12),
    TABLE_ENTRY(MFX_FOURCC_YV12),
    TABLE_ENTRY(MFX_FOURCC_NV16),
    TABLE_ENTRY(MFX_FOURCC_YUY2),
    TABLE_ENTRY(MFX_FOURCC_RGB3),
    TABLE_ENTRY(MFX_FOURCC_RGB4),
    TABLE_ENTRY(MFX_FOURCC_P8),
    TABLE_ENTRY(MFX_FOURCC_P8_TEXTURE),
    TABLE_ENTRY(MFX_FOURCC_P010),
    TABLE_ENTRY(MFX_FOURCC_P210),
    TABLE_ENTRY(MFX_FOURCC_BGR4),
    TABLE_ENTRY(MFX_FOURCC_A2RGB10),
    TABLE_ENTRY(MFX_FOURCC_ARGB16),
    TABLE_ENTRY(MFX_FOURCC_R16)
};

static IdTable tbl_sts[] = {
    TABLE_ENTRY(MFX_ERR_NONE),
    TABLE_ENTRY(MFX_ERR_UNKNOWN),
    TABLE_ENTRY(MFX_ERR_NULL_PTR),
    TABLE_ENTRY(MFX_ERR_UNSUPPORTED),
    TABLE_ENTRY(MFX_ERR_MEMORY_ALLOC),
    TABLE_ENTRY(MFX_ERR_NOT_ENOUGH_BUFFER),
    TABLE_ENTRY(MFX_ERR_INVALID_HANDLE),
    TABLE_ENTRY(MFX_ERR_LOCK_MEMORY),
    TABLE_ENTRY(MFX_ERR_NOT_INITIALIZED),
    TABLE_ENTRY(MFX_ERR_NOT_FOUND),
    TABLE_ENTRY(MFX_ERR_MORE_DATA),
    TABLE_ENTRY(MFX_ERR_MORE_SURFACE),
    TABLE_ENTRY(MFX_ERR_ABORTED),
    TABLE_ENTRY(MFX_ERR_DEVICE_LOST),
    TABLE_ENTRY(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM),
    TABLE_ENTRY(MFX_ERR_INVALID_VIDEO_PARAM),
    TABLE_ENTRY(MFX_ERR_UNDEFINED_BEHAVIOR),
    TABLE_ENTRY(MFX_ERR_DEVICE_FAILED),
    TABLE_ENTRY(MFX_ERR_MORE_BITSTREAM),
    TABLE_ENTRY(MFX_ERR_INCOMPATIBLE_AUDIO_PARAM),
    TABLE_ENTRY(MFX_ERR_INVALID_AUDIO_PARAM),
    TABLE_ENTRY(MFX_WRN_IN_EXECUTION),
    TABLE_ENTRY(MFX_WRN_DEVICE_BUSY),
    TABLE_ENTRY(MFX_WRN_VIDEO_PARAM_CHANGED),
    TABLE_ENTRY(MFX_WRN_PARTIAL_ACCELERATION),
    TABLE_ENTRY(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
    TABLE_ENTRY(MFX_WRN_VALUE_NOT_CHANGED),
    TABLE_ENTRY(MFX_WRN_OUT_OF_RANGE),
    TABLE_ENTRY(MFX_WRN_FILTER_SKIPPED),
    TABLE_ENTRY(MFX_WRN_INCOMPATIBLE_AUDIO_PARAM),
    TABLE_ENTRY(MFX_TASK_DONE),
    TABLE_ENTRY(MFX_TASK_WORKING),
    TABLE_ENTRY(MFX_TASK_BUSY)
};

static IdTable tbl_codecid[] = {
    TABLE_ENTRY(MFX_CODEC_AVC),
    TABLE_ENTRY(MFX_CODEC_HEVC),
    TABLE_ENTRY(MFX_CODEC_MPEG2),
    TABLE_ENTRY(MFX_CODEC_VC1),
    TABLE_ENTRY(MFX_CODEC_CAPTURE)
};


const char* DumpContext::get_bufferid_str(mfxU32 bufferid)
{
    for (size_t i = 0; i < GET_ARRAY_SIZE(g_BufferIdTable); ++i)
    {
        if (g_BufferIdTable[i].id == bufferid)
        {
            return g_BufferIdTable[i].str;
        }
    }
    return NULL;
}

std::string GetmfxIMPL(mfxIMPL impl) {
    
    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
        
    for (int i = 0; i < (sizeof(tbl_impl) / sizeof(tbl_impl[0])); i++)
        if (tbl_impl[i].id == (impl & (MFX_IMPL_VIA_ANY - 1)))
            name = tbl_impl[i].str;
    stream << name;
    
    int via_flag = impl & ~(MFX_IMPL_VIA_ANY - 1);

    if (0 != via_flag)
    {
        stream << "|";
        name = "UNKNOWN";
        for (int i = 0; i < (sizeof(tbl_impl) / sizeof(tbl_impl[0])); i++)
            if (tbl_impl[i].id == via_flag)
                name = tbl_impl[i].str;
        stream << name;
    }
    return stream.str();
}

std::string GetFourCC(mfxU32 fourcc) {
    
    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
    int j = 0;
    for (int i = 0; i < (sizeof(tbl_fourcc) / sizeof(tbl_fourcc[0])); i++)
    {
        if (tbl_fourcc[i].id == fourcc)
        {
            name = "";
            while (tbl_fourcc[i].str[j + 11] != '\0')
            {
                name = name + tbl_fourcc[i].str[j + 11];
                j++;
            }
            name = name + "\0";
            break;
        }
    }
    stream<<name;
    return stream.str();
}

std::string GetStatusString(mfxStatus sts) {
    
    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN_STATUS";
    for (int i = 0; i < (sizeof(tbl_sts) / sizeof(tbl_sts[0])); i++)
    {
        if (tbl_sts[i].id == sts)
        {
            name = tbl_sts[i].str;
            break;
        }

    }
    stream<<name;
    return stream.str();
}

std::string GetCodecIdString(mfxU32 id) {
    
    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
    for (int i = 0; i < (sizeof(tbl_codecid) / sizeof(tbl_codecid[0])); i++)
    {
        if (tbl_codecid[i].id == id)
        {
            name = tbl_codecid[i].str;
            break;
        }

    }
    stream<<name;
    return stream.str();
}

//mfxcommon
DEFINE_GET_TYPE_DEF(mfxBitstream);
DEFINE_GET_TYPE_DEF(mfxExtBuffer);
DEFINE_GET_TYPE_DEF(mfxIMPL);
DEFINE_GET_TYPE_DEF(mfxInitParam);
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

//mfxfei
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncMV);
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncMBStat);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncFrameCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMVPredictors);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncQP);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMV);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMBStat);
DEFINE_GET_TYPE_DEF(mfxFeiPakMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiPakMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiParam);
DEFINE_GET_TYPE_DEF(mfxPAKInput);
DEFINE_GET_TYPE_DEF(mfxPAKOutput);
DEFINE_GET_TYPE_DEF(mfxExtFeiSPS);
DEFINE_GET_TYPE_DEF(mfxExtFeiPPS);
DEFINE_GET_TYPE_DEF(mfxExtFeiSliceHeader);


