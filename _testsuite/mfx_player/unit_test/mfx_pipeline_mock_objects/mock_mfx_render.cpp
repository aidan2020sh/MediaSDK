/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mock_mfx_render.h"


mfxStatus MockRender::QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest *request)
{
    TEST_METHOD_TYPE(QueryIOSurf) params;
    if (!_QueryIOSurf.GetReturn(params))
    {
        //strategy not defined
        return MFX_ERR_NONE;
    }

    if (NULL != request && NULL != params.value1)
    {
        *request = *params.value1;
    }

    return params.ret_val;
}

mfxStatus MockRender::Init(mfxVideoParam *par, const vm_char *pFilename = NULL)
{
    _Init.RegisterEvent(TEST_METHOD_TYPE(Init)(MFX_ERR_NONE, par, pFilename));

    return MFX_ERR_NONE;
}

mfxStatus MockRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    _RenderFrame.RegisterEvent(TEST_METHOD_TYPE(RenderFrame)(MFX_ERR_NONE, surface, pCtrl));

    return MFX_ERR_NONE;
}
mfxStatus MockRender::SetAutoView( bool bAuto)
{
    _SetAutoView.RegisterEvent(TEST_METHOD_TYPE(SetAutoView)(MFX_ERR_NONE, bAuto));

    return MFX_ERR_NONE;
}

mfxStatus MockRender::WaitTasks(mfxU32 /*nMilisecconds*/)
{
    TEST_METHOD_TYPE(WaitTasks) ret_val;
    
    _WaitTasks.GetReturn(ret_val);

    return ret_val.ret_val;
}