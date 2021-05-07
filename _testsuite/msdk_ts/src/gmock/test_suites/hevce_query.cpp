/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2020 Intel Corporation. All Rights Reserved.

File Name: hevce_query.cpp

\* ****************************************************************************** */


#include "ts_encoder.h"
#include "ts_struct.h"

#if defined(MFX_ONEVPL)
#include "mfxpavp.h"
#endif

namespace hevce_query
{

#define INVALID_WIDTH   16384
#define INVALID_HEIGHT  16384

    enum
    {
        MFX_PAR,
        TARGET_USAGE,
        EXT_BUFF,
        SESSION_NULL,
        SET_ALLOCK,
        AFTER,
        IN_PAR_NULL,
        OUT_PAR_NULL,
        RESOLUTION,
        DELTA,  // param depends on resolution
        CROP,
        IO_PATTERN,
        FRAME_RATE,
        PIC_STRUCT,
        PROTECTED,
        W_GT_MAX,
        H_GT_MAX,
        NONE,
        ALIGNMENT_HW,
        Compatibility,
        BitDepth,
        ChromaFormat,
        PBRC,
        ICQ,
        QPRange,
        NumReference,
        EXT_CO2,
        EXT_CO3,
        MFX_FRAMEINFO
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        mfxU32 sub_type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxI32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC){}
        ~TestSuite() { }

        static const unsigned int n_cases;

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:

        static const tc_struct test_case[];

        mfxU32 AlignResolutionByPlatform(mfxU16 value)
        {
            mfxU32 alignment = g_tsHWtype >= MFX_HW_ICL ? 8 : 16;
            return (value + (alignment - 1)) & ~(alignment - 1);
        }

        mfxU32 AlignSurfaceSize(mfxU16 value)
        {
            mfxU32 surfAlignemnet = 16;
            return (value + (surfAlignemnet - 1)) & ~(surfAlignemnet - 1);
        }

        bool IsHevcHwPlugin()
        {
            return 0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data));
        }

        bool IsHevcGaccPlugin()
        {
            return 0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data));
        }

        void CheckOutPar(tc_struct tc)
        {
            switch (tc.type)
            {
                case TARGET_USAGE:
                {
                    if (IsHevcHwPlugin())
                    {
                        // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
                        if (tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)
                        {
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                        }
                        else
                        {
                            if (tc.set_par[0].v % 3 == 0)
                                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f) + 1);
                            else
                                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f) - 1);
                        }
                    }
                    else if (IsHevcGaccPlugin())
                    {
                        // GACC: supported only TU = {4,5,6,7}
                        if (tc.set_par[0].v >= 4)
                        {
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                        }
                        else
                        {
                            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, 4);
                        }
                    }
                    else // SW: supported TU = {1,2,3,4,5,6,7}
                    {
                        tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
                    }
                    break;
                }
                case ALIGNMENT_HW:
                    if (IsHevcHwPlugin())
                    {
                        mfxExtBuffer* hp;
                        GetBuffer(MFX_EXTBUFF_HEVC_PARAM, m_pPar, &hp);
                        EXPECT_NE_THROW(nullptr, hp, "ERROR: failed to get resulted stream resolution");

                        mfxExtBuffer* hpOut;
                        GetBuffer(MFX_EXTBUFF_HEVC_PARAM, m_pParOut, &hpOut);
                        EXPECT_NE_THROW(nullptr, hpOut, "ERROR: failed to get resulted stream resolution");
                        //check width
                        EXPECT_EQ(AlignResolutionByPlatform(((mfxExtHEVCParam*)hp)->PicWidthInLumaSamples), ((mfxExtHEVCParam*)hpOut)->PicWidthInLumaSamples);
                        tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.Width, AlignSurfaceSize(m_pPar->mfx.FrameInfo.Width));
                        //check height
                        EXPECT_EQ(AlignResolutionByPlatform(((mfxExtHEVCParam*)hp)->PicHeightInLumaSamples), ((mfxExtHEVCParam*)hpOut)->PicHeightInLumaSamples);
                        tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.Height, AlignSurfaceSize(m_pPar->mfx.FrameInfo.Height));
                        //check crops
                        tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, m_pPar->mfx.FrameInfo.CropW);
                        tsStruct::check_eq(m_pParOut, tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, m_pPar->mfx.FrameInfo.CropH);
                    }
                    break;
                default: break;
            }
        }

        void GetBuffer(mfxU32 buff_id, const mfxVideoParam* par, mfxExtBuffer** buff)
        {
            *buff = nullptr;
            for (int i = 0; i < sizeof(par->ExtParam) / sizeof(mfxExtBuffer*); i++)
                if (par->ExtParam[i]->BufferId == buff_id)
                    *buff = par->ExtParam[i];
        }

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff, mfxExtBuffer** buff_out)
        {
                switch (buff_id)
                {
                case MFX_EXTBUFF_HEVC_PARAM:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtHEVCParam());
                        memset((*buff), 0, sizeof(mfxExtHEVCParam));
                        (*buff)->BufferId = MFX_EXTBUFF_HEVC_PARAM;
                        (*buff)->BufferSz = sizeof(mfxExtHEVCParam);
                        break;
                }
                case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtCodingOptionSPSPPS());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOptionSPSPPS());
                        memset((*buff), 0, sizeof(mfxExtCodingOptionSPSPPS));
                        (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
                        (*buff)->BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
                        break;
                }
                case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtVideoSignalInfo());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtVideoSignalInfo());
                        memset((*buff), 0, sizeof(mfxExtVideoSignalInfo));
                        (*buff)->BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
                        (*buff)->BufferSz = sizeof(mfxExtVideoSignalInfo);
                        break;
                }
                case MFX_EXTBUFF_CODING_OPTION:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtCodingOption());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOption());
                        memset((*buff), 0, sizeof(mfxExtCodingOption));
                        (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION;
                        (*buff)->BufferSz = sizeof(mfxExtCodingOption);
                        break;
                }
                case MFX_EXTBUFF_CODING_OPTION2:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtCodingOption2());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtCodingOption2());
                        memset((*buff), 0, sizeof(mfxExtCodingOption2));
                        (*buff)->BufferId = MFX_EXTBUFF_CODING_OPTION2;
                        (*buff)->BufferSz = sizeof(mfxExtCodingOption2);
                        break;
                }
                case MFX_EXTBUFF_VPP_DOUSE:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtVPPDoUse());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtVPPDoUse());
                        memset((*buff), 0, sizeof(mfxExtVPPDoUse));
                        (*buff)->BufferId = MFX_EXTBUFF_VPP_DOUSE;
                        (*buff)->BufferSz = sizeof(mfxExtVPPDoUse);
                        break;
                }
                case MFX_EXTBUFF_VPP_AUXDATA:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtVppAuxData());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtVppAuxData());
                        memset((*buff), 0, sizeof(mfxExtVppAuxData));
                        (*buff)->BufferId = MFX_EXTBUFF_VPP_AUXDATA;
                        (*buff)->BufferSz = sizeof(mfxExtVppAuxData);
                        break;
                }
                case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtAVCRefListCtrl());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtAVCRefListCtrl());
                        memset((*buff), 0, sizeof(mfxExtAVCRefListCtrl));
                        (*buff)->BufferId = MFX_EXTBUFF_AVC_REFLIST_CTRL;
                        (*buff)->BufferSz = sizeof(mfxExtAVCRefListCtrl);
                        break;
                }
                case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtVPPFrameRateConversion());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtVPPFrameRateConversion());
                        memset((*buff), 0, sizeof(mfxExtVPPFrameRateConversion));
                        (*buff)->BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
                        (*buff)->BufferSz = sizeof(mfxExtVPPFrameRateConversion);
                        break;
                }
                case MFX_EXTBUFF_PICTURE_TIMING_SEI:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtPictureTimingSEI());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtPictureTimingSEI());
                        memset((*buff), 0, sizeof(mfxExtPictureTimingSEI));
                        (*buff)->BufferId = MFX_EXTBUFF_PICTURE_TIMING_SEI;
                        (*buff)->BufferSz = sizeof(mfxExtPictureTimingSEI);
                        break;
                }
                case MFX_EXTBUFF_ENCODER_CAPABILITY:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtEncoderCapability());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtEncoderCapability());
                        memset((*buff), 0, sizeof(mfxExtEncoderCapability));
                        (*buff)->BufferId = MFX_EXTBUFF_ENCODER_CAPABILITY;
                        (*buff)->BufferSz = sizeof(mfxExtEncoderCapability);
                        break;
                }
                case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                {
                        (*buff) = (mfxExtBuffer*)(new mfxExtEncoderResetOption());
                        (*buff_out) = (mfxExtBuffer*)(new mfxExtEncoderResetOption());
                        memset((*buff), 0, sizeof(mfxExtEncoderResetOption));
                        (*buff)->BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
                        (*buff)->BufferSz = sizeof(mfxExtEncoderResetOption);
                        break;
                }
                case NONE:
                {
                        *buff = NULL;
                        *buff_out = NULL;
                        break;
                }
                default: break;
                }
            }
    };

    bool IsChromaFormatSupported(mfxU16 chromaFormat, mfxU16 bitDepthLuma)
    {
        switch (bitDepthLuma)
        {
        case 10:
            if (g_tsHWtype < MFX_HW_KBL)
                return false;
        case 8:
            if (g_tsHWtype < MFX_HW_SKL)
                return false;
            if (g_tsHWtype < MFX_HW_ICL
                && (chromaFormat == MFX_CHROMAFORMAT_YUV422
                    || chromaFormat == MFX_CHROMAFORMAT_YUV444))
                return false;
            break;
        case 12:
            if (g_tsHWtype < MFX_HW_TGL)
                return false;
            break;
        default:
            return false;
        }
        return true;
    }

    const tc_struct TestSuite::test_case[] =
    {
        //Target Usage
        {/*00*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}},
        {/*01*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2 } },
        {/*02*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3 } },
        {/*03*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 } },
        {/*04*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5 } },
        {/*05*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6 } },
        {/*06*/ MFX_ERR_NONE, TARGET_USAGE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 } },
        //Ext Buffers
        {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, {} },
        {/*08*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, {} },
        {/*09*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, {} },
        {/*10*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, {} },
        {/*11*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, {} },
        {/*12*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, {} },
        {/*13*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {} },
        {/*14*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, {} },
        {/*15*/ MFX_ERR_UNSUPPORTED, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, {} },
        {/*16*/ MFX_ERR_NULL_PTR, EXT_BUFF, NONE, {} },
        //Rate Control Metod
        {/*17*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*18*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*19*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*20*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
        {/*21*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*22*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED1 } },
        {/*23*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED2 } },
        {/*24*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED3 } },
        {/*25*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_RESERVED4 } },
        {/*26*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*27*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ } },
        //session
        {/*29*/ MFX_ERR_INVALID_HANDLE, SESSION_NULL, NONE, {} },
        //set alloc handle
        {/*30*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
        {/*31*/ MFX_ERR_NONE, SET_ALLOCK, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        {/*32*/ MFX_ERR_NONE, SET_ALLOCK, AFTER, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        //zero param
        {/*33*/ MFX_ERR_NONE, IN_PAR_NULL, NONE, {} },
        {/*34*/ MFX_ERR_NULL_PTR, OUT_PAR_NULL, NONE, {} },
        //IOPattern
        {/*35*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY } },
        {/*36*/ MFX_ERR_NONE, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY } },
        {/*37*/ MFX_ERR_UNSUPPORTED, IO_PATTERN, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, 0x8000 } },
        //Unsupported FourCC
        {/*38*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
                                                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Shift, 0 },
                                                 }
        },
        //Resolution
        {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 3 } },
        {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 3 } },
        {/*42*/ MFX_ERR_UNSUPPORTED, RESOLUTION, W_GT_MAX, {} },
        {/*43*/ MFX_ERR_UNSUPPORTED, RESOLUTION, H_GT_MAX, {} },
        {/*44*/ MFX_ERR_UNSUPPORTED, RESOLUTION, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                         { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                       }
        },
        //PicStruct
        {/*45*/ MFX_ERR_NONE, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
        {/*46*/ MFX_ERR_NONE, PIC_STRUCT, NONE,{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
        {/*47*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
        {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 } },
        {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0x11111111 } },
        //Crops
        {/*50*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 20 } },
        {/*51*/ MFX_ERR_UNSUPPORTED, CROP, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 20 } },
        {/*52*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 } },
        {/*53*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 } },
        {/*54*/ MFX_ERR_UNSUPPORTED, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 10 },
                                                    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 10 }
                                                  }
        },
        {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
                                                               }
        },
        {/*56*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
                                                               }
        },
        {/*57*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                                                               }
        },
        {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CROP, DELTA, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                                                               }
        },
        //frame rate
        {/*59*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 0 } },
        {/*60*/ MFX_ERR_UNSUPPORTED, FRAME_RATE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 0 } },
        //chroma format
        {/*61*/ MFX_ERR_UNSUPPORTED, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, 255 } },
        //num thread
        {/*62*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 1 } },
        {/*63*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumThread, 4 } },
        //gop pic size
        {/*64*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 } },
        //gop ref dist
        {/*65*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 } },
        //got opt flag
        {/*66*/ MFX_ERR_NONE, NONE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopOptFlag, 2 } },
        //protected
        {/*67*/
#if (defined(LINUX32) || defined(LINUX64))
            MFX_ERR_UNSUPPORTED,
#else
            MFX_ERR_NONE,
#endif
                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP } },
        {/*68*/
#if (defined(LINUX32) || defined(LINUX64))
            MFX_ERR_UNSUPPORTED,
#else
            MFX_ERR_NONE,
#endif
                          PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP } },
        {/*69*/ MFX_ERR_UNSUPPORTED, PROTECTED, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.Protected, 0xfff } },
        //Alignment
        {/*70*/ MFX_ERR_NONE, ALIGNMENT_HW, NONE, {} },
        {/*71*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8 } },
        {/*72*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, ALIGNMENT_HW, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 8 } },
        {/*73*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 } },
        {/*74*/ MFX_ERR_NONE, ALIGNMENT_HW, DELTA, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 } },
        // Parallel BRC with LowPower ON
        {/*75*/ MFX_ERR_NONE, Compatibility, PBRC, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
                                           { EXT_CO2, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID }}},
        {/*76*/ MFX_ERR_NONE, Compatibility, PBRC, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5 },
                                           { EXT_CO2, &tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_PYRAMID }}},
        // ICQ Mode with LowPower ON
        {/*77*/ MFX_ERR_NONE, Compatibility, ICQ, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 22 },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 }}},
        // QP range with LowPower ON
        {/*78*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, Compatibility, QPRange, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 6 },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }}},
        {/*79*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, Compatibility, QPRange, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 6 },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3 }}},
        // NumActiveRef with LowPower ON
        {/*80*/ MFX_ERR_NONE, Compatibility, NumReference, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_BEST_QUALITY },
                                           { EXT_CO3, &tsStruct::mfxExtCodingOption3.NumRefActiveP[0], 4 },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 20 }}},
        {/*81*/ MFX_ERR_NONE, Compatibility, NumReference, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_BEST_QUALITY },
                                           { EXT_CO3, &tsStruct::mfxExtCodingOption3.NumRefActiveBL0[0], 4 },
                                           { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 20 }}},
        //Invalid GoRefDist > GopPicSize
        {/*76*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 },
                                                              }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        mfxHDL hdl;
        mfxHandleType type;
        mfxExtBuffer* buff_in = NULL;
        mfxExtBuffer* buff_out = NULL;
        mfxStatus sts = tc.sts;
        ENCODE_CAPS_HEVC caps = {0};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        if (IsHevcHwPlugin() || IsHevcGaccPlugin())
        {
            if (g_tsHWtype < MFX_HW_SKL && IsHevcHwPlugin()) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query(m_session, m_pPar, m_pParOut);
                return 0;
            }

            if (g_tsConfig.lowpower == MFX_CODINGOPTION_ON && fourcc_id == GMOCK_FOURCC_Y212)
            {
                g_tsLog << "\n\nWARNING: Y212 format is supported in HEVCe DualPipe only!\n\n\n";
                throw tsSKIP;
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_OFF)
                && (fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are supported in HEVCe VDENC only!\n\n\n";
                throw tsSKIP;
            }
        }

        // set default parameters
        std::unique_ptr<mfxVideoParam> tmp_par(new mfxVideoParam);
        m_pParOut = tmp_par.get();
        *m_pParOut = m_par;

        MFXInit();
        Load();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
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

        mfxStatus caps_sts = GetCaps(&caps, &capSize);
        if (caps_sts != MFX_ERR_UNSUPPORTED)
            g_tsStatus.check(caps_sts);

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR);

            if (tc.type == RESOLUTION)
            {
                if (tc.sub_type == W_GT_MAX) {
                    if (caps.MaxPicWidth) {
                        m_pPar->mfx.FrameInfo.Width = caps.MaxPicWidth + 32;
                    }
                    else {
                        m_pPar->mfx.FrameInfo.Width = INVALID_WIDTH + 16;
                    }
                }
                if (tc.sub_type == H_GT_MAX) {
                    if (caps.MaxPicWidth) {
                        m_pPar->mfx.FrameInfo.Height = caps.MaxPicHeight + 32;
                    }
                    else {
                        m_pPar->mfx.FrameInfo.Height = INVALID_HEIGHT + 16;
                    }
                }
            }

        } else
        {   // pars depend on resolution
            for (mfxU32 i = 0; i < MAX_NPARS; i++) {
                if (tc.set_par[i].f && tc.set_par[i].ext_type == MFX_PAR) {
                    if (tc.type == RESOLUTION ||
                        (tc.type == ALIGNMENT_HW && IsHevcHwPlugin()))
                    {
                        if (tc.set_par[i].f->name.find("Width") != std::string::npos) {
                            m_pPar->mfx.FrameInfo.Width = ((m_pPar->mfx.FrameInfo.Width + 15) & ~15) + tc.set_par[i].v;
                        }
                        if (tc.set_par[i].f->name.find("Height") != std::string::npos)
                            m_pPar->mfx.FrameInfo.Height = ((m_pPar->mfx.FrameInfo.Height + 15) & ~15) + tc.set_par[i].v;
                    }
                    if (tc.type == CROP ||
                        (tc.type == ALIGNMENT_HW && IsHevcHwPlugin()))
                    {
                        if (tc.set_par[i].f->name.find("CropX") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropX += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropY") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropY += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropW") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropW += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropH") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropH += tc.set_par[i].v;
                    }
                }
            }
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

        if (tc.type == EXT_BUFF)
        {
            CreateBuffer(tc.sub_type, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            m_pParOut->ExtParam = &buff_out;
        }


        if (IsHevcHwPlugin())
        {
            if ((m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR) && (tc.sts == MFX_ERR_NONE))
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if (!USE_REFACTORED_HEVCE && (tc.type == IO_PATTERN) && (tc.sts == MFX_ERR_UNSUPPORTED))
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if (tc.type == EXT_BUFF)
            {
                if ((tc.sub_type == MFX_EXTBUFF_AVC_REFLIST_CTRL) ||
                    (tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                    (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                    sts = MFX_ERR_NONE;
                else if (tc.sub_type == NONE)
                    sts = MFX_ERR_NULL_PTR;
                else if (tc.sub_type == MFX_EXTBUFF_ENCODER_CAPABILITY && (g_tsImpl & MFX_IMPL_VIA_D3D11))
                    sts = MFX_ERR_NONE;

            }
            if (tc.type == CROP)
            {
                if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    sts = MFX_ERR_NONE;
                }
            }

            // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
            if ((!(tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)) && (tc.type == TARGET_USAGE))
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
        }
        else if (IsHevcGaccPlugin())
        {
            if (tc.type == PROTECTED)
                sts = MFX_ERR_UNSUPPORTED;
            // GACC: supported only TU = {4,5,6,7}
            if ((tc.set_par[0].v < 4) && (tc.type == TARGET_USAGE))
            {
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            // different expected status for SW HEVCe and GACC
            if (tc.type == PIC_STRUCT && tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                sts = MFX_ERR_UNSUPPORTED;
        }
        else
        {
            if (tc.type == PROTECTED)
                sts = MFX_ERR_UNSUPPORTED;
            // different expected status for SW HEVCe and GACC
            if (tc.type == PIC_STRUCT && tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (tc.type == IN_PAR_NULL)
        {
            m_pPar = NULL;
        }
        if (tc.type == OUT_PAR_NULL)
        {
            m_pParOut = NULL;
        }

        if (tc.type == Compatibility && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON || g_tsHWtype < MFX_HW_DG2))
        {
            g_tsLog << "[ SKIPPED ] VDEnc Compatibility tests are targeted only for VDEnc!\n\n\n";
            throw tsSKIP;
        }

        if (!IsChromaFormatSupported(m_par.mfx.FrameInfo.ChromaFormat, m_par.mfx.FrameInfo.BitDepthLuma)
            && sts >= MFX_ERR_NONE
            && tc.type != IN_PAR_NULL) // in this case MFX_ERR_NONE returns all the time, m_par isn't used
            sts = MFX_ERR_UNSUPPORTED;

        if (tc.type == ALIGNMENT_HW)
        {
            CreateBuffer(MFX_EXTBUFF_HEVC_PARAM, &buff_in, &buff_out);
            if (buff_in && buff_out)
                *buff_out = *buff_in;
            ((mfxExtHEVCParam*)buff_in)->PicWidthInLumaSamples = m_par.mfx.FrameInfo.Width;
            ((mfxExtHEVCParam*)buff_in)->PicHeightInLumaSamples = m_par.mfx.FrameInfo.Height;
            m_pPar->NumExtParam = 1;
            m_pParOut->NumExtParam = 1;
            m_pPar->ExtParam = &buff_in;
            m_pParOut->ExtParam = &buff_out;
        }

        g_tsStatus.expect(sts);
        g_tsStatus.disable_next_check();
        //call tested function
        if (tc.type != SESSION_NULL)
            sts = Query(m_session, m_pPar, m_pParOut);
        else
            sts = Query(NULL, m_pPar, m_pParOut);

        CheckOutPar(tc);
        if (buff_in)
            delete buff_in;
        if (buff_out)
            delete buff_out;
        g_tsStatus.check(sts);

        if ((tc.type == SET_ALLOCK) && (tc.sub_type == AFTER))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();

            if (!m_is_handle_set)
            {
                m_pFrameAllocator->get_hdl(type, hdl);

                if (IsHevcHwPlugin())
                    g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); // device was created by Core in Query() to get HW caps
                SetHandle(m_session, type, hdl);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_query, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_query, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_query, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_query, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_query, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_query, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_query, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_query, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_query, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
}
