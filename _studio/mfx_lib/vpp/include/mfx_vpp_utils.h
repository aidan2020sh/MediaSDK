/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010 - 2014 Intel Corporation. All Rights Reserved.
//
//
//                     utilities for (SW)VPP processing
//
*/
/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_UTILS_H
#define __MFX_VPP_UTILS_H

#include <vector>
#include "mfxvideo++int.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_interface.h"

// utility for correct processing of frame rates
// check if frame rates correspond inverse telecine algorithm (29.97(+-EPS) -> 23.976(+-EPS), EPS=0.1)
bool IsFrameRatesCorrespondITC(
    mfxU32  inFrameRateExtN,  
    mfxU32  inFrameRateExtD,
    mfxU32  outFrameRateExtN, 
    mfxU32  outFrameRateExtD);

// check if frame rates correspond advanced DI algorithm (example: 60i->60p. EPS = 0)
bool IsFrameRatesCorrespondMode30i60p(
    mfxU32  inFrameRateExtN,  
    mfxU32  inFrameRateExtD,
    mfxU32  outFrameRateExtN, 
    mfxU32  outFrameRateExtD);

// PicStruct processing
PicStructMode GetPicStructMode(mfxU16 inPicStruct, mfxU16 outPicStruct);
mfxStatus CheckInputPicStruct( mfxU16 inPicStruct );
mfxU16 UpdatePicStruct( mfxU16 inPicStruct, mfxU16 outPicStruct, bool bDynamicDeinterlace, mfxStatus& sts );

// copy surfaces based on Region Of Interest
mfxStatus SurfaceCopy_ROI(mfxFrameSurface1* out, mfxFrameSurface1* in, bool bROIControl = true);

//
mfxStatus SetBackGroundColor(mfxFrameSurface1 *ptr);

// compare ROI between 3 surfaces
bool IsROIConstant(mfxFrameSurface1* pSrc1, mfxFrameSurface1* pSrc2, mfxFrameSurface1* pSrc3);

bool IsRoiDifferent(mfxFrameSurface1 *input, mfxFrameSurface1 *output);

// utility calculates frames count (in/out) to correct processing in sync/async mode by external application
mfxStatus GetExternalFramesCount(mfxVideoParam* pParam,
                                 mfxU32* pListID,
                                 mfxU32 len,
                                 mfxU16 framesCountMin[2],
                                 mfxU16 framesCountSuggested[2]);

// utility to Check is composition enabled in current pipeline or not
mfxStatus GetCompositionEnabledStatus(mfxVideoParam* pParam );

// select configuration parameters for required filter
mfxStatus GetFilterParam(mfxVideoParam* par, mfxU32 filterName, mfxExtBuffer** ppHint);

// print names of used filters
void ShowPipeline( std::vector<mfxU32> pipelineList);

// common utils
void GetDoNotUseFilterList( mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen );
void GetDoUseFilterList( mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen );
void GetConfigurableFilterList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen );
bool GetExtParamList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen );
bool CheckFilterList(mfxU32* pList, mfxU32 count, bool bDoUseTable);

mfxStatus GetPipelineList(
    mfxVideoParam* videoParam, 
    std::vector<mfxU32> & pipelineList, 
    //mfxU32* pLen,
    bool    bExtended = false);

mfxStatus CheckFrameInfo(mfxFrameInfo* info, mfxU32 request);

mfxStatus CheckCropParam( mfxFrameInfo* info );

mfxStatus CompareFrameInfo(mfxFrameInfo* info1, mfxFrameInfo* info2);

//--------------------------------------------------------
bool IsFilterFound( const mfxU32* pList, mfxU32 len, mfxU32 filterName );

class VppCntx
{
public:
    explicit VppCntx(mfxU32* pList, mfxU32 len)
    {
        if (len > 0)
        {
            m_list.resize(len);
            for(mfxU32 i = 0; i < len; i++)
            {
                m_list[i] = pList[i];
            }
        }
    }

    bool IsFilterFound(mfxU32 filterName) 
    {
        if(m_list.size() > 0 )
        {
            return ::IsFilterFound( &m_list[0], mfxU32(m_list.size()), filterName ); 
        }

        return false;
    }

private:
    std::vector<mfxU32> m_list;
};
//--------------------------------------------------------

mfxU32 GetFilterIndex( mfxU32* pList, mfxU32 len, mfxU32 filterName);

mfxStatus CheckProtectedMode( mfxU16 mode );

mfxU16 vppMax_16u(const mfxU16* pSrc, int len);

mfxStatus ExtendedQuery(VideoCORE * core, mfxU32 filterName, mfxExtBuffer* pHint);

mfxStatus CheckExtParam(VideoCORE * core, mfxExtBuffer** ppExtParam, mfxU16 count);

// gamut processing
mfxStatus CheckTransferMatrix( mfxU16 transferMatrix );
mfxGamutMode GetGamutMode( mfxU16 srcTransferMatrix, mfxU16 dstTransferMatrix );

// Opaque processing
mfxStatus CheckOpaqMode( mfxVideoParam* par, bool bOpaqMode[2] );
mfxStatus GetOpaqRequest( mfxVideoParam* par, bool bOpaqMode[2], mfxFrameAllocRequest requestOpaq[2] );

//
mfxStatus CheckIOPattern_AndSetIOMemTypes(mfxU16 IOPattern, mfxU16* pInMemType, mfxU16* pOutMemType, bool bSWLib = true );

// Video Analytics
mfxU16 EstimatePicStruct( 
    mfxU32* pVariance,
    mfxU16 width,
    mfxU16 height);

mfxU16 MapDNFactor( mfxU16 denoiseFactor );

mfxU32 GetMFXFrcMode(const mfxVideoParam & videoParam);
mfxStatus SetMFXFrcMode(const mfxVideoParam & videoParam, mfxU32 mode);
mfxStatus SetMFXISMode(const mfxVideoParam & videoParam, mfxU32 mode);

mfxU32 GetDeinterlacingMode(const mfxVideoParam & videoParam);
mfxStatus SetDeinterlacingMode(const mfxVideoParam & videoParam, mfxU32 mode);

mfxStatus SetSignalInfo(const mfxVideoParam & videoParam, mfxU32 trMatrix, mfxU32 Range);

void ExtractDoUseList(
    mfxU32* pSrcList, 
    mfxU32 len, 
    std::vector<mfxU32> & dstList);

bool CheckDoUseCompatibility( mfxU32 filterName );

mfxStatus GetCrossList(
    const std::vector<mfxU32> & pipelineList, 
    const std::vector<mfxU32> & capsList, 
    std::vector<mfxU32> & douseList,
    std::vector<mfxU32> & dontUseList);

mfxStatus CheckLimitationsSW(
    const mfxVideoParam & param, 
    const std::vector<mfxU32> & supportedList, 
    bool bCorrectionEnable);

mfxStatus CheckLimitationsHW(
    const mfxVideoParam & param, 
    const std::vector<mfxU32> & supportedDoUseList,
    const MfxHwVideoProcessing::mfxVppCaps & caps,
    bool bCorrectionEnable);

bool IsFrcInterpolationEnable(
    const mfxVideoParam & param, 
    const MfxHwVideoProcessing::mfxVppCaps & caps);

bool IsConfigurable( mfxU32 filterId );
size_t GetConfigSize( mfxU32 filterId );

//mfxStatus QueryExtParams()

#endif // __MFX_VPP_UTILS_H

#endif // MFX_ENABLE_VPP
/*EOF*/
