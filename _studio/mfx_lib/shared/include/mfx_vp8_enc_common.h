/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
//          VP8 encoder common
//
*/

#include "mfx_common.h"
 
#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE)
 
#ifndef __MFX_VP8_ENC_COMMON__H
#define __MFX_VP8_ENC_COMMON__H
 
#include "mfxvp8.h"

namespace VP8_encoder
{
    mfxU16 GetDefaultAsyncDepth();

    /*function for init/reset*/
    mfxStatus CheckParametersAndSetDefault( mfxVideoParam*           pParamSrc,
                                            mfxVideoParam*           pParamDst,
                                            mfxExtCodingOptionVP8*   pExCodingVP8Dst,
                                            mfxExtOpaqueSurfaceAlloc*pOpaqAllocDst,
                                            bool                     bExternalFrameAllocator,
                                            bool                     bReset = false);

    /*functios for Query*/

    mfxStatus SetSupportedParameters(mfxVideoParam* par);

    mfxStatus CheckParameters(mfxVideoParam*   parSrc,
                              mfxVideoParam*   parDst);

    mfxStatus CheckExtendedBuffers (mfxVideoParam* par);

}
#endif 
#endif 