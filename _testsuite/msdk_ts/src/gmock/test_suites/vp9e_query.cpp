﻿/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfx_ext_buffers.h"
#include "gmock/test_suites/vp9e_utils.h"
#include "mfxpavp.h"

namespace vp9e_query
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_VP9){}
    ~TestSuite() { }

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        mfxU32 sub_type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    template<mfxU32 fourcc>
    int RunTest_Subtype(const unsigned int id);

    int RunTest(const tc_struct& tc, unsigned int fourcc_id);
    static const unsigned int n_cases;

    static const unsigned int n_cases_nv12;
    static const unsigned int n_cases_p010;
    static const unsigned int n_cases_ayuv;
    static const unsigned int n_cases_y410;

private:

    enum
    {
        MFX_PAR,
        SESSION_NULL,
        TARGET_USAGE,
        NONE,
        RESOLUTION,
        MAX_RESOLUTION,
        MAX_RESOLUTION_PLUS_1,
        NOT_ALLIGNED,
        SET_ALLOCK,
        AFTER,
        EXT_BUFF,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        IO_PATTERN,
        MEMORY_TYPE,
        FRAME_RATE,
        PIC_STRUCT,
        CROP,
        XY,
        W,
        H,
        YH,
        XW,
        WH,
        PROTECTED,
        INVALID,
        NONCONFIGURABLE_PARAM,
        PRIVATE_DDI
    };

    static const tc_struct test_case[];
};

#define VP9_VDENC_MAX_RESOL_CNL (4096)
#define VP9_VDENC_MAX_RESOL_ICL (7680)
#define VP9E_MAX_QP_VALUE (255)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //TU
    {/*00 Target usage 1 'Best Quality'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1} },
    {/*01 Target usage 2*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
    {/*02 Target usage 3*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
    {/*03 Target usage 4 'Balanced'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
    {/*04 Target usage 5*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
    {/*05 Target usage 6*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
    {/*06 Target usage 7 'Best Speed'*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },

    //ExtBuf
    {/*07*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, {} },

    //BRC
    {/*08*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
    {/*09*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
    {/*10*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
    {/*11*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
    {/*12*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
    {/*13*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
    {/*14*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
    {/*15*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
    {/*16*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
    {/*17*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
    {/*18*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 16 } },
    {/*19 'BufferSizeInKB' smaller than 'InitialDelayInKB'*/ MFX_ERR_UNSUPPORTED, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 20 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 10 },
        }
    },
    {/*20 QPs out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, VP9E_MAX_QP_VALUE + 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, VP9E_MAX_QP_VALUE + 1 },
        }
    },

    //Session
    {/*21*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },

    //Set allock handle
    {/*22*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*23*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*24*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },

    //Zero params
    {/*25*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
    {/*26*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },

    //IOPattern
    {/*27*/ MFX_ERR_NONE, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
    {/*28*/ MFX_ERR_NONE, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
    {/*29*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, MEMORY_TYPE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },

    //FourCC
    {/*30*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 } },
    {/*31*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV16 } },
    {/*32*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2 } },
    {/*33*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB3 } },
    {/*34*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4 } },
    {/*35*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8_TEXTURE } },
    {/*36*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P016 } },
    {/*37*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P210 } },
    {/*38*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_A2RGB10 } },
    {/*39*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_ARGB16 } },
    {/*40*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_ABGR16 } },
    {/*41*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_R16 } },
    {/*42*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_AYUV_RGB4 } },
    {/*43*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_UYVY } },
    {/*44*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y210 } },
    {/*45*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y216 } },
    {/*46*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_Y416 } },

    //Resolution
    {/*47 Check zero resolution*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
        }
    },
    {/*48 Height is 0*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 }
        }
    },
    {/*49 Width is 0*/ MFX_ERR_NONE, RESOLUTION, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 }
        }
    },
    {/*50 Check maximum supported resolution*/ MFX_ERR_NONE, MAX_RESOLUTION, NONE,
    {
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9_VDENC_MAX_RESOL_CNL },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9_VDENC_MAX_RESOL_CNL }
    }
    },
    {/*51 Check too big resolution*/ MFX_ERR_UNSUPPORTED, MAX_RESOLUTION_PLUS_1, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9_VDENC_MAX_RESOL_CNL + 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9_VDENC_MAX_RESOL_CNL + 1 }
        }
    },

    //PicStruct
    {/*52*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
    {/*53*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
    {/*54*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
    {/*55*/ MFX_ERR_UNSUPPORTED, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },

    //Crops
    {/*56 Invalid crop region*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
    {/*57 Invalid crop region*/ MFX_ERR_UNSUPPORTED, CROP, XY, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
    {/*58 Valid crop region*/ MFX_ERR_NONE, CROP, W, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 } },
    {/*59 Valid crop region*/ MFX_ERR_NONE, CROP, H, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 } },
    {/*60 Crop region exceeds frame size*/ MFX_ERR_UNSUPPORTED, CROP, WH,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 + 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 + 10 }
        }
    },
    {/*61 Check crops*/ MFX_ERR_UNSUPPORTED, CROP, XW,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
        }
    },
    {/*62 Check crops*/ MFX_ERR_UNSUPPORTED, CROP, YH,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
        }
    },
    {/*63 Valid crop region*/ MFX_ERR_NONE, CROP, W,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 - 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 },
        }
    },
    {/*64 Valid crop region*/ MFX_ERR_NONE, CROP, H,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
        }
    },

    //Frame rate
    {/*65*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
    {/*66*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },

    //GOP params
    {/*67*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 } },
    {/*68*/ MFX_ERR_NONE, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0xffff } },
    {/*69*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
    {/*70*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0xffff } },
    {/*71 non-configurable param (silent ignore expected)*/ MFX_ERR_NONE, NONCONFIGURABLE_PARAM, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },

    //Protection (currently unsupported)
    {/*72*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
    {/*73*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
    {/*74*/ MFX_ERR_UNSUPPORTED, PROTECTED, INVALID, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },

    //Async depth
    {/*75*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1 } },
    {/*76*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 10 } },
    {/*77*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 0xffff } },

    //Chroma format
    {/*78 Invalid chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
    {/*79 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME } },
    {/*80 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422 } },
    {/*81 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV400 } },
    {/*82 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411 } },
    {/*83 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422H } },
    {/*84 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V } },
    {/*85 Unsupported chroma format*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 } },

    //Format definitions
    {/*86 Bit depth luma is not set*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 } },
    {/*87 Bit depth luma is wrong*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 9 } },
    {/*88 Bit depth chroma is not set*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 } },
    {/*89 Bit depth chroma is wrong*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 9 } },
    {/*90 Bit depths and chroma format are 0, but '0' is a value for ChromaFormat (=MONOCHROME)*/ MFX_ERR_UNSUPPORTED, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 0 },
        }
    },
    {/*91 Bit depths and Fourcc are not set: OK for query*/ MFX_ERR_NONE, NONE, NONE,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },
        }
    },

    // Reference frames
    {/*92*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 } },
    {/*93*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 } },
    {/*94*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 } },
    {/*95 out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 4 } },
    {/*96 out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 0xffff } },

    // LowPower
    {/*97 out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ADAPTIVE } },
    {/*98 out of range*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, 0xffff } },
    {/*99 default value*/ MFX_ERR_NONE, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_UNKNOWN } },
    {/*100 off-unsupported value*/ MFX_ERR_UNSUPPORTED, NONE, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF } },

    // IVF-headers check
    {/*101 explicitly enabled IVF-headers*/ MFX_ERR_NONE, NONE, NONE,
        { MFX_PAR, &tsStruct::mfxExtVP9Param.WriteIVFHeaders, MFX_CODINGOPTION_ON }
    },
    {/*102 explicitly disabled IVF-headers*/ MFX_ERR_NONE, NONE, NONE,
        { MFX_PAR, &tsStruct::mfxExtVP9Param.WriteIVFHeaders, MFX_CODINGOPTION_OFF }
    },
    {/*103 check default state for IVF-headers [=enabled]*/ MFX_ERR_NONE, NONE, NONE,
        { MFX_PAR, &tsStruct::mfxExtVP9Param.WriteIVFHeaders, MFX_CODINGOPTION_UNKNOWN }
    },

    // Check error on unsupportd ext-buf
    {/*104 */ MFX_ERR_UNSUPPORTED, NONE, NONE,
        { MFX_PAR, &tsStruct::mfxExtHEVCTiles.NumTileRows, 2 }
    },

    //Check RefreshFrameContext option
    {/*105 */ MFX_ERR_NONE, EXT_BUFF, PRIVATE_DDI }


};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

const unsigned int TestSuite::n_cases_nv12 = n_cases;
const unsigned int TestSuite::n_cases_p010 = n_cases;
const unsigned int TestSuite::n_cases_ayuv = n_cases;
const unsigned int TestSuite::n_cases_y410 = n_cases;

template<mfxU32 fourcc>
int TestSuite::RunTest_Subtype(const unsigned int id)
{
    const tc_struct& tc = test_case[id];
    return RunTest(tc, fourcc);
}

int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
{
    TS_START;

    mfxHDL hdl;
    mfxHandleType type;
    mfxStatus sts;

    m_pParOut = new mfxVideoParam;

    MFXInit();

    Load();

    m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;

    SetFrameInfo(m_par.mfx.FrameInfo, fourcc_id);
    SETPARS(m_par, MFX_PAR);

    tsExtBufType<mfxVideoParam> m_par_out;
    m_par_out.mfx.CodecId = m_par.mfx.CodecId;
    m_pParOut = &m_par_out;

    if (m_par.NumExtParam)
    {
        for (mfxU8 i = 0; i < m_par.NumExtParam; i++)
        {
            mfxExtBuffer *pInBuf = m_par.ExtParam[i];
            if (pInBuf)
            {
                m_par_out.AddExtBuffer(pInBuf->BufferId, m_par.ExtParam[i]->BufferSz);
            }
        }
    }

    if (!GetAllocator())
    {
        if (m_pVAHandle)
            SetAllocator(m_pVAHandle, true);
        else
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
    }

    if ((tc.type == SET_ALLOCK) && (tc.sub_type != AFTER)
         && (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle)))
    {
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator();

        if (!m_is_handle_set)
        {
            m_pFrameAllocator->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }
    }

    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
    {
        // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
        if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
            || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
        }
    }
    else {
        g_tsLog << "WARNING: loading encoder from plugin failed!\n";
        return 0;
    }

    if (tc.type == EXT_BUFF)
    {
        m_pPar->NumExtParam = 1;
        m_pParOut->NumExtParam = 1;
        m_pPar->ExtParam = NULL;
        m_pParOut->ExtParam = NULL;
    }

    if (g_tsHWtype >= MFX_HW_ICL)
    {
        if (tc.type == MAX_RESOLUTION)
        {
            m_par.mfx.FrameInfo.Width = VP9_VDENC_MAX_RESOL_ICL;
            m_par.mfx.FrameInfo.Height = VP9_VDENC_MAX_RESOL_ICL;
        }
        else if (tc.type == MAX_RESOLUTION_PLUS_1)
        {
            m_par.mfx.FrameInfo.Width = VP9_VDENC_MAX_RESOL_ICL + 1;
            m_par.mfx.FrameInfo.Height = VP9_VDENC_MAX_RESOL_ICL + 1;
        }
    }

    g_tsStatus.expect(tc.sts);

    if (tc.type == EXT_BUFF)
    {
        if (tc.sub_type == NONE)
            sts = MFX_ERR_NULL_PTR;
        else
            if (tc.sts != MFX_ERR_NONE)
                sts = MFX_ERR_UNSUPPORTED;
    }

    if (tc.sub_type == PRIVATE_DDI)
    {
        m_par.AddExtBuffer(MFX_EXTBUFF_DDI, sizeof(mfxExtCodingOptionDDI));
        m_par_out.AddExtBuffer(MFX_EXTBUFF_DDI, sizeof(mfxExtCodingOptionDDI));
        mfxExtCodingOptionDDI* m_extDDI = (mfxExtCodingOptionDDI*)m_par.GetExtBuffer(MFX_EXTBUFF_DDI);
        m_extDDI->RefreshFrameContext = MFX_CODINGOPTION_ON;
    }

    if (tc.type == IN_PAR_NULL)
    {
        m_pPar = NULL;
    }
    if (tc.type == OUT_PAR_NULL)
    {
        m_pParOut = NULL;
    }

    //call tested function
    if (tc.type != SESSION_NULL)
        sts = Query(m_session, m_pPar, m_pParOut);
    else
        sts = Query(NULL, m_pPar, m_pParOut);

    if (sts == MFX_ERR_NONE && tc.type != IN_PAR_NULL)
    {
        if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CBR || m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_VBR)
        {
            // For some BRC methods Query sets BRCParamMultiplier for the output when it is '0' on the input (without warning)
            m_pPar->mfx.BRCParamMultiplier = 1;
        }

        // Only 1, 4, 7 TUs are supported - the rest are mapped to the nearest value (without warning)
        if (m_pPar->mfx.TargetUsage == 2)
        {
            m_pPar->mfx.TargetUsage = 1;
        }
        else if (m_pPar->mfx.TargetUsage == 3)
        {
            m_pPar->mfx.TargetUsage = 4;
        }
        else if (m_pPar->mfx.TargetUsage == 5)
        {
            m_pPar->mfx.TargetUsage = 4;
        }
        else if (m_pPar->mfx.TargetUsage == 6)
        {
            m_pPar->mfx.TargetUsage = 7;
        }

        // Struct compare stage
        {
            mfxVideoParam cmp_copy_in = *m_pPar;
            mfxVideoParam cmp_copy_out = *m_pParOut;

            // ExtParam ptr is zeroed for temp structs because it is always different and breaks byte-to-byte compare
            if (cmp_copy_in.ExtParam)
                cmp_copy_in.ExtParam = nullptr;
            if (cmp_copy_out.ExtParam)
                cmp_copy_out.ExtParam = nullptr;

            int cmp_result = memcmp(&cmp_copy_in, &cmp_copy_out, sizeof(mfxVideoParam));

            int cmp_result_ext_buf = 0;
            for (mfxU8 i = 0; i < m_par.NumExtParam; i++)
            {
                mfxExtBuffer *pInBuf = m_par.ExtParam[i];
                tsExtBufType<mfxVideoParam> m_temp_par = m_par;
                void *buf_ptr_par = m_temp_par.GetExtBuffer(pInBuf->BufferId);
                tsExtBufType<mfxVideoParam> m_temp_par_out = m_par_out;
                void *buf_ptr_par_out = m_temp_par_out.GetExtBuffer(pInBuf->BufferId);

                if (buf_ptr_par && !buf_ptr_par_out)
                {
                    ADD_FAILURE() << "ERROR: Query() one of the ext_buffers presents in In but not found in Out"; throw tsFAIL;
                }

                cmp_result_ext_buf = memcmp(buf_ptr_par, buf_ptr_par_out, m_par.ExtParam[i]->BufferSz);
                if (cmp_result_ext_buf)
                {
                    break;
                }
            }

            if (!tc.type == (bool)NONCONFIGURABLE_PARAM && (cmp_result != 0 || cmp_result_ext_buf != 0))
            {
                TS_TRACE(*m_pPar);
                TS_TRACE(*m_pParOut);
                ADD_FAILURE() << "ERROR: In and Out structs are different (expected be the same for this case)"; throw tsFAIL;
            }
            else if (tc.type == (bool)NONCONFIGURABLE_PARAM && cmp_result == 0)
            {
                TS_TRACE(*m_pPar);
                TS_TRACE(*m_pParOut);
                ADD_FAILURE() << "ERROR: Query() unsupported param provided, but In and Out structs are the same (expected cleaning out the param)"; throw tsFAIL;
            }
        }
    }

    if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
    {
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator();

        if (!m_is_handle_set)
        {
            m_pFrameAllocator->get_hdl(type, hdl);

            if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
                g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps
            SetHandle(m_session, type, hdl);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_query,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_query, RunTest_Subtype<MFX_FOURCC_P010>, n_cases_p010);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_query,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases_ayuv);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_query, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases_y410);
}
