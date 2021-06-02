/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_outline_render.h"

#include "outline_dll.h"
#include "outline_mfx_wrapper.h"


MFXOutlineRender::MFXOutlineRender(const FileWriterRenderInputParams & params, IVideoSession *core, mfxStatus *status)
: MFXFileWriteRender(params, core, status)
, m_checker (0)
, m_decoderParams(0)
, m_wasSequenceProcessed(false)
, m_wasInitialized(false)
, m_isNeedToCheckFramesNumber(false)
, m_refOutline(0)
, m_FWRenderFile(nullptr)
{
}

MFXOutlineRender::~MFXOutlineRender()
{
    Close();
    InternalClose();
}

mfxStatus MFXOutlineRender::InternalInit(mfxVideoParam *par, const vm_char *pFilename)
{
    UNREFERENCED_PARAMETER(par);

    if (m_wasInitialized)
        return MFX_ERR_NONE;

    OutlineFactoryAbstract * factory;
    MFX_CHECK_POINTER(factory = GetOutlineFactory());

    VideoDataChecker_MFX_Base * checker;
    MFX_CHECK_POINTER(checker = factory->GetMFXChecher());

    m_checker = checker;

    TestComponentParams testParams;

    testParams.m_OutlineFilename = pFilename;
    testParams.m_RefOutlineFilename = m_refOutline;
    testParams.m_Mode = (pFilename && pFilename[0] ?  TEST_OUTLINE_WRITE : TEST_OUTLINE_NONE) |
                        (m_refOutline && m_refOutline[0] ?  TEST_OUTLINE_READ : TEST_OUTLINE_NONE);

    MFX_CHECK(UMC::UMC_OK == m_checker->Init(&testParams));

    OutlineParams params;
    params.m_isPrettyPrint = true;
    params.m_searchMethod = FIND_MODE_TIMESTAMP;
    m_checker->SetParams(params);

    m_wasInitialized = true;

    return MFX_ERR_NONE;
}

mfxStatus MFXOutlineRender::Init(mfxVideoParam *par, const vm_char *pFilename)
{
    MFX_CHECK_STS(InternalInit(par, pFilename));

    MFX_CHECK_STS(MFXFileWriteRender::Init(par, m_FWRenderFile));

    return MFX_ERR_NONE;
}

mfxStatus MFXOutlineRender::Reset(mfxVideoParam *par)
{
    m_wasSequenceProcessed = false;
    MFX_CHECK(UMC::UMC_OK == m_checker->Reset());

    return MFXFileWriteRender::Reset(par);
}

mfxStatus MFXOutlineRender::InternalClose()
{
    if (m_isNeedToCheckFramesNumber)
    {
        MFX_CHECK (!m_checker->IsFrameExist() && !m_checker->IsSequenceExist());
    }

    m_wasSequenceProcessed = false;

    if (m_checker)
    {
        UMC::Status sts = m_checker->Close();
        delete m_checker;
        m_checker = 0;

        MFX_CHECK(UMC::UMC_OK == sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXOutlineRender::Close()
{
    MFX_CHECK(!m_isNeedToCheckFramesNumber || !m_checker->IsFrameExist());

    m_wasSequenceProcessed = false;
    return MFX_ERR_NONE;
}

void MFXOutlineRender::IsNeedToCheckFramesNumber(bool check)
{
    m_isNeedToCheckFramesNumber = check;
}

void MFXOutlineRender::SetDecoderVideoParamsPtr(mfxVideoParam *par)
{
    m_decoderParams = par;
}

void MFXOutlineRender::SetFiles( const vm_char *pOutputFilename
                                , const vm_char *pRefFilename)
{
    m_refOutline = pRefFilename;
    m_FWRenderFile = pOutputFilename;
}


mfxStatus MFXOutlineRender::ProcessSequence(mfxVideoParam *par)
{
    MFX_CHECK(UMC::UMC_OK == m_checker->ProcessSequence(par));

    m_wasSequenceProcessed = true;

    return MFX_ERR_NONE;
}

mfxStatus MFXOutlineRender::RenderFrame(mfxFrameSurface1 * surface, mfxEncodeCtrl * pCtrl)
{
    UNREFERENCED_PARAMETER(pCtrl);

    if (!surface)
        return MFX_ERR_NONE;

    if (!m_wasSequenceProcessed)
    {
        MFX_CHECK_STS(ProcessSequence(m_decoderParams));
    }

#if defined(_WIN32) || defined(_WIN64)
    mfxHDLPair handle;
    if (NULL != surface->Data.MemId)
    {
        MFX_CHECK_STS(GetFrameHDL(surface->Data.MemId, (mfxHDL*)(&handle)));
    }

    bool bFrameLocked = false;
    if (m_bDecodeD3D11)
    {
        if (m_pLock == nullptr)
            m_pLock = new MFXFrameLocker(m_pHWDevice);
        if (NULL != surface)
        {
            if (NULL == surface->Data.Y &&
                NULL == surface->Data.U &&
                NULL == surface->Data.V)
            {
                MFX_CHECK_STS(m_pLock->MapFrame(&surface->Data, (mfxHDL*)(&handle)));
                bFrameLocked = true;
            }
        }
    }
    else
#endif
    {
        MFX_CHECK_STS(LockFrame(surface));
    }

    mfxFrameSurface1 * convertedSurface = surface;

    if (surface->Info.FourCC != m_auxSurface.Info.FourCC)
    {
        convertedSurface = ConvertSurface(surface, &m_auxSurface, &m_params);
        if (!convertedSurface)
            return MFX_ERR_UNKNOWN;
    }

    MFX_CHECK_STS(MFXFileWriteRender::RenderFrame(convertedSurface));

    if (m_checker->ProcessFrame(convertedSurface) != UMC::UMC_OK)
    {
        OutlineFactoryAbstract * factory;

        MFX_CHECK_POINTER(factory = GetOutlineFactory());

        OutlineErrors * errors = factory->GetOutlineErrors();
        if (errors)
            errors->PrintError();

        return MFX_ERR_UNKNOWN;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (m_bDecodeD3D11)
    {
        if (NULL != surface && bFrameLocked)
            MFX_CHECK_STS(m_pLock->UnmapFrame(&surface->Data));
    }
    else
#endif
    {
        MFX_CHECK_STS(UnlockFrame(surface));
    }

    return MFX_ERR_NONE;
}
