/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */


EXTBUF(mfxExtCodingOption                , MFX_EXTBUFF_CODING_OPTION                   )
EXTBUF(mfxExtCodingOptionSPSPPS          , MFX_EXTBUFF_CODING_OPTION_SPSPPS            )
EXTBUF(mfxExtVPPDoNotUse                 , MFX_EXTBUFF_VPP_DONOTUSE                    )
EXTBUF(mfxExtVppAuxData                  , MFX_EXTBUFF_VPP_AUXDATA                     )
EXTBUF(mfxExtVPPDenoise                  , MFX_EXTBUFF_VPP_DENOISE                     )
EXTBUF(mfxExtVPPProcAmp                  , MFX_EXTBUFF_VPP_PROCAMP                     )
EXTBUF(mfxExtVPPDetail                   , MFX_EXTBUFF_VPP_DETAIL                      )
EXTBUF(mfxExtVideoSignalInfo             , MFX_EXTBUFF_VIDEO_SIGNAL_INFO               )
EXTBUF(mfxExtVPPDoUse                    , MFX_EXTBUFF_VPP_DOUSE                       )
EXTBUF(mfxExtOpaqueSurfaceAlloc          , MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION       )
EXTBUF(mfxExtAVCRefListCtrl              , MFX_EXTBUFF_AVC_REFLIST_CTRL                )
EXTBUF(mfxExtVPPFrameRateConversion      , MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION       )
EXTBUF(mfxExtPictureTimingSEI            , MFX_EXTBUFF_PICTURE_TIMING_SEI              )
EXTBUF(mfxExtAvcTemporalLayers           , MFX_EXTBUFF_AVC_TEMPORAL_LAYERS             )
EXTBUF(mfxExtCodingOption2               , MFX_EXTBUFF_CODING_OPTION2                  )
EXTBUF(mfxExtVPPImageStab                , MFX_EXTBUFF_VPP_IMAGE_STABILIZATION         )
EXTBUF(mfxExtEncoderCapability           , MFX_EXTBUFF_ENCODER_CAPABILITY              )
EXTBUF(mfxExtEncoderResetOption          , MFX_EXTBUFF_ENCODER_RESET_OPTION            )
EXTBUF(mfxExtAVCEncodedFrameInfo         , MFX_EXTBUFF_ENCODED_FRAME_INFO              )
EXTBUF(mfxExtVPPComposite                , MFX_EXTBUFF_VPP_COMPOSITE                   )
EXTBUF(mfxExtVPPVideoSignalInfo          , MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO           )
EXTBUF(mfxExtEncoderROI                  , MFX_EXTBUFF_ENCODER_ROI                     )
EXTBUF(mfxExtVPPDeinterlacing            , MFX_EXTBUFF_VPP_DEINTERLACING               )
EXTBUF(mfxExtVP8CodingOption             , MFX_EXTBUFF_VP8_CODING_OPTION               )
EXTBUF(mfxExtLAControl                   , MFX_EXTBUFF_LOOKAHEAD_CTRL                  )
EXTBUF(mfxExtVPPFieldProcessing          , MFX_EXTBUFF_VPP_FIELD_PROCESSING            )
EXTBUF(mfxExtContentLightLevelInfo       , MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO        )
EXTBUF(mfxExtMasteringDisplayColourVolume, MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME )
EXTBUF(mfxExtMultiFrameParam             , MFX_EXTBUFF_MULTI_FRAME_PARAM               )
EXTBUF(mfxExtMultiFrameControl           , MFX_EXTBUFF_MULTI_FRAME_CONTROL             )
#if (MFX_VERSION >= 1026)
EXTBUF(mfxExtVppMctf                     , MFX_EXTBUFF_VPP_MCTF                        )
#endif
EXTBUF(mfxExtColorConversion             , MFX_EXTBUFF_VPP_COLOR_CONVERSION            )
// FEI
EXTBUF(mfxExtFeiParam               , MFX_EXTBUFF_FEI_PARAM                 )
EXTBUF(mfxExtFeiSPS                 , MFX_EXTBUFF_FEI_SPS                   )
EXTBUF(mfxExtFeiPPS                 , MFX_EXTBUFF_FEI_PPS                   )
EXTBUF(mfxExtFeiEncFrameCtrl        , MFX_EXTBUFF_FEI_ENC_CTRL              )
EXTBUF(mfxExtFeiEncMVPredictors     , MFX_EXTBUFF_FEI_ENC_MV_PRED           )
EXTBUF(mfxExtFeiEncMBCtrl           , MFX_EXTBUFF_FEI_ENC_MB                )
EXTBUF(mfxExtFeiEncMV               , MFX_EXTBUFF_FEI_ENC_MV                )
EXTBUF(mfxExtFeiEncMBStat           , MFX_EXTBUFF_FEI_ENC_MB_STAT           )
EXTBUF(mfxExtFeiEncQP               , MFX_EXTBUFF_FEI_ENC_QP                )
EXTBUF(mfxExtFeiPreEncCtrl          , MFX_EXTBUFF_FEI_PREENC_CTRL           )
EXTBUF(mfxExtFeiPreEncMVPredictors  , MFX_EXTBUFF_FEI_PREENC_MV_PRED        )
EXTBUF(mfxExtFeiPreEncMV            , MFX_EXTBUFF_FEI_PREENC_MV             )
EXTBUF(mfxExtFeiPreEncMBStat        , MFX_EXTBUFF_FEI_PREENC_MB             )
EXTBUF(mfxExtFeiPakMBCtrl           , MFX_EXTBUFF_FEI_PAK_CTRL              )
#if !defined(OPEN_SOURCE)
EXTBUF(mfxExtFEIH265Param           , MFX_EXTBUFF_FEI_H265_PARAM            )
EXTBUF(mfxExtFEIH265Input           , MFX_EXTBUFF_FEI_H265_INPUT            )
EXTBUF(mfxExtFEIH265Output          , MFX_EXTBUFF_FEI_H265_OUTPUT           )
#endif
EXTBUF(mfxExtFeiSliceHeader         , MFX_EXTBUFF_FEI_SLICE                 )
EXTBUF(mfxExtFeiRepackCtrl          , MFX_EXTBUFF_FEI_REPACK_CTRL           )
EXTBUF(mfxExtFeiDecStreamOut        , MFX_EXTBUFF_FEI_DEC_STREAM_OUT        )
#if (MFX_VERSION >= 1027)
EXTBUF(mfxExtFeiHevcEncFrameCtrl    , MFX_EXTBUFF_HEVCFEI_ENC_CTRL          )
EXTBUF(mfxExtFeiHevcEncMVPredictors , MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED       )
EXTBUF(mfxExtFeiHevcEncQP           , MFX_EXTBUFF_HEVCFEI_ENC_QP            )
EXTBUF(mfxExtFeiHevcEncCtuCtrl      , MFX_EXTBUFF_HEVCFEI_ENC_CTU_CTRL      )
#endif
// end of FEI
// Camera
EXTBUF(mfxExtCamTotalColorControl   , MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL     )
EXTBUF(mfxExtCamCscYuvRgb           , MFX_EXTBUF_CAM_CSC_YUV_RGB             )
EXTBUF(mfxExtCamGammaCorrection     , MFX_EXTBUF_CAM_GAMMA_CORRECTION        )
EXTBUF(mfxExtCamWhiteBalance        , MFX_EXTBUF_CAM_WHITE_BALANCE           )
EXTBUF(mfxExtCamHotPixelRemoval     , MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL       )
EXTBUF(mfxExtCamBlackLevelCorrection, MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION  )
EXTBUF(mfxExtCamVignetteCorrection  , MFX_EXTBUF_CAM_VIGNETTE_CORRECTION     )
EXTBUF(mfxExtCamBayerDenoise        , MFX_EXTBUF_CAM_BAYER_DENOISE           )
EXTBUF(mfxExtCamColorCorrection3x3  , MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3    )
EXTBUF(mfxExtCamPadding             , MFX_EXTBUF_CAM_PADDING                 )
EXTBUF(mfxExtCamPipeControl         , MFX_EXTBUF_CAM_PIPECONTROL             )
// end of Camera
EXTBUF(mfxExtAVCRefLists            , MFX_EXTBUFF_AVC_REFLISTS               )
EXTBUF(mfxExtCodingOption3          , MFX_EXTBUFF_CODING_OPTION3             )
EXTBUF(mfxExtMBQP                   , MFX_EXTBUFF_MBQP                       )
EXTBUF(mfxExtMBForceIntra           , MFX_EXTBUFF_MB_FORCE_INTRA             )

EXTBUF(mfxExtChromaLocInfo          , MFX_EXTBUFF_CHROMA_LOC_INFO            )
EXTBUF(mfxExtDecodedFrameInfo       , MFX_EXTBUFF_DECODED_FRAME_INFO         )
EXTBUF(mfxExtDecodeErrorReport      , MFX_EXTBUFF_DECODE_ERROR_REPORT        )

// Threading API
EXTBUF(mfxExtThreadsParam           , MFX_EXTBUFF_THREADS_PARAM)

EXTBUF(mfxExtVPPRotation            , MFX_EXTBUFF_VPP_ROTATION)
EXTBUF(mfxExtVPPMirroring           , MFX_EXTBUFF_VPP_MIRRORING)
EXTBUF(mfxExtMVCSeqDesc             , MFX_EXTBUFF_MVC_SEQ_DESC              )
//EXTBUF(mfxExtMVCTargetViews         , MFX_EXTBUFF_MVC_TARGET_VIEWS          )
//EXTBUF(mfxExtJPEGQuantTables        , MFX_EXTBUFF_JPEG_QT                   )
//EXTBUF(mfxExtJPEGHuffmanTables      , MFX_EXTBUFF_JPEG_HUFFMAN              )
EXTBUF(mfxExtMBDisableSkipMap       , MFX_EXTBUFF_MB_DISABLE_SKIP_MAP)

//Screen capture
EXTBUF(mfxExtScreenCaptureParam     , MFX_EXTBUFF_SCREEN_CAPTURE_PARAM      )
EXTBUF(mfxExtDirtyRect              , MFX_EXTBUFF_DIRTY_RECTANGLES          )
EXTBUF(mfxExtMoveRect               , MFX_EXTBUFF_MOVING_RECTANGLES         )

#if (MFX_VERSION >= MFX_VERSION_NEXT)
EXTBUF(mfxExtVP9DecodedFrameInfo    , MFX_EXTBUFF_VP9_DECODED_FRAME_INFO    )
#endif

#if (MFX_VERSION >= 1026)
EXTBUF(mfxExtVP9Segmentation        , MFX_EXTBUFF_VP9_SEGMENTATION          )
EXTBUF(mfxExtVP9TemporalLayers      , MFX_EXTBUFF_VP9_TEMPORAL_LAYERS       )
EXTBUF(mfxExtVP9Param               , MFX_EXTBUFF_VP9_PARAM                 )
#endif

EXTBUF(mfxExtHEVCParam              , MFX_EXTBUFF_HEVC_PARAM                )
EXTBUF(mfxExtHEVCTiles              , MFX_EXTBUFF_HEVC_TILES                )
EXTBUF(mfxExtPredWeightTable        , MFX_EXTBUFF_PRED_WEIGHT_TABLE         )

#if defined(__MFXBRC_H__)
EXTBUF(mfxExtBRC, MFX_EXTBUFF_BRC)
#endif // defined(__MFXBRC_H__)

EXTBUF(mfxExtEncodedUnitsInfo       , MFX_EXTBUFF_ENCODED_UNITS_INFO        )

#if (MFX_VERSION >= MFX_VERSION_NEXT)
EXTBUF(mfxExtAVCScalingMatrix       , MFX_EXTBUFF_AVC_SCALING_MATRIX        )
#endif

#if defined(__MFXPCP_H__)

#if !defined(OPEN_SOURCE)
EXTBUF(mfxExtPAVPOption             , MFX_EXTBUFF_PAVP_OPTION               )
#if (MFX_VERSION >= MFX_VERSION_NEXT)
EXTBUF(mfxExtDecryptedParam         , MFX_EXTBUFF_DECRYPTED_PARAM           )
#endif
#endif

#if (MFX_VERSION >= 1030)
EXTBUF(mfxExtCencParam              , MFX_EXTBUFF_CENC_PARAM                )
#endif

#endif // defined(__MFXPCP_H__)

#if defined(__MFXSCD_H__)
EXTBUF(mfxExtSCD, MFX_EXTBUFF_SCD)
#endif // defined(__MFXSCD_H__)
