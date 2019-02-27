//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2011-2012 Intel Corporation. All Rights Reserved.
//

#include "test_vpp_utils.h"

mfxStatus ConfigVideoEnhancementFilters( sInputParams* pParams, sAppResources* pResources )
{
    mfxVideoParam*   pVppParam = pResources->pVppParams;
    mfxU32  enhFilterCount = 0;

    // [0] common tuning params
    pVppParam->NumExtParam = 0;
    // to simplify logic
    pVppParam->ExtParam    = (mfxExtBuffer**)pResources->pExtBuf;

    pResources->extDoUse.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    pResources->extDoUse.Header.BufferSz = sizeof(mfxExtVPPDoUse);
    pResources->extDoUse.NumAlg  = 0;
    pResources->extDoUse.AlgList = NULL;

    // [1] video enhancement algorithms can be enabled with default parameters 
    if( VPP_FILTER_DISABLED != pParams->denoiseParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_DENOISE;
    }
    if( VPP_FILTER_DISABLED != pParams->vaParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;    
    }

    if( VPP_FILTER_DISABLED != pParams->varianceParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_VARIANCE_REPORT;    
    }
    if( VPP_FILTER_DISABLED != pParams->idetectParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION;    
    }
    if( VPP_FILTER_DISABLED != pParams->procampParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_PROCAMP;     
    }
    if( VPP_FILTER_DISABLED != pParams->detailParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_DETAIL;     
    }
    // MSDK API 2013
    if( VPP_FILTER_ENABLED_DEFAULT == pParams->istabParam.mode)
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
    }
    /*if( VPP_FILTER_DISABLED != pParams->aceParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_AUTO_CONTRAST;
    }
    if( VPP_FILTER_DISABLED != pParams->steParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_SKIN_TONE;
    }
    if( VPP_FILTER_DISABLED != pParams->tccParam.mode )
    {
        pResources->tabDoUseAlg[enhFilterCount++] = MFX_EXTBUFF_VPP_COLOR_SATURATION_LEVEL;
    }*/

    if( enhFilterCount > 0 )
    {
        pResources->extDoUse.NumAlg  = enhFilterCount;
        pResources->extDoUse.AlgList = pResources->tabDoUseAlg;
        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->extDoUse);
    }

    // [2] video enhancement algorithms can be configured
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->denoiseParam.mode )
    {
        pResources->denoiseConfig.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
        pResources->denoiseConfig.Header.BufferSz = sizeof(mfxExtVPPDenoise);

        pResources->denoiseConfig.DenoiseFactor   = pParams->denoiseParam.factor;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->denoiseConfig);
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->vaParam.mode )
    {
        // video analysis filters isn't configured
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->frcParam.mode )
    {
        pResources->frcConfig.Header.BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        pResources->frcConfig.Header.BufferSz = sizeof(mfxExtVPPFrameRateConversion);

        pResources->frcConfig.Algorithm   = (mfxU16)pParams->frcParam.algorithm;//MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->frcConfig);
    }

    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->procampParam.mode )
    {
        pResources->procampConfig.Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
        pResources->procampConfig.Header.BufferSz = sizeof(mfxExtVPPProcAmp);

        pResources->procampConfig.Hue        = pParams->procampParam.hue;
        pResources->procampConfig.Saturation = pParams->procampParam.saturation;
        pResources->procampConfig.Contrast   = pParams->procampParam.contrast;
        pResources->procampConfig.Brightness = pParams->procampParam.brightness;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->procampConfig);     
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->detailParam.mode )
    {
        pResources->detailConfig.Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
        pResources->detailConfig.Header.BufferSz = sizeof(mfxExtVPPDetail);

        pResources->detailConfig.DetailFactor   = pParams->detailParam.factor;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->detailConfig);    
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->deinterlaceParam.mode )
    {
        pResources->deinterlaceConfig.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        pResources->deinterlaceConfig.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
        pResources->deinterlaceConfig.Mode = pParams->deinterlaceParam.algorithm;
        pResources->deinterlaceConfig.TelecinePattern  = pParams->deinterlaceParam.tc_pattern;
        pResources->deinterlaceConfig.TelecineLocation = pParams->deinterlaceParam.tc_pos;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->deinterlaceConfig);
    }
    if( 0 != pParams->rotate )
    {
        pResources->rotationConfig.Header.BufferId = MFX_EXTBUFF_VPP_ROTATION;
        pResources->rotationConfig.Header.BufferSz = sizeof(mfxExtVPPRotation);
        pResources->rotationConfig.Angle           = pParams->rotate;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->rotationConfig);
    }
    //if( VPP_FILTER_ENABLED_CONFIGURED == pParams->gamutParam.mode )
    //{
    //    pResources->gamutConfig.Header.BufferId = MFX_EXTBUFF_VPP_GAMUT_MAPPING;
    //    pResources->gamutConfig.Header.BufferSz = sizeof(mfxExtVPPGamutMapping);

    //    //pResources->detailConfig.DetailFactor   = pParams->detailParam.factor;
    //    if( pParams->gamutParam.bBT709 )
    //    {
    //        pResources->gamutConfig.InTransferMatrix  = MFX_TRASNFERMATRIX_XVYCC_BT709;
    //        pResources->gamutConfig.OutTransferMatrix = MFX_TRANSFERMATRIX_BT709;
    //    }
    //    else
    //    {
    //        pResources->gamutConfig.InTransferMatrix  = MFX_TRANSFERMATRIX_XVYCC_BT601;
    //        pResources->gamutConfig.OutTransferMatrix = MFX_TRANSFERMATRIX_BT601;
    //    }

    //    pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->gamutConfig);
    //}
    // MSDK 1.5 -------------------------------------------
    //if( VPP_FILTER_ENABLED_CONFIGURED == pParams->tccParam.mode )
    //{
    //    pResources->tccConfig.Header.BufferId = MFX_EXTBUFF_VPP_COLOR_SATURATION_LEVEL;
    //    pResources->tccConfig.Header.BufferSz = sizeof(mfxExtVPPColorSaturationLevel);

    //    pResources->tccConfig.Red     = pParams->tccParam.Red;
    //    pResources->tccConfig.Green   = pParams->tccParam.Green;
    //    pResources->tccConfig.Blue    = pParams->tccParam.Blue;
    //    pResources->tccConfig.Magenta = pParams->tccParam.Magenta;
    //    pResources->tccConfig.Yellow  = pParams->tccParam.Yellow;
    //    pResources->tccConfig.Cyan    = pParams->tccParam.Cyan;

    //    pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->tccConfig);
    //}
    //if( VPP_FILTER_ENABLED_CONFIGURED == pParams->aceParam.mode )
    //{
    //    // to do
    //}
    //if( VPP_FILTER_ENABLED_CONFIGURED == pParams->steParam.mode )
    //{
    //    pResources->steConfig.Header.BufferId = MFX_EXTBUFF_VPP_SKIN_TONE;
    //    pResources->steConfig.Header.BufferSz = sizeof(mfxExtVPPSkinTone);
    //    pResources->steConfig.SkinToneFactor  = pParams->steParam.SkinToneFactor;

    //    pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->steConfig);
    //}
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->istabParam.mode )
    {
        pResources->istabConfig.Header.BufferId = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
        pResources->istabConfig.Header.BufferSz = sizeof(mfxExtVPPImageStab);
        pResources->istabConfig.Mode            = pParams->istabParam.istabMode;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->istabConfig);
    }

    // ----------------------------------------------------
    // MVC
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->multiViewParam.mode )
    {
        pResources->multiViewConfig.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
        pResources->multiViewConfig.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);

        pResources->multiViewConfig.NumView = pParams->multiViewParam.viewCount;
        pResources->multiViewConfig.View    = new mfxMVCViewDependency [ pParams->multiViewParam.viewCount ];

        ViewGenerator viewGenerator( pParams->multiViewParam.viewCount );

        for( mfxU16 viewIndx = 0; viewIndx < pParams->multiViewParam.viewCount; viewIndx++ )
        {
            pResources->multiViewConfig.View[viewIndx].ViewId = viewGenerator.GetNextViewID();          
        }

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->multiViewConfig);
    }

    // ----------------------------------------------------
    // SVC
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->svcParam.mode )
    {
        memset( (void*)&(pResources->svcConfig), 0, sizeof(mfxExtSVCSeqDesc));

        pResources->svcConfig.Header.BufferId = MFX_EXTBUFF_SVC_SEQ_DESC;
        pResources->svcConfig.Header.BufferSz = sizeof(mfxExtSVCSeqDesc);

        for( mfxU16 layer = 0; layer < 8; layer++ )
        {
            pResources->svcConfig.DependencyLayer[layer].Active = pParams->svcParam.descr[layer].active;
            if( pResources->svcConfig.DependencyLayer[layer].Active )
            {
                // aya: here we use aligned size (width, height)
                pResources->svcConfig.DependencyLayer[layer].Width = pParams->svcParam.descr[layer].width;
                pResources->svcConfig.DependencyLayer[layer].Height= pParams->svcParam.descr[layer].height;

                // aya: should be modified in case of descr will contain crop info
                pResources->svcConfig.DependencyLayer[layer].CropX = 0;
                pResources->svcConfig.DependencyLayer[layer].CropY = 0;
                pResources->svcConfig.DependencyLayer[layer].CropW = pResources->svcConfig.DependencyLayer[layer].Width;
                pResources->svcConfig.DependencyLayer[layer].CropH = pResources->svcConfig.DependencyLayer[layer].Height;
            }
        }

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->svcConfig);
    }

    // confirm configuration
    if( 0 == pVppParam->NumExtParam )
    {
        pVppParam->ExtParam = NULL;
    }

    return MFX_ERR_NONE;

} // mfxStatus ConfigVideoEnhancementFilters( sAppResources* pResources, mfxVideoParam* pParams )
/* EOF */