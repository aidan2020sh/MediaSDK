/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "fei_encpak.h"

FEI_EncPakInterface::FEI_EncPakInterface(MFXVideoSession* session, iTaskPool* task_pool, mfxU32 allocId, bufList* ext_bufs, AppConfig* config)
    : m_pmfxSession(session)
    , m_pmfxENC((config->bOnlyENC || config->bENCPAK) ? new MFXVideoENC(*session) : NULL)
    , m_pmfxPAK((config->bOnlyPAK || config->bENCPAK) ? new MFXVideoPAK(*session) : NULL)
    , m_inputTasks(task_pool)
    , m_allocId(allocId)
    , m_pExtBuffers(ext_bufs)
    , m_pAppConfig(config)
    , m_SyncPoint(0)
    , m_bSingleFieldMode(config->bFieldProcessingMode)
    , m_pMvPred_in(NULL)
    , m_pENC_MBCtrl_in(NULL)
    , m_pMbQP_in(NULL)
    , m_pRepackCtrl_in(NULL)
    , m_pMBstat_out(NULL)
    , m_pMV_out(NULL)
    , m_pMBcode_out(NULL)
{
    MSDK_ZERO_MEMORY(m_mfxBS);
    MSDK_ZERO_MEMORY(m_videoParams_ENC);
    MSDK_ZERO_MEMORY(m_videoParams_PAK);

    m_InitExtParams_ENC.reserve(1);
    m_InitExtParams_PAK.reserve(1);

    /* Default values for I-frames */
    memset(&m_tmpMBencMV, 0x8000, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
}

FEI_EncPakInterface::~FEI_EncPakInterface()
{
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);

    for (mfxU32 i = 0; i < m_InitExtParams_ENC.size(); ++i)
    {
        switch (m_InitExtParams_ENC[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PARAM:
            mfxExtFeiParam* ptr = reinterpret_cast<mfxExtFeiParam*>(m_InitExtParams_ENC[i]);
            MSDK_SAFE_DELETE(ptr);
            break;
        }
    }
    m_InitExtParams_ENC.clear();

    for (mfxU32 i = 0; i < m_InitExtParams_PAK.size(); ++i)
    {
        switch (m_InitExtParams_PAK[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PARAM:
            mfxExtFeiParam* ptr = reinterpret_cast<mfxExtFeiParam*>(m_InitExtParams_PAK[i]);
            MSDK_SAFE_DELETE(ptr);
            break;
        }
    }
    m_InitExtParams_PAK.clear();

    WipeMfxBitstream(&m_mfxBS);

    SAFE_FCLOSE(m_pMvPred_in);
    SAFE_FCLOSE(m_pENC_MBCtrl_in);
    SAFE_FCLOSE(m_pMbQP_in);
    SAFE_FCLOSE(m_pRepackCtrl_in);
    SAFE_FCLOSE(m_pMBstat_out);
    SAFE_FCLOSE(m_pMV_out);
    SAFE_FCLOSE(m_pMBcode_out);

    m_pAppConfig->PipelineCfg.pEncVideoParam = NULL;
    m_pAppConfig->PipelineCfg.pPakVideoParam = NULL;
}

mfxStatus FEI_EncPakInterface::Init()
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;
    if (m_pmfxENC)
    {
        sts = m_pmfxENC->Init(&m_videoParams_ENC);
        MSDK_CHECK_STATUS(sts, "FEI ENC: Init failed");
    }
    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->Init(&m_videoParams_PAK);
        MSDK_CHECK_STATUS(sts, "FEI PAK: Init failed");
    }
    return sts;
}

mfxStatus FEI_EncPakInterface::Close()
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;
    if (m_pmfxENC)
    {
        sts = m_pmfxENC->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "FEI ENC: Close failed");
    }
    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "FEI PAK: Close failed");
    }
    m_RefInfo.Clear();
    return sts;
}

mfxStatus FEI_EncPakInterface::Reset(mfxU16 width, mfxU16 height, mfxU16 crop_w, mfxU16 crop_h)
{
    if (width && height && crop_w && crop_h)
    {
        m_videoParams_ENC.mfx.FrameInfo.Width  = width;
        m_videoParams_ENC.mfx.FrameInfo.Height = height;
        m_videoParams_ENC.mfx.FrameInfo.CropW  = crop_w;
        m_videoParams_ENC.mfx.FrameInfo.CropH  = crop_h;

        m_videoParams_PAK.mfx.FrameInfo.Width  = width;
        m_videoParams_PAK.mfx.FrameInfo.Height = height;
        m_videoParams_PAK.mfx.FrameInfo.CropW  = crop_w;
        m_videoParams_PAK.mfx.FrameInfo.CropH  = crop_h;
    }

    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;
    if (m_pmfxENC)
    {
        sts = m_pmfxENC->Reset(&m_videoParams_ENC);
        MSDK_CHECK_STATUS(sts, "FEI ENC: Reset failed");
    }
    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->Reset(&m_videoParams_PAK);
        MSDK_CHECK_STATUS(sts, "FEI PAK: Reset failed");
    }
    m_RefInfo.Clear();
    return sts;
}

mfxStatus FEI_EncPakInterface::QueryIOSurf(mfxFrameAllocRequest* enc_request, mfxFrameAllocRequest* pak_request)
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;
    if (m_pmfxENC && enc_request)
    {
        sts = m_pmfxENC->QueryIOSurf(&m_videoParams_ENC, enc_request);
        MSDK_CHECK_STATUS(sts, "FEI ENC: Reset failed");
    }
    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->QueryIOSurf(&m_videoParams_PAK, pak_request);
        MSDK_CHECK_STATUS(sts, "FEI PAK: Reset failed");
    }
    return sts;
}

mfxVideoParam* FEI_EncPakInterface::GetCommonVideoParams()
{
    return m_pmfxENC ? &m_videoParams_ENC : &m_videoParams_PAK;
}

mfxStatus FEI_EncPakInterface::UpdateVideoParam()
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;
    if (m_pmfxENC)
    {
        sts = m_pmfxENC->GetVideoParam(&m_videoParams_ENC);
        MSDK_CHECK_STATUS(sts, "FEI ENC: GetVideoParam failed");
    }
    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->GetVideoParam(&m_videoParams_PAK);
        MSDK_CHECK_STATUS(sts, "FEI PAK: GetVideoParam failed");
    }
    return sts;
}

void FEI_EncPakInterface::GetRefInfo(
            mfxU16 & picStruct,
            mfxU16 & refDist,
            mfxU16 & numRefFrame,
            mfxU16 & gopSize,
            mfxU16 & gopOptFlag,
            mfxU16 & idrInterval,
            mfxU16 & numRefActiveP,
            mfxU16 & numRefActiveBL0,
            mfxU16 & numRefActiveBL1,
            mfxU16 & bRefType)
{
    numRefActiveP   = m_pAppConfig->NumRefActiveP;
    numRefActiveBL0 = m_pAppConfig->NumRefActiveBL0;
    numRefActiveBL1 = m_pAppConfig->NumRefActiveBL1;

    bRefType = m_pAppConfig->bRefType == MFX_B_REF_UNKNOWN ? MFX_B_REF_OFF : m_pAppConfig->bRefType;

    picStruct   = m_pmfxENC ? m_videoParams_ENC.mfx.FrameInfo.PicStruct : m_videoParams_PAK.mfx.FrameInfo.PicStruct;
    refDist     = m_pmfxENC ? m_videoParams_ENC.mfx.GopRefDist          : m_videoParams_PAK.mfx.GopRefDist;
    numRefFrame = m_pmfxENC ? m_videoParams_ENC.mfx.NumRefFrame         : m_videoParams_PAK.mfx.NumRefFrame;
    gopSize     = m_pmfxENC ? m_videoParams_ENC.mfx.GopPicSize          : m_videoParams_PAK.mfx.GopPicSize;
    gopOptFlag  = m_pmfxENC ? m_videoParams_ENC.mfx.GopOptFlag          : m_videoParams_PAK.mfx.GopOptFlag;
    idrInterval = m_pmfxENC ? m_videoParams_ENC.mfx.IdrInterval         : m_videoParams_PAK.mfx.IdrInterval;
}

mfxStatus FEI_EncPakInterface::FillParameters()
{
    mfxStatus sts = MFX_ERR_NONE;

    /* Share ENCODE video parameters with other interfaces */
    m_pAppConfig->PipelineCfg.pEncVideoParam = &m_videoParams_ENC;
    m_pAppConfig->PipelineCfg.pPakVideoParam = &m_videoParams_PAK;

    m_videoParams_ENC.AllocId = m_allocId;

    m_videoParams_ENC.mfx.CodecId           = m_pAppConfig->CodecId;
    m_videoParams_ENC.mfx.TargetUsage       = 0; // FEI doesn't have support of
    m_videoParams_ENC.mfx.TargetKbps        = 0; // these features
    m_videoParams_ENC.mfx.RateControlMethod = MFX_RATECONTROL_CQP; // For now FEI work with RATECONTROL_CQP only

    sts = ConvertFrameRate(m_pAppConfig->dFrameRate, &m_videoParams_ENC.mfx.FrameInfo.FrameRateExtN, &m_videoParams_ENC.mfx.FrameInfo.FrameRateExtD);
    MSDK_CHECK_STATUS(sts, "ConvertFrameRate failed");

    /* Binary flag, 0 signals encoder to take frames in display order. PREENC, ENCPAK, ENC, PAK interfaces works only in encoded order */
    m_videoParams_ENC.mfx.EncodedOrder = mfxU16(m_pAppConfig->EncodedOrder || (m_pAppConfig->bPREENC || m_pAppConfig->bENCPAK || m_pAppConfig->bOnlyENC || m_pAppConfig->bOnlyPAK));

    m_videoParams_ENC.mfx.QPI = m_videoParams_ENC.mfx.QPP = m_videoParams_ENC.mfx.QPB = m_pAppConfig->QP;

    /* Specify memory type */
    m_videoParams_ENC.IOPattern = mfxU16(m_pAppConfig->bUseHWmemory ? MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    bool VPPrequired = m_pAppConfig->nWidth != m_pAppConfig->nDstWidth || m_pAppConfig->nHeight != m_pAppConfig->nDstHeight || m_pAppConfig->bDynamicRC;
    if (m_pAppConfig->bDECODE && !VPPrequired)
    {
        MSDK_CHECK_POINTER(m_pAppConfig->PipelineCfg.pDecodeVideoParam, MFX_ERR_NULL_PTR);
        /* in case of decoder without VPP copy FrameInfo from decoder */
        MSDK_MEMCPY_VAR(m_videoParams_ENC.mfx.FrameInfo, &m_pAppConfig->PipelineCfg.pDecodeVideoParam->mfx.FrameInfo, sizeof(mfxFrameInfo));
        m_videoParams_ENC.mfx.FrameInfo.PicStruct = m_pAppConfig->nPicStruct;

        m_pAppConfig->nWidth  = m_pAppConfig->nDstWidth  = m_videoParams_ENC.mfx.FrameInfo.CropW;
        m_pAppConfig->nHeight = m_pAppConfig->nDstHeight = m_videoParams_ENC.mfx.FrameInfo.CropH;
    }
    else if (VPPrequired)
    {
        MSDK_CHECK_POINTER(m_pAppConfig->PipelineCfg.pVppVideoParam, MFX_ERR_NULL_PTR);
        /* in case of VPP copy FrameInfo from VPP output */
        MSDK_MEMCPY_VAR(m_videoParams_ENC.mfx.FrameInfo, &m_pAppConfig->PipelineCfg.pVppVideoParam->vpp.Out, sizeof(mfxFrameInfo));
    }
    else
    {
        // frame info parameters
        m_videoParams_ENC.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_videoParams_ENC.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_videoParams_ENC.mfx.FrameInfo.PicStruct    = m_pAppConfig->nPicStruct;

        // Set frame size and crops
        // Width must be a multiple of 16
        // Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        m_videoParams_ENC.mfx.FrameInfo.Width  = MSDK_ALIGN16(m_pAppConfig->nDstWidth);
        m_videoParams_ENC.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE & m_videoParams_ENC.mfx.FrameInfo.PicStruct) ?
                                                 MSDK_ALIGN16(m_pAppConfig->nDstHeight) : MSDK_ALIGN32(m_pAppConfig->nDstHeight);

        m_videoParams_ENC.mfx.FrameInfo.CropX = 0;
        m_videoParams_ENC.mfx.FrameInfo.CropY = 0;
        m_videoParams_ENC.mfx.FrameInfo.CropW = m_pAppConfig->nDstWidth;
        m_videoParams_ENC.mfx.FrameInfo.CropH = m_pAppConfig->nDstHeight;
    }

    m_videoParams_ENC.AsyncDepth       = 1; //current limitation
    m_videoParams_ENC.mfx.GopRefDist   = (std::max)(m_pAppConfig->refDist, mfxU16(1));
    m_videoParams_ENC.mfx.GopPicSize   = (std::max)(m_pAppConfig->gopSize, mfxU16(1));
    m_videoParams_ENC.mfx.IdrInterval  = m_pAppConfig->nIdrInterval;
    m_videoParams_ENC.mfx.GopOptFlag   = m_pAppConfig->GopOptFlag;
    m_videoParams_ENC.mfx.CodecProfile = m_pAppConfig->CodecProfile;
    m_videoParams_ENC.mfx.CodecLevel   = m_pAppConfig->CodecLevel;

    /* Multi references and multi slices*/
    m_videoParams_ENC.mfx.NumRefFrame = (std::max)(m_pAppConfig->numRef, mfxU16(1));
    m_videoParams_ENC.mfx.NumSlice    = (std::max)(m_pAppConfig->numSlices, mfxU16(1));

    /* Create extended buffer to Init FEI ENCPAK */
    if (m_pmfxENC)
    {
        mfxExtFeiParam* pExtBufInit = new mfxExtFeiParam;
        MSDK_ZERO_MEMORY(*pExtBufInit);

        pExtBufInit->Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        pExtBufInit->Header.BufferSz = sizeof(mfxExtFeiParam);
        pExtBufInit->Func = MFX_FEI_FUNCTION_ENC;

        pExtBufInit->SingleFieldProcessing = mfxU16(m_bSingleFieldMode ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pExtBufInit));

        m_videoParams_ENC.ExtParam    = &m_InitExtParams_ENC[0]; // vector is stored linearly in memory
        m_videoParams_ENC.NumExtParam = (mfxU16)m_InitExtParams_ENC.size();
    }

    if (m_pmfxPAK)
    {
        MSDK_MEMCPY_VAR(m_videoParams_PAK, &m_videoParams_ENC, sizeof(mfxVideoParam));

        mfxExtFeiParam* pExtBufInit = new mfxExtFeiParam;
        MSDK_ZERO_MEMORY(*pExtBufInit);

        pExtBufInit->Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        pExtBufInit->Header.BufferSz = sizeof(mfxExtFeiParam);
        pExtBufInit->Func = MFX_FEI_FUNCTION_PAK;

        pExtBufInit->SingleFieldProcessing = mfxU16(m_bSingleFieldMode ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        m_InitExtParams_PAK.push_back(reinterpret_cast<mfxExtBuffer *>(pExtBufInit));

        m_videoParams_PAK.ExtParam    = &m_InitExtParams_PAK[0]; // vector is stored linearly in memory
        m_videoParams_PAK.NumExtParam = (mfxU16)m_InitExtParams_PAK.size();
    }

    mfxU32 nEncodedDataBufferSize = m_videoParams_ENC.mfx.FrameInfo.Width * m_videoParams_ENC.mfx.FrameInfo.Height * 4;
    sts = InitMfxBitstream(&m_mfxBS, nEncodedDataBufferSize);
    MSDK_CHECK_STATUS_SAFE(sts, "InitMfxBitstream failed", WipeMfxBitstream(&m_mfxBS));

    /* Init filepointers if some input buffers are specified */

    if (!m_pAppConfig->bPREENC && m_pMvPred_in == NULL && m_pAppConfig->mvinFile != NULL) //not load if we couple with PREENC
    {
        printf("Using MV input file: %s\n", m_pAppConfig->mvinFile);
        MSDK_FOPEN(m_pMvPred_in, m_pAppConfig->mvinFile, MSDK_CHAR("rb"));
        if (m_pMvPred_in == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mvinFile);
            exit(-1);
        }

        /* Alloc temporal buffers */
        if (m_pAppConfig->bRepackPreencMV)
        {
            m_tmpForMedian.resize(16);

            mfxU32 n_MB = m_pAppConfig->bDynamicRC ? m_pAppConfig->PipelineCfg.numMB_drc_max : m_pAppConfig->PipelineCfg.numMB_frame;
            m_tmpForReading.resize(n_MB);
        }
    }

    if (m_pRepackCtrl_in == NULL && m_pAppConfig->repackctrlFile != NULL)
    {
        printf("Using Frame Size control input file: %s\n", m_pAppConfig->repackctrlFile);
        MSDK_FOPEN(m_pRepackCtrl_in, m_pAppConfig->repackctrlFile, MSDK_CHAR("rb"));
        if (m_pRepackCtrl_in == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->repackctrlFile);
            exit(-1);
        }
    }

    if (m_pENC_MBCtrl_in == NULL && m_pAppConfig->mbctrinFile != NULL)
    {
        printf("Using MB control input file: %s\n", m_pAppConfig->mbctrinFile);
        MSDK_FOPEN(m_pENC_MBCtrl_in, m_pAppConfig->mbctrinFile, MSDK_CHAR("rb"));
        if (m_pENC_MBCtrl_in == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mbctrinFile);
            exit(-1);
        }
    }

    if (m_pMbQP_in == NULL && m_pAppConfig->mbQpFile != NULL)
    {
        printf("Use MB QP input file: %s\n", m_pAppConfig->mbQpFile);
        MSDK_FOPEN(m_pMbQP_in, m_pAppConfig->mbQpFile, MSDK_CHAR("rb"));
        if (m_pMbQP_in == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mbQpFile);
            exit(-1);
        }
    }

    if (m_pMBstat_out == NULL && m_pAppConfig->mbstatoutFile != NULL)
    {
        printf("Use MB distortion output file: %s\n", m_pAppConfig->mbstatoutFile);
        MSDK_FOPEN(m_pMBstat_out, m_pAppConfig->mbstatoutFile, MSDK_CHAR("wb"));
        if (m_pMBstat_out == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mbstatoutFile);
            exit(-1);
        }
    }

    bool onlyPAK = !m_pmfxENC && m_pmfxPAK;

    if (m_pMV_out == NULL && m_pAppConfig->mvoutFile != NULL)
    {
        printf("Use MV output file: %s\n", m_pAppConfig->mvoutFile);

        MSDK_FOPEN(m_pMV_out, m_pAppConfig->mvoutFile, onlyPAK ? MSDK_CHAR("rb") : MSDK_CHAR("wb"));
        if (m_pMV_out == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mvoutFile);
            exit(-1);
        }
    }

    if (m_pMBcode_out == NULL && m_pAppConfig->mbcodeoutFile != NULL)
    {
        printf("Use MB code output file: %s\n", m_pAppConfig->mbcodeoutFile);

        MSDK_FOPEN(m_pMBcode_out, m_pAppConfig->mbcodeoutFile, onlyPAK ? MSDK_CHAR("rb") : MSDK_CHAR("wb"));
        if (m_pMBcode_out == NULL) {
            mdprintf(stderr, "Can't open file %s\n", m_pAppConfig->mbcodeoutFile);
            exit(-1);
        }
    }

    if (m_pmfxPAK)
    {
        sts = m_FileWriter.Init(m_pAppConfig->dstFileBuff[0]);
        MSDK_CHECK_STATUS(sts, "FEI ENCPAK: FileWriter.Init failed");
    }

    return sts;
}

mfxStatus FEI_EncPakInterface::FillRefInfo(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    m_RefInfo.Clear();

    iTask* ref_task = NULL;
    mfxFrameSurface1* ref_surface = NULL;
    std::vector<mfxFrameSurface1*>::iterator rslt;
    mfxU32 k = 0, fid, n_l0, n_l1, numOfFields = eTask->m_fieldPicFlag ? 2 : 1;

    for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        fid = eTask->m_fid[fieldId];
        for (DpbFrame* instance = eTask->m_dpb[fid].Begin(); instance != eTask->m_dpb[fid].End(); instance++)
        {
            ref_task = m_inputTasks->GetTaskByFrameOrder(instance->m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_RefInfo.reference_frames.begin(), m_RefInfo.reference_frames.end(), ref_surface);

            if (rslt == m_RefInfo.reference_frames.end()){
                m_RefInfo.state[fieldId].dpb_idx.push_back((mfxU16)m_RefInfo.reference_frames.size());
                m_RefInfo.reference_frames.push_back(ref_surface);
            }
            else{
                m_RefInfo.state[fieldId].dpb_idx.push_back(static_cast<mfxU16>(std::distance(m_RefInfo.reference_frames.begin(), rslt)));
            }
        }

        /* in some cases l0 and l1 lists are equal, if so same ref lists for l0 and l1 should be used*/
        n_l0 = eTask->GetNBackward(fieldId);
        n_l1 = eTask->GetNForward(fieldId);

        if (!n_l0 && eTask->m_list0[fid].Size() && !(eTask->m_type[fid] & MFX_FRAMETYPE_I))
        {
            n_l0 = eTask->m_list0[fid].Size();
        }

        if (!n_l1 && eTask->m_list1[fid].Size() && (eTask->m_type[fid] & MFX_FRAMETYPE_B))
        {
            n_l1 = eTask->m_list1[fid].Size();
        }

        k = 0;
        for (mfxU8 const * instance = eTask->m_list0[fid].Begin(); k < n_l0 && instance != eTask->m_list0[fid].End(); instance++)
        {
            ref_task = m_inputTasks->GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_RefInfo.reference_frames.begin(), m_RefInfo.reference_frames.end(), ref_surface);

            if (rslt == m_RefInfo.reference_frames.end()){
                return MFX_ERR_MORE_SURFACE; // surface from L0 list not in DPB (should never happen)
            }
            else{
                m_RefInfo.state[fieldId].l0_idx.push_back(static_cast<mfxU16>(std::distance(m_RefInfo.reference_frames.begin(), rslt)));
            }

            //m_ref_info.state[fieldId].l0_idx.push_back(*instance & 127);
            m_RefInfo.state[fieldId].l0_parity.push_back((*instance) >> 7);
            k++;
        }

        k = 0;
        for (mfxU8 const * instance = eTask->m_list1[fid].Begin(); k < n_l1 && instance != eTask->m_list1[fid].End(); instance++)
        {
            ref_task = m_inputTasks->GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_RefInfo.reference_frames.begin(), m_RefInfo.reference_frames.end(), ref_surface);

            if (rslt == m_RefInfo.reference_frames.end()){
                return MFX_ERR_MORE_SURFACE; // surface from L0 list not in DPB (should never happen)
            }
            else{
                m_RefInfo.state[fieldId].l1_idx.push_back(static_cast<mfxU16>(std::distance(m_RefInfo.reference_frames.begin(), rslt)));
            }

            //m_ref_info.state[fieldId].l1_idx.push_back(*instance & 127);
            m_RefInfo.state[fieldId].l1_parity.push_back((*instance) >> 7);
            k++;
        }
    }

    return sts;
}

mfxStatus FEI_EncPakInterface::InitFrameParams(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    /* Alloc temporal buffers */
    if (m_pAppConfig->bRepackPreencMV && !m_tmpForReading.size())
    {
        mfxU32 n_MB = m_pAppConfig->bDynamicRC ? m_pAppConfig->PipelineCfg.numMB_drc_max : m_pAppConfig->PipelineCfg.numMB_frame;
        m_tmpForReading.resize(n_MB);
    }

    eTask->outPAK.Bs = &m_mfxBS;

    eTask->bufs = m_pExtBuffers->GetFreeSet();
    MSDK_CHECK_POINTER(eTask->bufs, MFX_ERR_NULL_PTR);

    sts = FillRefInfo(eTask); // get info to fill reference structures
    MSDK_CHECK_STATUS(sts, "FillRefInfo failed");

    bool is_I_frame = ExtractFrameType(*eTask, eTask->m_fieldPicFlag) & MFX_FRAMETYPE_I;

    if (m_pmfxENC)
    {
        eTask->in.NumFrameL0 = (mfxU16)m_RefInfo.reference_frames.size();
        eTask->in.NumFrameL1 = 0;
        eTask->in.L0Surface = &m_RefInfo.reference_frames[0];
        eTask->in.L1Surface = NULL;
        eTask->in.NumExtParam  = is_I_frame ? eTask->bufs->I_bufs.in.NumExtParam()  : eTask->bufs->PB_bufs.in.NumExtParam();
        eTask->in.ExtParam     = is_I_frame ? eTask->bufs->I_bufs.in.ExtParam()     : eTask->bufs->PB_bufs.in.ExtParam();
        eTask->out.NumExtParam = is_I_frame ? eTask->bufs->I_bufs.out.NumExtParam() : eTask->bufs->PB_bufs.out.NumExtParam();
        eTask->out.ExtParam    = is_I_frame ? eTask->bufs->I_bufs.out.ExtParam()    : eTask->bufs->PB_bufs.out.ExtParam();
    }

    if (m_pmfxPAK)
    {
        eTask->inPAK.NumFrameL0 = (mfxU16)m_RefInfo.reference_frames.size();
        eTask->inPAK.NumFrameL1 = 0;
        eTask->inPAK.L0Surface = &m_RefInfo.reference_frames[0];
        eTask->inPAK.L1Surface = NULL;
        eTask->inPAK.NumExtParam  = is_I_frame ? eTask->bufs->I_bufs.out.NumExtParam() : eTask->bufs->PB_bufs.out.NumExtParam();
        eTask->inPAK.ExtParam     = is_I_frame ? eTask->bufs->I_bufs.out.ExtParam()    : eTask->bufs->PB_bufs.out.ExtParam();
        eTask->outPAK.NumExtParam = is_I_frame ? eTask->bufs->I_bufs.in.NumExtParam()  : eTask->bufs->PB_bufs.in.NumExtParam();
        eTask->outPAK.ExtParam    = is_I_frame ? eTask->bufs->I_bufs.in.ExtParam()     : eTask->bufs->PB_bufs.in.ExtParam();
    }

    /* SPS, PPS, SliceHeader processing */
    mfxExtFeiSPS*         feiSPS         = NULL;
    mfxExtFeiPPS*         feiPPS         = NULL;
    mfxExtFeiSliceHeader* feiSliceHeader = NULL;

    int pMvPredId = 0;
    for (std::vector<mfxExtBuffer*>::iterator it = eTask->bufs->PB_bufs.in.buffers.begin();
         it != eTask->bufs->PB_bufs.in.buffers.end(); ++it)
    {
        switch ((*it)->BufferId)
        {
        case MFX_EXTBUFF_FEI_PPS:
            if (!feiPPS){ feiPPS = reinterpret_cast<mfxExtFeiPPS*>(*it); }
            break;

        case MFX_EXTBUFF_FEI_SPS:
            feiSPS = reinterpret_cast<mfxExtFeiSPS*>(*it);
            break;

        case MFX_EXTBUFF_FEI_SLICE:
            if (!feiSliceHeader){ feiSliceHeader = reinterpret_cast<mfxExtFeiSliceHeader*>(*it); }
            break;

        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (m_pMvPred_in)
            {
                mfxExtFeiEncMVPredictors* pMvPredBuf = reinterpret_cast<mfxExtFeiEncMVPredictors*>(*it);

                if (!(ExtractFrameType(*eTask, pMvPredId) & MFX_FRAMETYPE_I))
                {
                    if (m_pAppConfig->bRepackPreencMV)
                    {
                        SAFE_FREAD(&m_tmpForReading[0], sizeof(m_tmpForReading[0])*pMvPredBuf->NumMBAlloc, 1, m_pMvPred_in, MFX_ERR_MORE_DATA);
                        repackPreenc2Enc(&m_tmpForReading[0], pMvPredBuf->MB, pMvPredBuf->NumMBAlloc, &m_tmpForMedian[0]);
                    }
                    else {
                        SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, m_pMvPred_in, MFX_ERR_MORE_DATA);
                    }
                }
                else{
                    int shft = m_pAppConfig->bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    SAFE_FSEEK(m_pMvPred_in, shft*pMvPredBuf->NumMBAlloc, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
                pMvPredId++;
            }
            break;
        } // switch (eTask->bufs->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->bufs->PB_bufs.in.NumExtParam; i++)

    for (mfxU32 fieldId = 0; fieldId < mfxU32(eTask->m_fieldPicFlag ? 2 : 1); fieldId++)
    {
        if (feiPPS)
        {
            feiPPS[fieldId].IDRPicFlag        = (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_IDR) ? 1 : 0;
            feiPPS[fieldId].NumRefIdxL0Active = (mfxU16)m_RefInfo.state[fieldId].l0_idx.size();
            feiPPS[fieldId].NumRefIdxL1Active = (mfxU16)m_RefInfo.state[fieldId].l1_idx.size();
            feiPPS[fieldId].ReferencePicFlag  = (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_REF) ? 1 : 0;
            feiPPS[fieldId].FrameNum          = eTask->m_frameNum;

            memset(feiPPS[fieldId].ReferenceFrames, -1, 16 * sizeof(mfxU16));
            memcpy(feiPPS[fieldId].ReferenceFrames, &m_RefInfo.state[fieldId].dpb_idx[0], sizeof(mfxU16)*m_RefInfo.state[fieldId].dpb_idx.size());
        }

        if (feiSliceHeader)
        {
            MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_NULL_PTR);

            for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSliceAlloc; i++)
            {
                MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL0, 32);
                MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL1, 32);

                feiSliceHeader[fieldId].Slice[i].SliceType         = FrameTypeToSliceType(ExtractFrameType(*eTask, fieldId));
                feiSliceHeader[fieldId].Slice[i].NumRefIdxL0Active = (mfxU16)m_RefInfo.state[fieldId].l0_idx.size();
                feiSliceHeader[fieldId].Slice[i].NumRefIdxL1Active = (mfxU16)m_RefInfo.state[fieldId].l1_idx.size();
                feiSliceHeader[fieldId].Slice[i].IdrPicId          = eTask->m_frameIdrCounter;

                for (mfxU32 k = 0; k < m_RefInfo.state[fieldId].l0_idx.size(); k++)
                {
                    feiSliceHeader[fieldId].Slice[i].RefL0[k].Index       = m_RefInfo.state[fieldId].l0_idx[k];
                    feiSliceHeader[fieldId].Slice[i].RefL0[k].PictureType = (mfxU16)(!eTask->m_fieldPicFlag ? MFX_PICTYPE_FRAME :
                        (m_RefInfo.state[fieldId].l0_parity[k] ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                }
                for (mfxU32 k = 0; k < m_RefInfo.state[fieldId].l1_idx.size(); k++)
                {
                    feiSliceHeader[fieldId].Slice[i].RefL1[k].Index       = m_RefInfo.state[fieldId].l1_idx[k];
                    feiSliceHeader[fieldId].Slice[i].RefL1[k].PictureType = (mfxU16)(!eTask->m_fieldPicFlag ? MFX_PICTYPE_FRAME :
                        (m_RefInfo.state[fieldId].l1_parity[k] ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                }
            }
        } // if (feiSliceHeader)
    }

    /* the rest ext buffers */
    int encCtrlId = 0;
    for (int i = 0; i < eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_CTRL:
        {
            mfxExtFeiEncFrameCtrl* feiEncCtrl = reinterpret_cast<mfxExtFeiEncFrameCtrl*>(eTask->in.ExtParam[i]);
            feiEncCtrl->MVPredictor = (ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_I) ? 0 : (m_pMvPred_in != NULL);

            // adjust ref window size if search window is 0
            if (m_pAppConfig->SearchWindow == 0 && m_pmfxENC)
            {
                // window size is limited to 1024 for bi-prediction
                bool adjust_window_size = (ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_B) && m_pAppConfig->RefHeight * m_pAppConfig->RefWidth > 1024;

                feiEncCtrl->RefHeight = adjust_window_size ? 32 : m_pAppConfig->RefHeight;
                feiEncCtrl->RefWidth  = adjust_window_size ? 32 : m_pAppConfig->RefWidth;
            }

            /* Driver requires these fields to be zero in case of feiEncCtrl->MVPredictor == false
            but MSDK lib will adjust them to zero if application doesn't */
            feiEncCtrl->NumMVPredictors[0] = feiEncCtrl->MVPredictor * m_pAppConfig->PipelineCfg.numOfPredictors[encCtrlId][0];
            feiEncCtrl->NumMVPredictors[1] = feiEncCtrl->MVPredictor * m_pAppConfig->PipelineCfg.numOfPredictors[encCtrlId][1];

            encCtrlId++;
        }
        break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (m_pENC_MBCtrl_in)
            {
                mfxExtFeiEncMBCtrl* pMbEncCtrl = reinterpret_cast<mfxExtFeiEncMBCtrl*>(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, m_pENC_MBCtrl_in, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_QP:
            if (m_pMbQP_in)
            {
                mfxExtFeiEncQP* pMbQP = reinterpret_cast<mfxExtFeiEncQP*>(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, m_pMbQP_in, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<eTask->in.NumExtParam; i++)

    mdprintf(stderr, "enc: %d t: %d %d\n", eTask->m_frameOrder, (eTask->m_type[0] & MFX_FRAMETYPE_IPB), (eTask->m_type[1] & MFX_FRAMETYPE_IPB));
    return sts;
}

mfxStatus FEI_EncPakInterface::ReadPAKdata(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    for (int i = 0; i < eTask->inPAK.NumExtParam; i++){
        switch (eTask->inPAK.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (m_pMV_out){
                mfxExtFeiEncMV* mvBuf = reinterpret_cast<mfxExtFeiEncMV*>(eTask->inPAK.ExtParam[i]);
                SAFE_FREAD(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, m_pMV_out, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (m_pMBcode_out){
                mfxExtFeiPakMBCtrl* mbcodeBuf = reinterpret_cast<mfxExtFeiPakMBCtrl*>(eTask->inPAK.ExtParam[i]);
                SAFE_FREAD(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, m_pMBcode_out, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->inPAK.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->inPAK.NumExtParam; i++)

    return sts;
}

mfxStatus FEI_EncPakInterface::AllocateSufficientBuffer()
{
    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_NULL_PTR);
    mfxStatus sts = m_pmfxPAK->GetVideoParam(&par);
    MSDK_CHECK_STATUS(sts, "m_pmfxPAK->GetVideoParam failed");

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(&m_mfxBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_STATUS_SAFE(sts, "ExtendMfxBitstream failed", WipeMfxBitstream(&m_mfxBS));

    return sts;
}

mfxStatus FEI_EncPakInterface::EncPakOneFrame(iTask* eTask)
{
    MFX_ITT_TASK("EncPakOneFrame");
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = InitFrameParams(eTask);
    MSDK_CHECK_STATUS(sts, "FEI ENCPAK: InitFrameParams failed");

    for (;;)
    {
        mdprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[eTask->m_fid[0]], eTask->m_type[eTask->m_fid[1]]);

        for (int i = 0; i < 1 + m_pAppConfig->bFieldProcessingMode; ++i)
        {
            if (m_pmfxENC)
            {
                sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &m_SyncPoint);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);

                sts = m_pmfxSession->SyncOperation(m_SyncPoint, MSDK_WAIT_INTERVAL);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "synced : %d\n", sts);
            }

            if (i == 0)
            {
                MSDK_DEBUG
                /* In case of PAK only we need to read data from file */
                if (m_pAppConfig->bOnlyPAK)
                {
                    sts = ReadPAKdata(eTask);
                    MSDK_CHECK_STATUS(sts, "ReadPAKdata failed");
                }
            }

            if (m_pmfxPAK)
            {
                sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &m_SyncPoint);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);

                sts = m_pmfxSession->SyncOperation(m_SyncPoint, MSDK_WAIT_INTERVAL);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "synced : %d\n", sts);
            }
        }

        if (MFX_ERR_NONE < sts && !m_SyncPoint) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts){
                WaitForDeviceToBecomeFree(*m_pmfxSession, m_SyncPoint, sts);
            }
        }
        else if (MFX_ERR_NONE < sts && m_SyncPoint)
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer();
            MSDK_CHECK_STATUS(sts, "AllocateSufficientBuffer failed");
        }
        else
        {
            break;
        }
    }
    MSDK_CHECK_STATUS(sts, "FEI ENCPAK failed to encode frame");

    if (m_pmfxPAK)
    {
        sts = m_FileWriter.WriteNextFrame(&m_mfxBS);
        MSDK_CHECK_STATUS(sts, "FEI ENCODE: WriteNextFrame failed");
    }

    if (m_pmfxENC)
    {
        sts = FlushOutput(eTask);
        MSDK_CHECK_STATUS(sts, "DropENCPAKoutput failed");
    }

    return sts;
}

mfxStatus FEI_EncPakInterface::FlushOutput(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    int mvBufId = 0;
    for (int i = 0; i < eTask->out.NumExtParam; i++)
    {
        switch (eTask->out.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (m_pMV_out)
            {
                mfxExtFeiEncMV* mvBuf = reinterpret_cast<mfxExtFeiEncMV*>(eTask->out.ExtParam[i]);

                if (!(ExtractFrameType(*eTask, mvBufId) & MFX_FRAMETYPE_I)){
                    SAFE_FWRITE(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, m_pMV_out, MFX_ERR_MORE_DATA);
                }
                else
                {
                    for (mfxU32 k = 0; k < mvBuf->NumMBAlloc; k++)
                    {
                        SAFE_FWRITE(&m_tmpMBencMV, sizeof(m_tmpMBencMV), 1, m_pMV_out, MFX_ERR_MORE_DATA);
                    }
                }
                mvBufId++;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (m_pMBstat_out){
                mfxExtFeiEncMBStat* mbstatBuf = reinterpret_cast<mfxExtFeiEncMBStat*>(eTask->out.ExtParam[i]);
                SAFE_FWRITE(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, m_pMBstat_out, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (m_pMBcode_out){
                mfxExtFeiPakMBCtrl* mbcodeBuf = reinterpret_cast<mfxExtFeiPakMBCtrl*>(eTask->out.ExtParam[i]);
                SAFE_FWRITE(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, m_pMBcode_out, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->out.ExtParam[i]->BufferId)
    } // for(int i=0; i<eTask->out.NumExtParam; i++)

    return MFX_ERR_NONE;
}

mfxStatus FEI_EncPakInterface::ResetState()
{
    mfxStatus sts = MFX_ERR_NONE;

    // mark sync point as free
    m_SyncPoint = NULL;

    // prepare bit stream
    m_mfxBS.DataOffset = 0;
    m_mfxBS.DataLength = 0;

    if (m_pmfxPAK)
    {
        // reset FileWriter
        sts = m_FileWriter.Init(m_pAppConfig->dstFileBuff[0]);
        MSDK_CHECK_STATUS(sts, "FEI ENCPAK: FileWriter.Init failed");
    }

    SAFE_FSEEK(m_pMvPred_in,     0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pENC_MBCtrl_in, 0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pMbQP_in,       0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pRepackCtrl_in, 0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pMBstat_out,    0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pMV_out,        0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(m_pMBcode_out,    0, SEEK_SET, MFX_ERR_MORE_DATA);

    return sts;
}
