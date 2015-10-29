//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//


#include "pipeline_fei.h"
#include "refListsManagement_fei.h"
#include "sysmem_allocator.h"

#if D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "d3d_device.h"
#include "d3d11_device.h"
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_allocator.h"
#include "vaapi_device.h"
#endif

#if _DEBUG
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

mfxU8 GetDefaultPicOrderCount(mfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        return 0;
    if (par.mfx.GopRefDist > 1)
        return 0;

    return 2;
}

static FILE* mbstatout   = NULL;
static FILE* mvout       = NULL;
static FILE* mvENCPAKout = NULL;
static FILE* mbcodeout   = NULL;
static FILE* pMvPred     = NULL;
static FILE* pEncMBs     = NULL;
static FILE* pPerMbQP    = NULL;


CEncTaskPool::CEncTaskPool()
{
    m_pTasks            = NULL;
    m_pmfxSession       = NULL;
    m_nTaskBufferStart  = 0;
    m_nPoolSize         = 0;
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter)
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_POINTER(pWriter, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize,   0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    // nPoolSize must be even in case of 2 output bitstreams
    if (pOtherWriter && (0 != nPoolSize % 2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_pmfxSession = pmfxSession;
    m_nPoolSize   = nPoolSize;

    m_pTasks = new sTask [m_nPoolSize];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;

    if (pOtherWriter) // 2 bitstreams on output
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i+=2)
        {
            sts = m_pTasks[i+0].Init(nBufferSize, pWriter);
            sts = m_pTasks[i+1].Init(nBufferSize, pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    else
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts  = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
    {
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);

        if (MFX_ERR_NONE == sts)
        {
            //Write output for encode
            mfxBitstream& bs = m_pTasks[m_nTaskBufferStart].mfxBS;
            dropENCODEoutput(bs);


            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // move task buffer start to the next executing task
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++)
            {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
                {
                    break;
                }
            }
        }

        return sts;
    }
    else
    {
        return MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }
}

mfxU32 CEncTaskPool::GetFreeTaskIndex()
{
    mfxU32 off = 0;

    if (m_pTasks)
    {
        for (off = 0; off < m_nPoolSize; off++)
        {
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP)
            {
                break;
            }
        }
    }

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(sTask **ppTask)
{
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize)
    {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];

    return MFX_ERR_NONE;
}

void CEncTaskPool::Close()
{
    if (m_pTasks)
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            m_pTasks[i].Close();
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;
}

sTask::sTask()
    : EncSyncP(0)
    , pWriter(NULL)
{
    MSDK_ZERO_MEMORY(mfxBS);
}

mfxStatus sTask::Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pwriter)
{
    Close();

    pWriter = pwriter;

    mfxStatus sts = Reset();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));

    return sts;
}

mfxStatus sTask::Close()
{
    WipeMfxBitstream(&mfxBS);
    EncSyncP = 0;

    return MFX_ERR_NONE;
}

mfxStatus sTask::WriteBitstream()
{
    if (!pWriter)
        return MFX_ERR_NOT_INITIALIZED;

    return pWriter->WriteNextFrame(&mfxBS);
}

mfxStatus sTask::Reset()
{
    // mark sync point as free
    EncSyncP = NULL;

    // prepare bit stream
    mfxBS.DataOffset = 0;
    mfxBS.DataLength = 0;

    for (std::list<std::pair<bufSet*, mfxFrameSurface1*> >::iterator it = bufs.begin(); it != bufs.end(); )
    {
        if ((*it).second->Data.Locked == 0)
        {
            (*it).first->vacant = true;
            it = bufs.erase(it);
            //break;
        }
        else{
            ++it;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams)
{
    m_encpakParams = *pInParams;

    m_mfxEncParams.mfx.CodecId                 = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.TargetKbps              = pInParams->nBitRate; // in Kbps
    /*For now FEI work with RATECONTROL_CQP only*/
    m_mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    m_mfxEncParams.mfx.EncodedOrder = pInParams->EncodedOrder; // binary flag, 0 signals encoder to take frames in display order

    if (0 != pInParams->QP)
        m_mfxEncParams.mfx.QPI = m_mfxEncParams.mfx.QPP = m_mfxEncParams.mfx.QPB = pInParams->QP;

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = pInParams->nPicStruct;

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nDstWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nDstHeight;
    m_mfxEncParams.AsyncDepth      = 1; //current limitation
    m_mfxEncParams.mfx.GopRefDist  = pInParams->refDist > 0 ? pInParams->refDist : 1;
    m_mfxEncParams.mfx.GopPicSize  = pInParams->gopSize > 0 ? pInParams->gopSize : 1;
    m_mfxEncParams.mfx.IdrInterval = pInParams->nIdrInterval;
    m_mfxEncParams.mfx.GopOptFlag  = pInParams->GopOptFlag;
    m_mfxEncParams.mfx.CodecProfile = pInParams->CodecProfile;
    m_mfxEncParams.mfx.CodecLevel   = pInParams->CodecLevel;

    /* Multi references and multi slices*/
    if (pInParams->numRef == 0)
    {
        m_mfxEncParams.mfx.NumRefFrame = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumRefFrame = pInParams->numRef;
    }

    if (pInParams->numSlices == 0)
    {
        m_mfxEncParams.mfx.NumSlice = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumSlice = pInParams->numSlices;
    }

    // configure the depth of the look ahead BRC if specified in command line
    if (pInParams->bRefType || pInParams->Trellis)
    {
        m_CodingOption2.BRefType = pInParams->bRefType;
        m_CodingOption2.Trellis  = pInParams->Trellis;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption2);
    }

    if(pInParams->bENCODE)
    {
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENCPAK;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);
    }

    if(pInParams->bPREENC)
    {
#if 0          //find better way
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);
#endif
    }

    //mfxExtBuffer * test_ext_buf = (mfxExtBuffer *)&m_encpakInit;

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        //m_mfxEncParams.ExtParam = &test_ext_buf;
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxVppParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    // input frame info
    m_mfxVppParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    m_mfxVppParams.vpp.In.PicStruct = pInParams->nPicStruct;;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxVppParams.vpp.In.FrameRateExtN, &m_mfxVppParams.vpp.In.FrameRateExtD);

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxVppParams.vpp.In.Width = MSDK_ALIGN16(pInParams->nWidth);
    m_mfxVppParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.In.PicStruct) ?
        MSDK_ALIGN16(pInParams->nHeight) : MSDK_ALIGN32(pInParams->nHeight);

    // set crops in input mfxFrameInfo for correct work of file reader
    // VPP itself ignores crops at initialization
    m_mfxVppParams.vpp.In.CropW = pInParams->nWidth;
    m_mfxVppParams.vpp.In.CropH = pInParams->nHeight;

    // fill output frame info
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.Out, &m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));

    // only resizing is supported
    m_mfxVppParams.vpp.Out.Width = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxVppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.Out.PicStruct) ?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    // configure and attach external parameters
    m_VppDoNotUse.NumAlg = 4;
    m_VppDoNotUse.AlgList = new mfxU32[m_VppDoNotUse.NumAlg];
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList, MFX_ERR_MEMORY_ALLOC);
    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE; // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_VppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL; // turn off detail enhancement (on by default)
    m_VppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP; // turn off processing amplified (on by default)
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    m_mfxVppParams.ExtParam = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();

    m_mfxVppParams.AsyncDepth = 1; //current limitation

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxDecodeParams(sInputParams *pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);

    m_mfxDecParams.AsyncDepth = 1; //current limitation

    // set video type in parameters
    m_mfxDecParams.mfx.CodecId = pInParams->DecodeId;

    m_BSReader.Init(pInParams->strSrcFile);
    InitMfxBitstream(&m_mfxBS, 1024 * 1024);

    // read a portion of data for DecodeHeader function
    sts = m_BSReader.ReadNextFrame(&m_mfxBS);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;
    else
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // try to find a sequence header in the stream
    // if header is not found this function exits with error (e.g. if device was lost and there's no header in the remaining stream)
    for (;;)
    {
        // parse bit stream and fill mfx params
        sts = m_pmfxDECODE->DecodeHeader(&m_mfxBS, &m_mfxDecParams);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_mfxBS.MaxLength == m_mfxBS.DataLength)
            {
                sts = ExtendMfxBitstream(&m_mfxBS, m_mfxBS.MaxLength * 2);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            // read a portion of data for DecodeHeader function
            sts = m_BSReader.ReadNextFrame(&m_mfxBS);
            if (MFX_ERR_MORE_DATA == sts)
                return sts;
            else
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


            continue;
        }
        else
            break;
    }

    // if input stream header doesn't contain valid values use default (30.0)
    if (!(m_mfxDecParams.mfx.FrameInfo.FrameRateExtN * m_mfxDecParams.mfx.FrameInfo.FrameRateExtD))
    {
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtN = 30;
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtD = 1;
    }

     // specify memory type
     if (pInParams->bUseHWLib)
         m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
     else
         m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;
#if D3D_SURFACES_SUPPORT
    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

#if MFX_D3D11_SUPPORT
    if (D3D11_MEMORY == m_memType)
        m_hwdev = new CD3D11Device();
    else
#endif // #if MFX_D3D11_SUPPORT
        m_hwdev = new CD3D9Device();

    if (NULL == m_hwdev)
        return MFX_ERR_MEMORY_ALLOC;

    sts = m_hwdev->Init(
        window,
        0,
        MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#elif LIBVA_SUPPORT
    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::ResetDevice()
{
    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
        return m_hwdev->Reset();
    }
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    //MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest DecRequest;
    mfxFrameAllocRequest VppRequest;
    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest ReconRequest;

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder

    MSDK_ZERO_MEMORY(DecRequest);
    MSDK_ZERO_MEMORY(VppRequest);
    MSDK_ZERO_MEMORY(EncRequest);
    MSDK_ZERO_MEMORY(ReconRequest);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.

    if (m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    m_maxQueueLength = m_refDist * 2 + m_mfxEncParams.AsyncDepth;
    /* temporary solution for a while for PREENC + ENC cases
    * (It required to calculate accurate formula for all usage cases)
    * this is not optimal from memory usage consumption */
    m_maxQueueLength += m_mfxEncParams.mfx.NumRefFrame + 1;

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + (m_nAsyncDepth - 1);
    if ((m_encpakParams.bPREENC) || (m_encpakParams.bENCPAK) || (m_encpakParams.bENCODE) ||
        (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))
    {
        nEncSurfNum = m_maxQueueLength;                   // temporary solution
        nEncSurfNum += m_pmfxVPP ? m_refDist + 1 : 0;     // to be improved in next release
    }

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->QueryIOSurf(&m_mfxDecParams, &DecRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_decodePoolSize = DecRequest.NumFrameSuggested;
        DecRequest.NumFrameMin = DecRequest.NumFrameSuggested;

        if (!m_pmfxVPP)
        {
            // in case of Decode and absence of VPP we use the same surface pool for the entire pipeline
            nEncSurfNum += m_mfxEncParams.mfx.NumRefFrame + 1;
            if (DecRequest.NumFrameSuggested <= nEncSurfNum)
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = nEncSurfNum;
            else
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = m_decodePoolSize + nEncSurfNum;
            if ((m_pmfxENC) || (m_pmfxPAK)) // plus reconstructed frames for PAK
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = DecRequest.NumFrameSuggested + m_mfxEncParams.mfx.GopRefDist * 2 + m_mfxEncParams.AsyncDepth;
        }
        MSDK_MEMCPY_VAR(DecRequest.Info, &(m_mfxDecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        DecRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        // alloc frames for decoder
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &DecRequest, &m_DecResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // prepare mfxFrameSurface1 array for decoder
        m_pDecSurfaces = new mfxFrameSurface1[m_DecResponse.NumFrameActual];
        MSDK_CHECK_POINTER(m_pDecSurfaces, MFX_ERR_MEMORY_ALLOC);
        MSDK_ZERO_ARRAY(m_pDecSurfaces, m_DecResponse.NumFrameActual);

        for (int i = 0; i < m_DecResponse.NumFrameActual; i++)
        {
            MSDK_MEMCPY_VAR(m_pDecSurfaces[i].Info, &(m_mfxDecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
            m_pDecSurfaces[i].Data.MemId = m_DecResponse.mids[i];
        }

        if (!m_pmfxVPP)
            return MFX_ERR_NONE;
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->QueryIOSurf(&m_mfxVppParams, &VppRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        VppRequest.NumFrameMin = VppRequest.NumFrameSuggested;
        if (!m_pmfxDECODE)
        {
            // in case of absence of DECODE we need to allocate input surfaces for VPP
            sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &VppRequest, &m_VppResponse);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // prepare mfxFrameSurface1 array for VPP
            m_pVppSurfaces = new mfxFrameSurface1[m_VppResponse.NumFrameActual];
            MSDK_CHECK_POINTER(m_pVppSurfaces, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_ARRAY(m_pVppSurfaces, m_VppResponse.NumFrameActual);

            for (int i = 0; i < m_VppResponse.NumFrameActual; i++)
            {
                MSDK_MEMCPY_VAR(m_pVppSurfaces[i].Info, &(m_mfxVppParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

                if (m_bExternalAlloc)
                    m_pVppSurfaces[i].Data.MemId = m_VppResponse.mids[i];
                else
                {
                    // get YUV pointers
                    sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_VppResponse.mids[i], &(m_pVppSurfaces[i].Data));
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
            }
        }
    }

    // prepare allocation requests

    EncRequest.NumFrameMin = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

    //EncRequest.AllocId = PREENC_ALLOC;
    if ((m_pmfxPREENC) || m_pmfxDECODE)
    {
        EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    }
    else if (m_pmfxVPP)
        EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    else if ((m_pmfxPAK) || (m_pmfxENC))
        EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /* Alloc reconstructed surfaces for PAK */
    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        //ReconRequest.AllocId = PAK_ALLOC;
        MSDK_MEMCPY_VAR(ReconRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        ReconRequest.NumFrameMin = nEncSurfNum; // m_mfxEncParams.mfx.GopRefDist * (m_pmfxVPP && !m_pmfxDECODE ? 4 : 2) + m_mfxEncParams.AsyncDepth;        // temporary solution
        ReconRequest.NumFrameSuggested = nEncSurfNum; //m_mfxEncParams.mfx.GopRefDist * (m_pmfxVPP && ! m_pmfxDECODE ? 4 : 2) + m_mfxEncParams.AsyncDepth;  // to be improved in next release
        /* type of reconstructed surfaces for PAK should be same as for Media SDK's decoders!!!
         * Because libVA required reconstructed surfaces for vaCreateContext */
        ReconRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &ReconRequest, &m_ReconResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = new mfxFrameSurface1[m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);
    MSDK_ZERO_ARRAY(m_pEncSurfaces, m_EncResponse.NumFrameActual);

    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        m_pReconSurfaces = new mfxFrameSurface1[m_ReconResponse.NumFrameActual];
        MSDK_CHECK_POINTER(m_pReconSurfaces, MFX_ERR_MEMORY_ALLOC);
        MSDK_ZERO_ARRAY(m_pReconSurfaces, m_ReconResponse.NumFrameActual);
    }

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        MSDK_MEMCPY_VAR(m_pEncSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    for (int i = 0; i < m_ReconResponse.NumFrameActual; i++)
    {
        MSDK_MEMCPY_VAR(m_pReconSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pReconSurfaces[i].Data.MemId = m_ReconResponse.mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_ReconResponse.mids[i], &(m_pReconSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
#if D3D_SURFACES_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        mfxHDL hdl = NULL;
        mfxHandleType hdl_t =
        #if MFX_D3D11_SUPPORT
            D3D11_MEMORY == m_memType ? MFX_HANDLE_D3D11_DEVICE :
        #endif // #if MFX_D3D11_SUPPORT
            MFX_HANDLE_D3D9_DEVICE_MANAGER;

        sts = m_hwdev->GetHandle(hdl_t, &hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // handle is needed for HW library only
        mfxIMPL impl = 0;
        m_mfxSession.QueryIMPL(&impl);

        if (impl != MFX_IMPL_SOFTWARE)
        {
            sts = m_mfxSession.SetHandle(hdl_t, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (m_encpakParams.bDECODE){
                sts = m_decode_mfxSession.SetHandle(hdl_t, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(hdl_t, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }

        // create D3D allocator
#if MFX_D3D11_SUPPORT
        if (D3D11_MEMORY == m_memType)
        {
            m_pMFXAllocator = new D3D11FrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3D11AllocatorParams *pd3dAllocParams = new D3D11AllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pDevice = reinterpret_cast<ID3D11Device *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }
        else
#endif // #if MFX_D3D11_SUPPORT
        {
            m_pMFXAllocator = new D3DFrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to Media SDK */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        m_bExternalAlloc = true;
#endif
#ifdef LIBVA_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        /* It's possible to skip failed result here and switch to SW implementation,
        but we don't process this way */

        mfxHDL hdl = NULL;
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        // provide device manager to MediaSDK
        sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create VAAPI allocator
        m_pMFXAllocator = new vaapiFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
        MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);

        p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        m_bExternalAlloc = true;
#endif
    }
    else
    {
#ifdef LIBVA_SUPPORT
        //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
        mfxIMPL impl;
        m_mfxSession.QueryIMPL(&impl);

        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
        {
            sts = CreateHWDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxHDL hdl = NULL;
            sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            // provide device manager to MediaSDK
            sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (m_encpakParams.bDECODE){
                sts = m_decode_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }
#endif

        // create system memory allocator
        m_pMFXAllocator = new SysMemFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        /* In case of system memory we demonstrate "no external allocator" usage model.
        We don't call SetAllocator, Media SDK uses internal allocator.
        We use system memory allocator simply as a memory manager for application*/
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pDecSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pVppSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pReconSurfaces);

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_DecResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_ReconResponse);
    }
}

void CEncodingPipeline::DeleteHWDevice()
{
    MSDK_SAFE_DELETE(m_hwdev);
}

void CEncodingPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);

    DeleteHWDevice();
}

CEncodingPipeline::CEncodingPipeline()
{
    //m_pUID = NULL;
    m_pmfxDECODE          = NULL;
    m_pmfxVPP             = NULL;
    m_pmfxPREENC          = NULL;
    m_pmfxENC             = NULL;
    m_pmfxPAK             = NULL;
    m_pmfxENCODE          = NULL;
    m_pMFXAllocator       = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType             = D3D9_MEMORY; //only hw memory is supported
    m_bExternalAlloc      = false;
    m_pDecSurfaces        = NULL;
    m_pEncSurfaces        = NULL;
    m_pVPP_mfxSession     = NULL;
    m_pReconSurfaces      = NULL;
    m_nAsyncDepth         = 0;
    m_refFrameCounter     = 0;
    m_LastDecSyncPoint    = 0;

    MSDK_ZERO_MEMORY(m_mfxBS);

    m_FileWriters.first = m_FileWriters.second = NULL;

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(m_VppDoNotUse);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_DecResponse);
    MSDK_ZERO_MEMORY(m_EncResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename)
{
    MSDK_CHECK_ERROR(ppWriter, NULL, MFX_ERR_NULL_PTR);

    MSDK_SAFE_DELETE(*ppWriter);
    *ppWriter = new CSmplBitstreamWriter;
    MSDK_CHECK_POINTER(*ppWriter, MFX_ERR_MEMORY_ALLOC);
    mfxStatus sts = (*ppWriter)->Init(filename);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::InitFileWriters(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // prepare output file writers
    if((pParams->bENCODE) || (pParams->bENCPAK) || (pParams->bOnlyPAK)){ //need only for ENC+PAK, only ENC + only PAK, only PAK
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    m_refDist = pParams->refDist > 0 ? pParams->refDist : 1;
    m_gopSize = pParams->gopSize > 0 ? pParams->gopSize : 1;

    // prepare input file reader
    if (!pParams->bDECODE)
    {
        sts = m_FileReader.Init(pParams->strSrcFile,
            pParams->ColorFormat,
            0,
            pParams->srcFileBuff);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    sts = InitFileWriters(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Init session
    if (pParams->bUseHWLib)
    {
        // try searching on all display adapters
        mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;

        // if d3d11 surfaces are used ask the library to run acceleration through D3D11
        // feature may be unsupported due to OS or MSDK API version
        if (D3D11_MEMORY == pParams->memType)
            impl |= MFX_IMPL_VIA_D3D11;

        sts = m_mfxSession.Init(impl, NULL);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);

        if (pParams->bDECODE){
            sts = m_decode_mfxSession.Init(impl, NULL);

            // MSDK API version may not support multiple adapters - then try initialize on the default
            if (MFX_ERR_NONE != sts)
                sts = m_decode_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);
        }

        if(pParams->bPREENC){
            sts = m_preenc_mfxSession.Init(impl, NULL);

            // MSDK API version may not support multiple adapters - then try initialize on the default
            if (MFX_ERR_NONE != sts)
               sts = m_preenc_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);
        }
    }
    else
    {
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);

        if (pParams->bDECODE)
            sts = m_decode_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);

        if(pParams->bPREENC){
            sts = m_preenc_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);
        }
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (pParams->bDECODE)
    {
        // create decoder
        m_pmfxDECODE = new MFXVideoDECODE(m_decode_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxDECODE, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxDecodeParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // create preprocessor if resizing was requested from command line
    if (pParams->nWidth  != pParams->nDstWidth ||
        pParams->nHeight != pParams->nDstHeight)
    {
        if (pParams->bDECODE)
            m_pVPP_mfxSession = &m_decode_mfxSession;
        else if (pParams->bPREENC)
            m_pVPP_mfxSession = &m_preenc_mfxSession;
        else
            m_pVPP_mfxSession = &m_mfxSession;
        m_pmfxVPP = new MFXVideoVPP(*m_pVPP_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxVPP, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxVppParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    else
        m_pVPP_mfxSession = NULL;

    //if(pParams->bENCODE){
        // create encoder
        m_pmfxENCODE = new MFXVideoENCODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_MEMORY_ALLOC);
    //}

    if(pParams->bPREENC){
        // create encoder
        m_pmfxPREENC = new MFXVideoENC(m_preenc_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPREENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bENCPAK){
        // create encoder
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyENC){
        // create ENC
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyPAK){
        // create PAK
        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    m_nAsyncDepth = 1; // this number can be tuned for better performance

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close()
{
    if (m_FileWriters.first)
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_FileWriters.first->m_nProcessedFramesNum);

    MSDK_SAFE_DELETE(m_pmfxDECODE);
    MSDK_SAFE_DELETE(m_pmfxVPP);
    MSDK_SAFE_DELETE(m_pmfxENCODE);
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);
    MSDK_SAFE_DELETE(m_pmfxPREENC);

    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);

    DeleteFrames();

    m_TaskPool.Close();
    m_mfxSession.Close();
    if (m_encpakParams.bDECODE)
    {
        m_decode_mfxSession.Close();
    }
    if (m_encpakParams.bPREENC){
        m_preenc_mfxSession.Close();
    }

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_FileReader.Close();
    FreeFileWriters();

    WipeMfxBitstream(&m_mfxBS);
}

void CEncodingPipeline::FreeFileWriters()
{
    if (m_FileWriters.second == m_FileWriters.first)
    {
        m_FileWriters.second = NULL; // second do not own the writer - just forget pointer
    }

    if (m_FileWriters.first)
        m_FileWriters.first->Close();
    MSDK_SAFE_DELETE(m_FileWriters.first);

    if (m_FileWriters.second)
        m_FileWriters.second->Close();
    MSDK_SAFE_DELETE(m_FileWriters.second);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(sInputParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //use encoded order if both is used
    if(m_encpakParams.bPREENC && (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)){
        m_mfxEncParams.mfx.EncodedOrder = 1;
    }

    // Init decode
    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->Init(&m_mfxDecParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Init(&m_mfxVppParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if ((m_encpakParams.bENCODE) && (m_pmfxENCODE) )
    {
        sts = m_pmfxENCODE->Init(&m_mfxEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPREENC)
    {
        mfxVideoParam preEncParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        preEncParams.NumExtParam = 1;
        preEncParams.ExtParam    = buf;
        //preEncParams.AllocId     = PREENC_ALLOC;

        sts = m_pmfxPREENC->Init(&preEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts) {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxENC)
    {
        mfxVideoParam encParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENC;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        encParams.NumExtParam = 1;
        encParams.ExtParam    = buf;
        //encParams.AllocId     = m_pmfxPREENC ? PAK_ALLOC : PREENC_ALLOC;

        sts = m_pmfxENC->Init(&encParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPAK)
    {
        mfxVideoParam pakParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PAK;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        pakParams.NumExtParam = 1;
        pakParams.ExtParam    = buf;
        //pakParams.AllocId     = PAK_ALLOC;

        sts = m_pmfxPAK->Init(&pakParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    //sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, 1, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENCODE->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::GetFreeTask(sTask **ppTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

PairU8 CEncodingPipeline::GetFrameType(mfxU32 frameOrder)
{
    mfxU32 gopOptFlag = m_encpakParams.GopOptFlag;
    mfxU32 gopPicSize = m_encpakParams.gopSize;
    mfxU32 gopRefDist = m_encpakParams.refDist;
    mfxU32 idrPicDist = gopPicSize * (m_encpakParams.nIdrInterval + 1);

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (frameOrder % idrPicDist == 0)
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
            (frameOrder + 1) % idrPicDist == 0)
            return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    return (m_encpakParams.nNumFrames && (frameOrder + 1) >= m_encpakParams.nNumFrames) ? ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : ExtendFrameType(MFX_FRAMETYPE_B);
}

PairU8 ExtendFrameType(mfxU32 type)
{
    mfxU32 type1 = type & 0xff;
    mfxU32 type2 = type >> 8;

    if (type2 == 0)
    {
        type2 = type1 & ~MFX_FRAMETYPE_IDR; // second field can't be IDR

        if (type1 & MFX_FRAMETYPE_I)
        {
            type2 &= ~MFX_FRAMETYPE_I;
            type2 |= MFX_FRAMETYPE_P;
        }
    }

    return PairU8(type1, type2);
}

BiFrameLocation CEncodingPipeline::GetBiFrameLocation(mfxU32 frameOrder)
{

    mfxU32 gopPicSize = m_encpakParams.gopSize;
    mfxU32 gopRefDist = m_encpakParams.refDist;
    mfxU32 biPyramid  = m_encpakParams.bRefType;

    BiFrameLocation loc;

    if (gopPicSize == 0xffff) //infinite GOP
        gopPicSize = 0xffffffff;

    if (biPyramid != MFX_B_REF_OFF)
    {
        bool ref = false;
        mfxU32 orderInMiniGop = frameOrder % gopPicSize % gopRefDist - 1;

        loc.encodingOrder = GetEncodingOrder(orderInMiniGop, 0, gopRefDist - 1, 0, ref);
        loc.miniGopCount = frameOrder % gopPicSize / gopRefDist;
        loc.refFrameFlag = mfxU16(ref ? MFX_FRAMETYPE_REF : 0);
    }

    return loc;
}

mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
{
    //assert(displayOrder >= begin);
    //assert(displayOrder <  end);

    ref = (end - begin > 1);

    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return counter;
    else if (displayOrder < pivot)
        return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
    else
        return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
}

mfxStatus CEncodingPipeline::SynchronizeFirstTask()
{
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::InitInterfaces()
{
    m_tmpForMedian  = NULL;
    m_tmpForReading = NULL;
    m_tmpMBpreenc   = NULL;
    m_tmpMBenc      = NULL;

    m_ctr           = NULL;

    mfxU32 fieldId = 0;
    int numExtInParams = 0, numExtInParamsI = 0, numExtOutParams = 0, numExtOutParamsI = 0;

    if (m_encpakParams.bPREENC)
    {
        //setup control structures
        m_disableMVoutPreENC     = m_encpakParams.mvoutFile == NULL       && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);
        m_disableMBStatPreENC    = (m_encpakParams.mbstatoutFile == NULL) ||   m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC; //couple with ENC+PAK
        bool enableMVpredictor   = m_encpakParams.mvinFile != NULL;
        bool enableMBQP          = m_encpakParams.mbQpFile != NULL        && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);

        bufSet* tmpForInit;
        mfxExtFeiPreEncCtrl*         preENCCtr = NULL;
        mfxExtFeiPreEncMVPredictors* mvPreds   = NULL;
        mfxExtFeiEncQP*              qps       = NULL;
        mfxExtFeiPreEncMV*           mvs       = NULL;
        mfxExtFeiPreEncMBStat*       mbdata    = NULL;

        int num_buffers = m_maxQueueLength + (m_encpakParams.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0);

        for (int k = 0; k < num_buffers; k++)
        {
            numExtInParams   = 0;
            numExtInParamsI  = 0;
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*tmpForInit);
            tmpForInit->vacant = true;

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                if (fieldId == 0){
                    preENCCtr = new mfxExtFeiPreEncCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(preENCCtr, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(preENCCtr, m_numOfFields);
                }
                numExtInParams++;
                numExtInParamsI++;

                preENCCtr[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
                preENCCtr[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
                preENCCtr[fieldId].PictureType = (mfxU16)(m_numOfFields == 1 ? MFX_PICTYPE_FRAME :
                    fieldId == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);
                preENCCtr[fieldId].RefPictureType[0]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].RefPictureType[1]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].DisableMVOutput         = m_disableMVoutPreENC;
                preENCCtr[fieldId].DisableStatisticsOutput = m_disableMBStatPreENC;
                preENCCtr[fieldId].FTEnable                = m_encpakParams.FTEnable;
                preENCCtr[fieldId].AdaptiveSearch          = m_encpakParams.AdaptiveSearch;
                preENCCtr[fieldId].LenSP                   = m_encpakParams.LenSP;
                preENCCtr[fieldId].MBQp                    = enableMBQP;
                preENCCtr[fieldId].MVPredictor             = enableMVpredictor;
                preENCCtr[fieldId].RefHeight               = m_encpakParams.RefHeight;
                preENCCtr[fieldId].RefWidth                = m_encpakParams.RefWidth;
                preENCCtr[fieldId].SubPelMode              = m_encpakParams.SubPelMode;
                preENCCtr[fieldId].SearchWindow            = m_encpakParams.SearchWindow;
                preENCCtr[fieldId].SearchPath              = m_encpakParams.SearchPath;
                preENCCtr[fieldId].Qp                      = m_encpakParams.QP;
                preENCCtr[fieldId].IntraSAD                = m_encpakParams.IntraSAD;
                preENCCtr[fieldId].InterSAD                = m_encpakParams.InterSAD;
                preENCCtr[fieldId].SubMBPartMask           = m_encpakParams.SubMBPartMask;
                preENCCtr[fieldId].IntraPartMask           = m_encpakParams.IntraPartMask;
                preENCCtr[fieldId].Enable8x8Stat           = m_encpakParams.Enable8x8Stat;

                if (preENCCtr[fieldId].MVPredictor)
                {
                    if (fieldId == 0){
                        mvPreds = new mfxExtFeiPreEncMVPredictors[m_numOfFields];
                        MSDK_CHECK_POINTER(mvPreds, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mvPreds, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    mvPreds[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
                    mvPreds[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMVPredictors);
                    mvPreds[fieldId].NumMBAlloc = m_numMB;
                    mvPreds[fieldId].MB = new mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB[m_numMB];
                    MSDK_CHECK_POINTER(mvPreds[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvPreds[fieldId].MB, m_numMB);

                    if (pMvPred == NULL) {
                        //read mvs from file
                        printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                        MSDK_FOPEN(pMvPred, m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                            exit(-1);
                        }
                    }
                }

                if (preENCCtr[fieldId].MBQp)
                {
                    if (fieldId == 0){
                        qps = new mfxExtFeiEncQP[m_numOfFields];
                        MSDK_CHECK_POINTER(qps, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(qps, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    qps[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                    qps[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    qps[fieldId].NumQPAlloc = m_numMB;
                    qps[fieldId].QP = new mfxU8[m_numMB];
                    MSDK_CHECK_POINTER(qps[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(qps[fieldId].QP, m_numMB);

                    if (pPerMbQP == NULL){
                        //read mvs from file
                        printf("Using QP input file: %s\n", m_encpakParams.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                            exit(-1);
                        }
                    }
                }

                if (!preENCCtr[fieldId].DisableMVOutput)
                {
                    if (fieldId == 0){
                        mvs = new mfxExtFeiPreEncMV[m_numOfFields];
                        MSDK_CHECK_POINTER(mvs, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mvs, m_numOfFields);
                    }
                    numExtOutParams++;

                    mvs[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
                    mvs[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
                    mvs[fieldId].NumMBAlloc = m_numMB;
                    mvs[fieldId].MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[m_numMB];
                    MSDK_CHECK_POINTER(mvs[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvs[fieldId].MB, m_numMB);

                    if (mvout == NULL && m_encpakParams.mvoutFile != NULL &&
                        !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC))
                    {
                        printf("Using MV output file: %s\n", m_encpakParams.mvoutFile);
                        m_tmpMBpreenc = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB;
                        memset(m_tmpMBpreenc, 0x8000, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB));
                        MSDK_FOPEN(mvout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                        if (mvout == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                            exit(-1);
                        }
                    }
                }

                if (!preENCCtr[fieldId].DisableStatisticsOutput)
                {
                    if (fieldId == 0){
                        mbdata = new mfxExtFeiPreEncMBStat[m_numOfFields];
                        MSDK_CHECK_POINTER(mbdata, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mbdata, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    mbdata[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
                    mbdata[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
                    mbdata[fieldId].NumMBAlloc = m_numMB;
                    mbdata[fieldId].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[m_numMB];
                    MSDK_CHECK_POINTER(mbdata[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mbdata[fieldId].MB, m_numMB);

                    if (mbstatout == NULL){
                        printf("Using MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                            exit(-1);
                        }
                    }
                }

            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

            tmpForInit-> I_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParamsI];
            MSDK_CHECK_POINTER(tmpForInit-> I_bufs.in.ExtParam,  MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.in.ExtParam,  MFX_ERR_MEMORY_ALLOC);
            tmpForInit-> I_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParamsI];
            MSDK_CHECK_POINTER(tmpForInit-> I_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);

            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.in.ExtParam,  numExtInParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.in.ExtParam,  numExtInParams);
            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.out.ExtParam, numExtOutParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.out.ExtParam, numExtOutParams);

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(preENCCtr + fieldId);
                tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(preENCCtr + fieldId);
            }
            if (enableMVpredictor){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(mvPreds + fieldId);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(mvPreds + fieldId);
                }
            }
            if (enableMBQP){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(qps + fieldId);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(qps + fieldId);
                }
            }
            if (!m_disableMVoutPreENC){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mvs + fieldId);
                }
            }
            if (!m_disableMBStatPreENC){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mbdata + fieldId);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mbdata + fieldId);
                }
            }

            m_preencBufs.push_back(tmpForInit);
        } // for (int k = 0; k < 2 * this->m_refDist + 1 + (m_encpakParams.bDECODE ? this->m_decodePoolSize + 1 : 0); k++)
    } // if (m_encpakParams.bPREENC)

    if ((m_encpakParams.bENCODE) || (m_encpakParams.bENCPAK) ||
        (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC))
    {
        bufSet* tmpForInit;
        //FEI buffers
        mfxEncodeCtrl*            feiCtrl            = NULL;
        mfxExtFeiSPS*             m_feiSPS           = NULL;
        mfxExtFeiPPS*             m_feiPPS           = NULL;
        mfxExtFeiSliceHeader*     m_feiSliceHeader   = NULL;
        mfxExtFeiEncFrameCtrl*    feiEncCtrl         = NULL;
        mfxExtFeiEncMVPredictors* feiEncMVPredictors = NULL;
        mfxExtFeiEncMBCtrl*       feiEncMBCtrl       = NULL;
        mfxExtFeiEncMBStat*       feiEncMbStat       = NULL;
        mfxExtFeiEncMV*           feiEncMV           = NULL;
        mfxExtFeiEncQP*           feiEncMbQp         = NULL;
        mfxExtFeiPakMBCtrl*       feiEncMBCode       = NULL;

        feiCtrl = new mfxEncodeCtrl;
        MSDK_ZERO_MEMORY(*feiCtrl);
        feiCtrl->FrameType = MFX_FRAMETYPE_UNKNOWN;
        feiCtrl->QP = m_encpakParams.QP;
        //feiCtrl->SkipFrame = 0;
        //feiCtrl->NumPayload = 0;
        //feiCtrl->Payload = NULL;
        m_ctr = feiCtrl;

        bool MVPredictors = (m_encpakParams.mvinFile   != NULL) || m_encpakParams.bPREENC; //couple with PREENC
        bool MBCtrl       = m_encpakParams.mbctrinFile != NULL;
        bool MBQP         = m_encpakParams.mbQpFile    != NULL;

        bool MBStatOut    = m_encpakParams.mbstatoutFile != NULL;
        bool MVOut        = m_encpakParams.mvoutFile     != NULL;
        bool MBCodeOut    = m_encpakParams.mbcodeoutFile != NULL;

        /*SPS header */
        m_feiSPS = new mfxExtFeiSPS;
        MSDK_CHECK_POINTER(m_feiSPS, MFX_ERR_MEMORY_ALLOC);
        MSDK_ZERO_MEMORY(*m_feiSPS);
        m_feiSPS->Header.BufferId = MFX_EXTBUFF_FEI_SPS;
        m_feiSPS->Header.BufferSz = sizeof(mfxExtFeiSPS);
        m_feiSPS->Pack    = m_encpakParams.bPassHeaders ? 1 : 0; /* MSDK will use this data to do packing*/
        m_feiSPS->SPSId   = 0;
        m_feiSPS->Profile = m_encpakParams.CodecProfile;
        m_feiSPS->Level   = m_encpakParams.CodecLevel;

        m_feiSPS->NumRefFrame = m_mfxEncParams.mfx.NumRefFrame;
        m_feiSPS->WidthInMBs  = m_mfxEncParams.mfx.FrameInfo.Width  / 16;
        m_feiSPS->HeightInMBs = m_mfxEncParams.mfx.FrameInfo.Height / 16;

        m_feiSPS->ChromaFormatIdc  = m_mfxEncParams.mfx.FrameInfo.ChromaFormat;
        m_feiSPS->FrameMBsOnlyFlag = (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
        m_feiSPS->MBAdaptiveFrameFieldFlag = 0;
        m_feiSPS->Direct8x8InferenceFlag   = 1;
        m_frameNumMax =
            m_feiSPS->Log2MaxFrameNum = 4;
        m_feiSPS->PicOrderCntType = GetDefaultPicOrderCount(m_mfxEncParams);
        m_feiSPS->Log2MaxPicOrderCntLsb       = 4;
        m_feiSPS->DeltaPicOrderAlwaysZeroFlag = 1;

        int num_buffers = m_maxQueueLength + (m_encpakParams.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0);

        for (int k = 0; k < num_buffers; k++)
        {
            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*tmpForInit);
            tmpForInit->vacant = true;

            numExtInParams   = m_encpakParams.bPassHeaders ? 1 : 0; // count SPS header
            numExtInParamsI  = m_encpakParams.bPassHeaders ? 1 : 0; // count SPS header
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                if (fieldId == 0){
                    feiEncCtrl = new mfxExtFeiEncFrameCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(feiEncCtrl, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncCtrl, m_numOfFields);
                }
                numExtInParams++;
                numExtInParamsI++;

                feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
                feiEncCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

                feiEncCtrl[fieldId].SearchPath             = m_encpakParams.SearchPath;
                feiEncCtrl[fieldId].LenSP                  = m_encpakParams.LenSP;
                feiEncCtrl[fieldId].SubMBPartMask          = m_encpakParams.SubMBPartMask;
                feiEncCtrl[fieldId].MultiPredL0            = m_encpakParams.MultiPredL0;
                feiEncCtrl[fieldId].MultiPredL1            = m_encpakParams.MultiPredL1;
                feiEncCtrl[fieldId].SubPelMode             = m_encpakParams.SubPelMode;
                feiEncCtrl[fieldId].InterSAD               = m_encpakParams.InterSAD;
                feiEncCtrl[fieldId].IntraSAD               = m_encpakParams.IntraSAD;
                feiEncCtrl[fieldId].IntraPartMask          = m_encpakParams.IntraPartMask;
                feiEncCtrl[fieldId].DistortionType         = m_encpakParams.DistortionType;
                feiEncCtrl[fieldId].RepartitionCheckEnable = m_encpakParams.RepartitionCheckEnable;
                feiEncCtrl[fieldId].AdaptiveSearch         = m_encpakParams.AdaptiveSearch;
                feiEncCtrl[fieldId].MVPredictor            = MVPredictors;
                feiEncCtrl[fieldId].NumMVPredictors        = m_encpakParams.NumMVPredictors; //always 4 predictors
                feiEncCtrl[fieldId].PerMBQp                = MBQP;
                feiEncCtrl[fieldId].PerMBInput             = MBCtrl;
                feiEncCtrl[fieldId].MBSizeCtrl             = m_encpakParams.bMBSize;
                feiEncCtrl[fieldId].ColocatedMbDistortion  = m_encpakParams.ColocatedMbDistortion;
                //Note:
                //(RefHeight x RefWidth) should not exceed 2048 for P frames and 1024 for B frames
                feiEncCtrl[fieldId].RefHeight    = m_encpakParams.RefHeight;
                feiEncCtrl[fieldId].RefWidth     = m_encpakParams.RefWidth;
                feiEncCtrl[fieldId].SearchWindow = m_encpakParams.SearchWindow;

                /* PPS header */
                if (fieldId == 0){
                    m_feiPPS = new mfxExtFeiPPS[m_numOfFields];
                    MSDK_CHECK_POINTER(m_feiPPS, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(m_feiPPS, m_numOfFields);
                }
                if (m_encpakParams.bPassHeaders){
                    numExtInParams++;
                    numExtInParamsI++;
                }

                m_feiPPS[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PPS;
                m_feiPPS[fieldId].Header.BufferSz = sizeof(mfxExtFeiPPS);
                m_feiPPS[fieldId].Pack = m_encpakParams.bPassHeaders ? 1 : 0;

                m_feiPPS[fieldId].SPSId = m_feiSPS ? m_feiSPS->SPSId : 0;
                m_feiPPS[fieldId].PPSId = 0;

                m_feiPPS[fieldId].FrameNum = 0;

                m_feiPPS[fieldId].PicInitQP = (m_encpakParams.QP != 0) ? m_encpakParams.QP : 26;

                m_feiPPS[fieldId].NumRefIdxL0Active = 1;
                m_feiPPS[fieldId].NumRefIdxL1Active = 1;

                m_feiPPS[fieldId].ChromaQPIndexOffset       = 0;
                m_feiPPS[fieldId].SecondChromaQPIndexOffset = 0;

                m_feiPPS[fieldId].IDRPicFlag                = 0;
                m_feiPPS[fieldId].ReferencePicFlag          = 0;
                m_feiPPS[fieldId].EntropyCodingModeFlag     = 1;
                m_feiPPS[fieldId].ConstrainedIntraPredFlag  = 0;
                m_feiPPS[fieldId].Transform8x8ModeFlag      = 1;

                if (fieldId == 0){
                    m_feiSliceHeader = new mfxExtFeiSliceHeader[m_numOfFields];
                    MSDK_CHECK_POINTER(m_feiSliceHeader, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(m_feiSliceHeader, m_numOfFields);
                }
                if (m_encpakParams.bPassHeaders){
                    numExtInParams++;
                    numExtInParamsI++;
                }

                m_feiSliceHeader[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
                m_feiSliceHeader[fieldId].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);
                m_feiSliceHeader[fieldId].Pack = m_encpakParams.bPassHeaders ? 1 : 0;
                m_feiSliceHeader[fieldId].NumSlice =
                    m_feiSliceHeader[fieldId].NumSliceAlloc = m_encpakParams.numSlices;
                m_feiSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_encpakParams.numSlices];
                MSDK_CHECK_POINTER(m_feiSliceHeader[fieldId].Slice, MFX_ERR_MEMORY_ALLOC);
                MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice, m_encpakParams.numSlices);
                // TODO: Implement real slice divider
                // For now only one slice is supported
                for (int numSlice = 0; numSlice < m_feiSliceHeader[fieldId].NumSlice; numSlice++)
                {
                    m_feiSliceHeader[fieldId].Slice->MBAaddress = 0;
                    m_feiSliceHeader[fieldId].Slice->NumMBs     = m_feiSPS->WidthInMBs * m_feiSPS->HeightInMBs;
                    m_feiSliceHeader[fieldId].Slice->SliceType  = 0;
                    m_feiSliceHeader[fieldId].Slice->PPSId      = m_feiPPS[fieldId].PPSId;
                    m_feiSliceHeader[fieldId].Slice->IdrPicId   = 0;

                    m_feiSliceHeader[fieldId].Slice->CabacInitIdc = 0;

                    m_feiSliceHeader[fieldId].Slice->SliceQPDelta = 26 - m_feiPPS[fieldId].PicInitQP;
                    m_feiSliceHeader[fieldId].Slice->DisableDeblockingFilterIdc = 0;
                    m_feiSliceHeader[fieldId].Slice->SliceAlphaC0OffsetDiv2     = 0;
                    m_feiSliceHeader[fieldId].Slice->SliceBetaOffsetDiv2        = 0;
                }

                if (MVPredictors)
                {
                    if (fieldId == 0){
                        feiEncMVPredictors = new mfxExtFeiEncMVPredictors[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMVPredictors, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMVPredictors, m_numOfFields);
                    }
                    numExtInParams++;

                    feiEncMVPredictors[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
                    feiEncMVPredictors[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);
                    feiEncMVPredictors[fieldId].NumMBAlloc = m_numMB;
                    feiEncMVPredictors[fieldId].MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMVPredictors[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMVPredictors[fieldId].MB, m_numMB);

                    if (!m_encpakParams.bPREENC && pMvPred == NULL) { //not load if we couple with PREENC
                        printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                        MSDK_FOPEN(pMvPred, m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                            exit(-1);
                        }
                        if (m_encpakParams.bRepackPreencMV){
                            m_tmpForMedian  = new mfxI16[16];
                            MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                            m_tmpForReading = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[m_numMB];
                            MSDK_CHECK_POINTER(m_tmpForReading, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForReading, m_numMB);
                        }
                    }
                    else {
                        m_tmpForMedian = new mfxI16[16];
                        MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                    }
                }

                if (MBCtrl)
                {
                    if (fieldId == 0){
                        feiEncMBCtrl = new mfxExtFeiEncMBCtrl[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMBCtrl, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMBCtrl, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiEncMBCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
                    feiEncMBCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
                    feiEncMBCtrl[fieldId].NumMBAlloc = m_numMB;
                    feiEncMBCtrl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMBCtrl[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCtrl[fieldId].MB, m_numMB);

                    if (pEncMBs == NULL){
                        printf("Using MB control input file: %s\n", m_encpakParams.mbctrinFile);
                        MSDK_FOPEN(pEncMBs, m_encpakParams.mbctrinFile, MSDK_CHAR("rb"));
                        if (pEncMBs == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbctrinFile);
                            exit(-1);
                        }
                    }
                }

                if (MBQP)
                {
                    if (fieldId == 0){
                        feiEncMbQp = new mfxExtFeiEncQP[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMbQp, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMbQp, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                    feiEncMbQp[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    feiEncMbQp[fieldId].NumQPAlloc = m_numMB;
                    feiEncMbQp[fieldId].QP = new mfxU8[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMbQp[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbQp[fieldId].QP, m_numMB);

                    if (pPerMbQP == NULL) {
                        printf("Use MB QP input file: %s\n", m_encpakParams.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                            exit(-1);
                        }
                    }
                }

                //Open output files if any
                //distortion buffer have to be always provided - current limitation
                if (MBStatOut)
                {
                    if (fieldId == 0){
                        feiEncMbStat = new mfxExtFeiEncMBStat[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMbStat, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMbStat, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMbStat[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
                    feiEncMbStat[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
                    feiEncMbStat[fieldId].NumMBAlloc = m_numMB;
                    feiEncMbStat[fieldId].MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMbStat[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbStat[fieldId].MB, m_numMB);

                    if (mbstatout == NULL) {
                        printf("Use MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                            exit(-1);
                        }
                    }
                }

                if ((MVOut) || (m_encpakParams.bENCPAK))
                {
                    MVOut = true; /* ENC_PAK need MVOut and MBCodeOut buffer */
                    if (fieldId == 0){
                        feiEncMV = new mfxExtFeiEncMV[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMV, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMV, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMV[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
                    feiEncMV[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMV);
                    feiEncMV[fieldId].NumMBAlloc = m_numMB;
                    feiEncMV[fieldId].MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMV[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMV[fieldId].MB, m_numMB);

                    if ((mvENCPAKout == NULL) && (NULL != m_encpakParams.mvoutFile)){
                        printf("Use MV output file: %s\n", m_encpakParams.mvoutFile);
                        if (m_encpakParams.bOnlyPAK)
                            MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("rb"));
                        else { /*for all other cases need to write into this file*/
                            MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                            m_tmpMBenc = new mfxExtFeiEncMV::mfxExtFeiEncMVMB;
                            memset(m_tmpMBenc, 0x8000, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
                        }
                        if (mvENCPAKout == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                            exit(-1);
                        }
                    }
                }

                //distortion buffer have to be always provided - current limitation
                if ((MBCodeOut) || (m_encpakParams.bENCPAK))
                {
                    MBCodeOut = true; /* ENC_PAK need MVOut and MBCodeOut buffer */
                    if (fieldId == 0){
                        feiEncMBCode = new mfxExtFeiPakMBCtrl[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMBCode, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMBCode, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMBCode[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
                    feiEncMBCode[fieldId].Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
                    feiEncMBCode[fieldId].NumMBAlloc = m_numMB;
                    feiEncMBCode[fieldId].MB = new mfxFeiPakMBCtrl[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMBCode[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCode[fieldId].MB, m_numMB);

                    if ((mbcodeout == NULL) && (NULL != m_encpakParams.mbcodeoutFile)){
                        printf("Use MB code output file: %s\n", m_encpakParams.mbcodeoutFile);
                        if (m_encpakParams.bOnlyPAK)
                            MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("rb"));
                        else
                            MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("wb"));

                        if (mbcodeout == NULL) {
                            fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbcodeoutFile);
                            exit(-1);
                        }
                    }
                }

            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

            tmpForInit->I_bufs.in.ExtParam = new mfxExtBuffer*[numExtInParamsI];
            MSDK_CHECK_POINTER(tmpForInit->I_bufs.in.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.in.ExtParam = new mfxExtBuffer*[numExtInParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.in.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->I_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParamsI];
            MSDK_CHECK_POINTER(tmpForInit->I_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);

            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.in.ExtParam, numExtInParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.in.ExtParam, numExtInParams);
            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.out.ExtParam, numExtOutParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.out.ExtParam, numExtOutParams);

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncCtrl[fieldId]);
                tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncCtrl[fieldId]);
            }

            if (m_feiSPS->Pack){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)m_feiSPS;
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)m_feiSPS;
                }
            }
            if (m_feiPPS[0].Pack){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&m_feiPPS[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&m_feiPPS[fieldId]);
                }
            }
            if (m_feiSliceHeader[0].Pack){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&m_feiSliceHeader[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&m_feiSliceHeader[fieldId]);
                }
            }
            if (MVPredictors){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    //tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)feiEncMVPredictors;
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMVPredictors[fieldId]);
                }
            }
            if (MBCtrl){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCtrl[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCtrl[fieldId]);
                }
            }
            if (MBQP){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbQp[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbQp[fieldId]);
                }
            }
            if (MBStatOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbStat[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbStat[fieldId]);
                }
            }
            if (MVOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMV[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMV[fieldId]);
                }
            }
            if (MBCodeOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCode[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCode[fieldId]);
                }
            }

            m_encodeBufs.push_back(tmpForInit);

        } // for (int k = 0; k < 2 * this->m_refDist + 1 + (m_encpakParams.bDECODE ? this->m_decodePoolSize + 1 : 0); k++)
    } // if (m_encpakParams.bENCODE)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::Run()
{
    //MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf      = NULL; // dispatching pointer
    mfxFrameSurface1* pReconSurf = NULL;

    bool bEndOfFile = false;
    ExtendedSurface DecExtSurface = { 0 };

    sTask *pCurrentTask  = NULL;  // a pointer to the current task
    mfxU16 nEncSurfIdx   = 0;     // index of free surface for encoder input (vpp output)
    mfxU16 nReconSurfIdx = 0;     // index of free surface for reconstruct pool
    m_frameNumMax = 4;

    m_numOfFields = 1; // default is progressive case
    mfxI32 heightMB = ((m_encpakParams.nHeight + 15) & ~15);
    mfxI32 widthMB  = ((m_encpakParams.nWidth  + 15) & ~15);
    m_numMB = heightMB * widthMB / 256;

    /* if TFF or BFF*/
    if ((m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
        (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF))
    {
        m_numOfFields = 2;
        // For interlaced mode, may need an extra MB vertically
        // For example if the progressive mode has 45 MB vertically
        // The interlace should have 23 MB for each field
        m_numMB = widthMB / 16 * (((heightMB / 16) + 1) / 2);
    }

    sts = InitInterfaces();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // main loop, preprocessing and encoding
    sts = MFX_ERR_NONE;
    bool twoEncoders = m_encpakParams.bPREENC && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bENCODE);
    bool create_task = (m_encpakParams.bPREENC)  || (m_encpakParams.bENCPAK)  ||
                       (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) ||
                       (m_encpakParams.bENCODE && m_encpakParams.EncodedOrder);

    m_frameCount = 0;
    m_frameOrderIdrInDisplayOrder = 0;
    m_frameType = PairU8((mfxU8)MFX_FRAMETYPE_UNKNOWN, (mfxU8)MFX_FRAMETYPE_UNKNOWN);

    m_isField = (mfxU8)(m_numOfFields == 2);
    m_ffid = m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF;
    m_sfid = m_isField - m_ffid;

    iTask* eTask = NULL;

    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if (m_encpakParams.nNumFrames && m_frameCount >= m_encpakParams.nNumFrames)
        {
            break;
        }

        m_frameType = GetFrameType(m_frameCount);

        if (m_frameType[m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF] & MFX_FRAMETYPE_IDR)
            m_frameOrderIdrInDisplayOrder = m_frameCount;

        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);
       //pCurrentTask->mfxBS.FrameType = frameType;

        eTask = NULL;
        if (create_task)
        {
            eTask = CreateAndInitTask();
        }

        if (!m_pmfxDECODE) // load frame from YUV file
        {
            // find free surface for encoder input
            mfxFrameSurface1 *pSurfPool = m_pEncSurfaces;
            mfxU16 poolSize = m_EncResponse.NumFrameActual;
            if (m_pmfxVPP)
            {
                pSurfPool = m_pVppSurfaces;
                poolSize  = m_VppResponse.NumFrameActual;
            }
            nEncSurfIdx = GetFreeSurface(pSurfPool, poolSize);
            MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
            // point pSurf to encoder surface
            pSurf = &pSurfPool[nEncSurfIdx];

            // load frame from file to surface data
            // if we share allocator with Media SDK we need to call Lock to access surface data and...
            if (m_bExternalAlloc)
            {
                // get YUV pointers
                sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
                MSDK_BREAK_ON_ERROR(sts);
            }

            sts = m_FileReader.LoadNextFrame(pSurf);
            MSDK_BREAK_ON_ERROR(sts);

            // ... after we're done call Unlock
            if (m_bExternalAlloc)
            {
                sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
                MSDK_BREAK_ON_ERROR(sts);
            }
        }
        else // decode frame
        {
            if (!bEndOfFile)
            {
                sts = DecodeOneFrame(&DecExtSurface);
                if (MFX_ERR_MORE_DATA == sts)
                {
                    sts = DecodeLastFrame(&DecExtSurface);
                    bEndOfFile = true;
                }
            }
            else
            {
                sts = DecodeLastFrame(&DecExtSurface);
            }

            if (sts == MFX_ERR_MORE_DATA)
            {
                DecExtSurface.pSurface = NULL;
                sts = MFX_ERR_NONE;
                break;
            }

            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            while (true)
            {
                sts = m_decode_mfxSession.SyncOperation(DecExtSurface.Syncp, MSDK_WAIT_INTERVAL);

                if (MFX_ERR_NONE < sts && !DecExtSurface.Syncp) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE <= sts && DecExtSurface.Syncp) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                }
            }

            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            pSurf = DecExtSurface.pSurface;
        }

        pSurf->Data.FrameOrder = m_frameCount; // in display order

        m_frameCount++;

        if (m_pmfxVPP)
        {
            // pre-process a frame
            ExtendedSurface VppExtSurface = { 0 };

            sts = VPPOneFrame(pSurf, &VppExtSurface);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            pSurf = VppExtSurface.pSurface;
        }

        if (m_encpakParams.bPREENC)
        {
            pSurf->Data.Locked++;
            eTask->in.InSurface = pSurf;

            eTask = ConfigureTask(eTask, false);
            if (eTask == NULL) continue; //not found frame to encode

            sts = InitPreEncFrameParams(eTask);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            for (;;)
            {
                fprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);

                sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                sts = MFX_ERR_NONE;
                /*PRE-ENC is running in separate session */
                sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                fprintf(stderr, "synced : %d\n", sts);


                if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    break;
                }
            }

            //drop output data to output file
            DropPREENCoutput(eTask);

            if(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC) {
                m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);

                if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
                    //repack MV predictors
                    sts = PassPreEncMVPred2Enc(eTask);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
            }

            if (!twoEncoders)
            {
                UpdateTaskQueue(eTask);
            }

        } // if (m_encpakParams.bPREENC)

        /* ENC_and_PAK or ENC only or PAK only call */
        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC) )
        {
            pSurf->Data.Locked++;
            eTask->in.InSurface    = pSurf;
            eTask->inPAK.InSurface = pSurf;
            eTask->encoded         = 0;
            /* this is Recon surface which will be generated by FEI PAK*/
            mfxFrameSurface1 *pSurfPool = m_pReconSurfaces;
            mfxU16 poolSize = m_ReconResponse.NumFrameActual;
            if (m_pmfxDECODE && !m_pmfxVPP)
            {
                pSurfPool = m_pDecSurfaces;
                poolSize  = m_DecResponse.NumFrameActual;
            }
            nReconSurfIdx = GetFreeSurface(pSurfPool, poolSize);
            MSDK_CHECK_ERROR(nReconSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
            pReconSurf = &pSurfPool[nReconSurfIdx];
            pReconSurf->Data.Locked++;
            eTask->out.OutSurface = eTask->outPAK.OutSurface = pReconSurf;
            eTask->outPAK.Bs = &pCurrentTask->mfxBS;

            if (!m_encpakParams.bPREENC){ // in case of preenc + enc (+ pak) pipeline eTask already cofigurated by preenc
                eTask = ConfigureTask(eTask, false);
                if (eTask == NULL) continue; //not found frame to encode
            }

            sts = InitEncFrameParams(eTask);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            for (;;) {
                fprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                {
                    sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    sts = MFX_ERR_NONE;
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                }

                /* In case of PAK only we need to read data from file */
                if (m_encpakParams.bOnlyPAK)
                {
                    ReadPAKdata(eTask);
                }

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                {
                    sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                    sts = MFX_ERR_NONE;
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                }

                if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    break;
                }
            }

            //drop output data to output file
            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                pCurrentTask->WriteBitstream();

            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
            {
                DropENCPAKoutput(eTask);
            }

            UpdateTaskQueue(eTask);

        } // if (m_encpakParams.bENCPAK)

        if (m_encpakParams.bENCODE)
        {
            if (create_task && !m_encpakParams.bPREENC)
            {
                eTask->in.InSurface = pSurf;
                eTask->in.InSurface->Data.Locked++;
                eTask = ConfigureTask(eTask, false); // reorder frames in case of encoded frame order
                if (eTask == NULL) continue; //not found frame to encode

                m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);
            }
            mfxFrameSurface1* encodeSurface = create_task ? eTask->in.InSurface : pSurf;
            sts = InitEncodeFrameParams(encodeSurface, pCurrentTask, m_frameType[m_ffid]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            for (;;) {
                // at this point surface for encoder contains either a frame from file or a frame processed by vpp

                sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, encodeSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    MSDK_BREAK_ON_ERROR(sts);

                    if (create_task)
                    {
                        UpdateTaskQueue(eTask);
                    }

                    sts = SynchronizeFirstTask();
                    MSDK_BREAK_ON_ERROR(sts);
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    if (pCurrentTask->EncSyncP)
                    {
                        sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        MSDK_BREAK_ON_ERROR(sts);

                        if (create_task)
                        {
                            UpdateTaskQueue(eTask);
                        }

                        sts = SynchronizeFirstTask();
                        MSDK_BREAK_ON_ERROR(sts);
                    }
                    break;
                }
            }
        } // if (m_encpakParams.bENCODE)

    } // while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // loop to get buffered frames from encoder
    if (m_encpakParams.bPREENC)
    {
        mdprintf(stderr,"input_tasks_size=%u\n", (mfxU32)m_inputTasks.size());
        //encode last frames
        mfxU32 numUnencoded = CountUnencodedFrames();

        //run processing on last frames
        if (numUnencoded > 0) {
            if (m_inputTasks.back()->m_type[m_ffid] & MFX_FRAMETYPE_B) {
                m_inputTasks.back()->m_type[m_ffid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            if (m_inputTasks.back()->m_type[m_sfid] & MFX_FRAMETYPE_B) {
                m_inputTasks.back()->m_type[m_sfid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            mdprintf(stderr, "run processing on last frames: %d\n", numUnencoded);
            while (numUnencoded != 0) {
                // get a pointer to a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                eTask = ConfigureTask(eTask, true);
                if (eTask==NULL) continue; //not found frame to encode

                sts = InitPreEncFrameParams(eTask);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                for (;;) {
                    //Only synced operation supported for now
                    fprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);
                    sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    /*PRE-ENC is running in separate session */
                    sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);

                    sts = MFX_ERR_NONE;

                    if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                    {
                        if (MFX_WRN_DEVICE_BUSY == sts)
                            MSDK_SLEEP(1); // wait if device is busy
                    } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                        sts = MFX_ERR_NONE; // ignore warnings if output is available
                        break;
                    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                        //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                        //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    } else {
                        // get next surface and new task for 2nd bitstream in ViewOutput mode
                        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                        break;
                    }
                }

                eTask->encoded = 1;
                CopyState(eTask);

                DropPREENCoutput(eTask);

                if (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)
                {
                    m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);

                    if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
                        //repack MV predictors
                        sts = PassPreEncMVPred2Enc(eTask);
                        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    }
                }

                if (m_encpakParams.bENCODE) //if we have both
                {
                    sts = InitEncodeFrameParams(eTask->in.InSurface, pCurrentTask, ExtractFrameType(*eTask));
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                    for (;;) {
                        //no ctrl for buffered frames
                        //sts = m_pmfxENCODE->EncodeFrameAsync(ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);
                        sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, eTask->in.InSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                        if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                        {
                            if (MFX_WRN_DEVICE_BUSY == sts)
                                MSDK_SLEEP(1); // wait if device is busy
                        } else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                            sts = MFX_ERR_NONE; // ignore warnings if output is available
                            sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            MSDK_BREAK_ON_ERROR(sts);
                            //sts = SynchronizeFirstTask();
                            //MSDK_BREAK_ON_ERROR(sts);
                                while (pCurrentTask->bufs.size() != 0){
                                    pCurrentTask->bufs.front().first->vacant = true; // false
                                    pCurrentTask->bufs.pop_front();
                                }
                            break;
                        } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                            sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                        } else {
                            // get new task for 2nd bitstream in ViewOutput mode
                            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                            if (pCurrentTask->EncSyncP)
                            {
                                sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                                MSDK_BREAK_ON_ERROR(sts);
                                //sts = SynchronizeFirstTask();
                                //MSDK_BREAK_ON_ERROR(sts);
                                while (pCurrentTask->bufs.size() != 0){
                                    pCurrentTask->bufs.front().first->vacant = true;
                                    pCurrentTask->bufs.pop_front();
                                }
                            }
                            break;
                        }
                    }
                    MSDK_BREAK_ON_ERROR(sts);

                    numUnencoded--;
                } // if (m_encpakParams.bENCODE)

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))
                {
                    // get a pointer to a free task (bit stream and sync point for encoder)
                    sts = GetFreeTask(&pCurrentTask);
                    MSDK_BREAK_ON_ERROR(sts);

                    eTask->encoded = 0; //because we not finished with it

                    sts = InitEncFrameParams(eTask);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                    for (;;) {
                        //Only synced operation supported for now
                        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                        {
                            fprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);
                            sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            fprintf(stderr, "synced : %d\n", sts);
                            MSDK_BREAK_ON_ERROR(sts);
                        }

                        /* In case of PAK only we need to read data from file */
                        if (m_encpakParams.bOnlyPAK)
                        {
                            ReadPAKdata(eTask);
                        }

                        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                        {
                            sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            fprintf(stderr, "synced : %d\n", sts);
                            MSDK_BREAK_ON_ERROR(sts);
                        }

                        if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                        {
                            if (MFX_WRN_DEVICE_BUSY == sts)
                                MSDK_SLEEP(1); // wait if device is busy
                        } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                            sts = MFX_ERR_NONE; // ignore warnings if output is available
                           break;
                        } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                           //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                           //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                        } else {
                           // get next surface and new task for 2nd bitstream in ViewOutput mode
                           MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                           break;
                        }
                    }

                    eTask->encoded = 1;

                    //drop output data to output file
                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                            pCurrentTask->WriteBitstream();

                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                    {
                        DropENCPAKoutput(eTask);
                    }

                    //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    numUnencoded--;
                } // if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))

            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

        if (m_encpakParams.bENCODE) { //if we have both
            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
        }

    } // if (m_encpakParams.bPREENC)

    // loop to get buffered frames from encoder
    if (!m_encpakParams.bPREENC && ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK)))
    {
        mdprintf(stderr, "input_tasks_size=%u\n", (mfxU32)m_inputTasks.size());
        //encode last frames
        mfxU32 numUnencoded = CountUnencodedFrames();

        //run processing on last frames
        if (numUnencoded > 0) {
            if (m_inputTasks.back()->m_type[m_ffid] & MFX_FRAMETYPE_B) {
                m_inputTasks.back()->m_type[m_ffid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            if (m_inputTasks.back()->m_type[m_sfid] & MFX_FRAMETYPE_B) {
                m_inputTasks.back()->m_type[m_sfid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            mdprintf(stderr, "run processing on last frames: %d\n", numUnencoded);
            while (numUnencoded != 0) {
                // get a pointer to a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                if (!m_encpakParams.bPREENC){ // in case of preenc + enc (+ pak) pipeline eTask already cofigurated by preenc
                    eTask = ConfigureTask(eTask, true);
                    if (eTask == NULL) continue; //not found frame to encode
                }

                sts = InitEncFrameParams(eTask);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                for (;;) {
                    //Only synced operation supported for now
                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                    {
                        fprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);
                        sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                        sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        fprintf(stderr, "synced : %d\n", sts);
                        MSDK_BREAK_ON_ERROR(sts);
                    }

                    /* In case of PAK only we need to read data from file */
                    if (m_encpakParams.bOnlyPAK)
                    {
                        ReadPAKdata(eTask);
                    }

                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                    {
                        sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                        sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        fprintf(stderr, "synced : %d\n", sts);
                        MSDK_BREAK_ON_ERROR(sts);
                    }

                    if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                    {
                        if (MFX_WRN_DEVICE_BUSY == sts)
                            MSDK_SLEEP(1); // wait if device is busy
                    } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                        sts = MFX_ERR_NONE; // ignore warnings if output is available
                        break;
                    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                        //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                        //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    } else {
                        // get next surface and new task for 2nd bitstream in ViewOutput mode
                        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                        break;
                    }
                }

                eTask->encoded = 1;

                //drop output data to output file
                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                    pCurrentTask->WriteBitstream();

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC))
                {
                    DropENCPAKoutput(eTask);
                }

                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                numUnencoded--;
            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

    } // if (m_encpakParams.bENCPAK)

    if (m_encpakParams.bENCODE && !m_encpakParams.bPREENC)
    {
        if (!create_task)
        {
            while (MFX_ERR_NONE <= sts) {
                // get a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                //Add output buffers
                bufSet* encode_IOBufs = getFreeBufSet(m_encodeBufs);
                MSDK_CHECK_POINTER(encode_IOBufs, MFX_ERR_NULL_PTR);
                if (encode_IOBufs->PB_bufs.out.NumExtParam > 0){
                    pCurrentTask->mfxBS.NumExtParam = encode_IOBufs->PB_bufs.out.NumExtParam;
                    pCurrentTask->mfxBS.ExtParam    = encode_IOBufs->PB_bufs.out.ExtParam;
                }

                for (;;) {

                    //no ctrl for buffered frames
                    sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                    if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                    {
                        if (MFX_WRN_DEVICE_BUSY == sts)
                            MSDK_SLEEP(1); // wait if device is busy
                    }
                    else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                        sts = MFX_ERR_NONE; // ignore warnings if output is available
                        sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        MSDK_BREAK_ON_ERROR(sts);
                        encode_IOBufs->vacant = true;
                        break;
                    }
                    else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                        sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    }
                    else {
                        // get new task for 2nd bitstream in ViewOutput mode
                        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                        if (pCurrentTask->EncSyncP)
                        {
                            sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            MSDK_BREAK_ON_ERROR(sts);
                        }
                        encode_IOBufs->vacant = true;
                        break;
                    }
                }
                MSDK_BREAK_ON_ERROR(sts);
            }

            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
        } // if (!create_task)
        else
        {
            mfxU32 numUnencoded = CountUnencodedFrames();

            if (numUnencoded > 0)
            {
                if (m_inputTasks.back()->m_type[m_ffid] & MFX_FRAMETYPE_B) {
                    m_inputTasks.back()->m_type[m_ffid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                }
                if (m_inputTasks.back()->m_type[m_sfid] & MFX_FRAMETYPE_B) {
                    m_inputTasks.back()->m_type[m_sfid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                }

                while (numUnencoded != 0)
                {
                    eTask = ConfigureTask(eTask, true);
                    if (eTask == NULL) continue; //not found frame to encode
                    m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);

                    sts = InitEncodeFrameParams(eTask->in.InSurface, pCurrentTask, ExtractFrameType(*eTask));
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                    for (;;) {
                        //no ctrl for buffered frames
                        sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, eTask->in.InSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                        eTask->encoded = 1;
                        CopyState(eTask);

                        if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                        {
                            if (MFX_WRN_DEVICE_BUSY == sts)
                                MSDK_SLEEP(1); // wait if device is busy
                        }
                        else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                            sts = MFX_ERR_NONE; // ignore warnings if output is available
                            sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            MSDK_BREAK_ON_ERROR(sts);
                            //sts = SynchronizeFirstTask();
                            //MSDK_BREAK_ON_ERROR(sts);
                            while (pCurrentTask->bufs.size() != 0){
                                pCurrentTask->bufs.front().first->vacant = true;
                                pCurrentTask->bufs.pop_front();
                            }
                            break;
                        }
                        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                            sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                        }
                        else {
                            // get new task for 2nd bitstream in ViewOutput mode
                            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                            if (pCurrentTask->EncSyncP)
                            {
                                sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                                MSDK_BREAK_ON_ERROR(sts);
                                //sts = SynchronizeFirstTask();
                                //MSDK_BREAK_ON_ERROR(sts);
                                while (pCurrentTask->bufs.size() != 0){
                                    pCurrentTask->bufs.front().first->vacant = true;
                                    pCurrentTask->bufs.pop_front();
                                }
                            }
                            break;
                        }
                    }

                    numUnencoded--;
                    MSDK_BREAK_ON_ERROR(sts);
                    /* FIXME ???*/
                    //drop output data to output file
                    //pCurrentTask->WriteBitstream();
                }
            }

            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }

        } // else
    } // if (m_encpakParams.bENCODE && !m_encpakParams.bPREENC)

    // deallocation
    {
        if (mvout != NULL)
            fclose(mvout);
        if (mbstatout != NULL)
            fclose(mbstatout);
        if (mbcodeout != NULL)
            fclose(mbcodeout);
        if (mvENCPAKout != NULL)
            fclose(mvENCPAKout);
        if (pMvPred != NULL)
            fclose(pMvPred);
        if (pEncMBs != NULL)
            fclose(pEncMBs);
        if (pPerMbQP != NULL)
            fclose(pPerMbQP);

        //unlock last frames
        ClearTasks();

        FreeBuffers(m_preencBufs);
        FreeBuffers(m_encodeBufs);

        MSDK_SAFE_DELETE(m_ctr);
        MSDK_SAFE_DELETE(m_tmpMBpreenc);
        MSDK_SAFE_DELETE(m_tmpForMedian);
        MSDK_SAFE_DELETE(m_tmpForReading);
        MSDK_SAFE_DELETE(m_tmpMBenc);
        MSDK_SAFE_DELETE(m_last_task);
    }
    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::PassPreEncMVPred2Enc(iTask* eTask){
    mfxExtFeiPreEncMV* mvs        = NULL;
    mfxExtFeiEncMVPredictors* mvp = NULL;
    bufSet* set                   = NULL;

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        for (int i = 0; i < eTask->out.NumExtParam; i++)
        {
            if (eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV){
                mvs = (mfxExtFeiPreEncMV*)(eTask->out.ExtParam[i + fieldId]);
                if (set == NULL){
                    set = getFreeBufSet(m_encodeBufs);
                    MSDK_CHECK_POINTER(set, MFX_ERR_NULL_PTR);
                }
                for (int j = 0; j < set->PB_bufs.in.NumExtParam; j++){
                    if (set->PB_bufs.in.ExtParam[j]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED){
                        mvp = (mfxExtFeiEncMVPredictors*)(set->PB_bufs.in.ExtParam[j + fieldId]);
                        repackPreenc2Enc(mvs->MB, mvp->MB, mvs->NumMBAlloc, m_tmpForMedian);
                        break;
                    }
                } //for (int j = 0; j < set->PB_bufs.in.NumExtParam; j++)
                break;
            }
        } // for (int i = 0; i < eTask->out.NumExtParam; i++)
    } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
    if (set){
        set->vacant = true;
    }

    return MFX_ERR_NONE;
}

mfxStatus repackPreenc2Enc(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf){

    MSDK_ZERO_ARRAY(EncMVPredMB, NumMB);
    for (mfxU32 i = 0; i<NumMB; i++)
    {
        //only one ref is used for now
        for (int j = 0; j < 4; j++){
            EncMVPredMB[i].RefIdx[j].RefL0 = 0;
            EncMVPredMB[i].RefIdx[j].RefL1 = 0;
        }

        //use only first subblock component of MV
        for (int j = 0; j<2; j++){
            EncMVPredMB[i].MV[0][j].x = get16Median(preencMVoutMB+i, tmpBuf, 0, j);
            EncMVPredMB[i].MV[0][j].y = get16Median(preencMVoutMB+i, tmpBuf, 1, j);
        }
    } // for(mfxU32 i=0; i<NumMBAlloc; i++)

    return MFX_ERR_NONE;
}

mfxI16 get16Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1){

    switch (xy){
    case 0:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].x;
        }
        break;
    case 1:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].y;
        }
        break;
    default:
        return 0;
    }

    std::sort(tmpBuf, tmpBuf + 16);
    return (tmpBuf[7] + tmpBuf[8]) / 2;
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    if (!m_pmfxDECODE)
        msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    else
        msdk_printf(MSDK_STRING("\nInput  video: %s\n"), CodecIdToStr(m_mfxDecParams.mfx.CodecId).c_str());
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = m_mfxEncParams.vpp.In;
    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Source picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    msdk_printf(MSDK_STRING("Destination picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(KBps)\t%d\n"), m_mfxEncParams.mfx.TargetKbps);
    msdk_printf(MSDK_STRING("Target usage\t%s\n"), TargetUsageToStr(m_mfxEncParams.mfx.TargetUsage));

    const msdk_char* sMemType = m_memType == D3D9_MEMORY  ? MSDK_STRING("d3d")
                       : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
                                                    : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) ? MSDK_STRING("hw_d3d11")
                     : (MFX_IMPL_HARDWARE & impl)  ? MSDK_STRING("hw")
                                                   : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}

/* reordering functions */

iTask* CEncodingPipeline::FindFrameToEncode(bool buffered_frames)
{
    std::list<iTask*> unencoded_queue;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it){
        if ((*it)->encoded == 0){
            unencoded_queue.push_back(*it);
        }
    }

    std::list<iTask*>::iterator top = ReorderFrame(unencoded_queue), begin = unencoded_queue.begin(), end = unencoded_queue.end();

    bool flush = unencoded_queue.size() < m_encpakParams.refDist && buffered_frames;

    if (flush && top == end && begin != end)
    {
        if (!!(m_encpakParams.GopOptFlag & MFX_GOP_STRICT))
        {
            top = begin; // TODO: reorder remaining B frames for B-pyramid when enabled
        }
        else
        {
            top = end;
            --top;
            //assert((*top)->frameType & MFX_FRAMETYPE_B);
            (*top)->m_type[0] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            (*top)->m_type[1] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            top = ReorderFrame(unencoded_queue);
            //assert(top != end || begin == end);
        }
    }

    if (top == end)
        return NULL; //skip B without refs

    return *top;
}

std::list<iTask*>::iterator CEncodingPipeline::ReorderFrame(std::list<iTask*>& unencoded_queue){
    std::list<iTask*>::iterator top = unencoded_queue.begin(), end = unencoded_queue.end();
    while (top != end &&                                     // go through buffered frames in display order and
        (((ExtractFrameType(**top) & MFX_FRAMETYPE_B) &&     // get earliest non-B frame
        CountFutureRefs((*top)->m_frameOrder) == 0) ||  // or B frame with L1 reference
        (*top)->encoded == 1))                               // which is not encoded yet
    {
        ++top;
    }

    if (top != end && (ExtractFrameType(**top) & MFX_FRAMETYPE_B))
    {
        // special case for B frames (when B pyramid is enabled)
        std::list<iTask*>::iterator i = top;
        while (++i != end &&                                          // check remaining
            (ExtractFrameType(**i) & MFX_FRAMETYPE_B) &&              // B frames
            ((*i)->m_loc.miniGopCount == (*top)->m_loc.miniGopCount)) // from the same mini-gop
        {
            if ((*i)->encoded == 0 && (*top)->m_loc.encodingOrder > (*i)->m_loc.encodingOrder)
                top = i;
        }
    }

    return top;
}

mfxU32 CEncodingPipeline::CountFutureRefs(mfxU32 frameOrder){
    mfxU32 count = 0;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
        if (frameOrder < (*it)->m_frameOrder && (*it)->encoded == 1)
            count++;
    return count;
}

iTask* CEncodingPipeline::GetTaskByFrameOrder(mfxU32 frame_order)
{
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        if ((*it)->m_frameOrder == frame_order)
            return *it;
    }

    return NULL;
}

iTask* CEncodingPipeline::CreateAndInitTask()
{
    iTask* eTask = new iTask;
    eTask->m_fieldPicFlag = m_isField;
    eTask->PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct;
    eTask->BRefType  = m_encpakParams.bRefType;
    eTask->m_fid[0]  = m_ffid;
    eTask->m_fid[1]  = m_sfid;

    if (eTask->PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        std::swap(m_frameType[0], m_frameType[1]);

    eTask->m_type       = m_frameType;
    eTask->m_frameOrder = m_frameCount;
    eTask->encoded      = 0;
    eTask->bufs         = NULL;
    eTask->preenc_bufs  = NULL;

    if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_B)
    {
        eTask->m_loc = GetBiFrameLocation(m_frameCount - m_frameOrderIdrInDisplayOrder);
        eTask->m_type[0] |= eTask->m_loc.refFrameFlag;
        eTask->m_type[1] |= eTask->m_loc.refFrameFlag;
    }

    eTask->m_reference[m_ffid] = !!(eTask->m_type[m_ffid] & MFX_FRAMETYPE_REF);
    eTask->m_reference[m_sfid] = !!(eTask->m_type[m_sfid] & MFX_FRAMETYPE_REF);

    eTask->m_poc[0] = GetPoc(*eTask, TFIELD);
    eTask->m_poc[1] = GetPoc(*eTask, BFIELD);

    eTask->m_nalRefIdc[m_ffid] = eTask->m_reference[m_ffid];
    eTask->m_nalRefIdc[m_sfid] = eTask->m_reference[m_sfid];

    return eTask;
}

iTask* CEncodingPipeline::ConfigureTask(iTask* task, bool is_buffered)
{
    if (!is_buffered)
        m_inputTasks.push_back(task); //inputTasks in display order

    iTask* eTask = FindFrameToEncode(is_buffered);
    if (eTask == NULL) return NULL; //not found frame to encode

    //...........................reflist control........................................
    eTask->prevTask = m_last_task;

    eTask->m_frameOrderIdr = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR) ? m_frameCount - 1 : (eTask->prevTask ? eTask->prevTask->m_frameOrderIdr : 0);
    eTask->m_frameOrderI   = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)   ? m_frameCount - 1 : (eTask->prevTask ? eTask->prevTask->m_frameOrderI    : 0);
    mfxU8  frameNumIncrement = (eTask->prevTask && (ExtractFrameType(*(eTask->prevTask)) & MFX_FRAMETYPE_REF || eTask->prevTask->m_nalRefIdc[0])) ? 1 : 0;
    eTask->m_frameNum = (eTask->prevTask && !(ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR)) ? mfxU16((eTask->prevTask->m_frameNum + frameNumIncrement) % (1 << m_frameNumMax)) : 0;

    eTask->m_picNum[0] = eTask->m_frameNum * (eTask->m_fieldPicFlag + 1) + eTask->m_fieldPicFlag;
    eTask->m_picNum[1] = eTask->m_picNum[0];

    eTask->m_dpb[eTask->m_fid[0]] = eTask->prevTask ? eTask->prevTask->m_dpbPostEncoding : ArrayDpbFrame();
    UpdateDpbFrames(*eTask, eTask->m_fid[0], 1 << m_frameNumMax);
    InitRefPicList(*eTask, eTask->m_fid[0]);
    ModifyRefPicLists(m_mfxEncParams, *eTask, m_ffid);
    MarkDecodedRefPictures(m_mfxEncParams, *eTask, eTask->m_fid[0]);
    if (eTask->m_fieldPicFlag)
    {
        UpdateDpbFrames(*eTask, eTask->m_fid[1], 1 << m_frameNumMax);
        InitRefPicList(*eTask, eTask->m_fid[1]);
        ModifyRefPicLists(m_mfxEncParams, *eTask, m_sfid);

        // mark second field of last added frame short-term ref
        eTask->m_dpbPostEncoding = eTask->m_dpb[eTask->m_fid[1]];
        if (eTask->m_reference[eTask->m_fid[1]])
            eTask->m_dpbPostEncoding.Back().m_refPicFlag[eTask->m_fid[1]] = 1;
    }

    return eTask;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL0SurfacesPreEnc(iTask* eTask, bool fair_reconstruct)
{
    mfxU32 task_processed = 0, size = eTask->m_list0[0].Size();
    mfxFrameSurface1 ** L0 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L0, NULL);
    MSDK_ZERO_ARRAY(L0, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list0[0].Begin(); task_processed < size && instance != eTask->m_list0[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L0);
            return NULL;
        }
        L0[task_processed++] = fair_reconstruct ? ref_task->outPAK.OutSurface : ref_task->in.InSurface;
    }

    return L0;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL1SurfacesPreEnc(iTask* eTask, bool fair_reconstruct)
{
    mfxU32 task_processed = 0, size = eTask->m_list1[0].Size();
    mfxFrameSurface1 ** L1 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L1, NULL);
    MSDK_ZERO_ARRAY(L1, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list1[0].Begin(); task_processed < size && instance != eTask->m_list1[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L1);
            return NULL;
        }
        L1[task_processed++] = fair_reconstruct ? ref_task->outPAK.OutSurface : ref_task->in.InSurface;
    }

    return L1;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL0SurfacesEnc(iTask* eTask, bool fair_reconstruct)
{
    mfxU32 task_processed = 0, size = eTask->m_list0[0].Size();
    mfxFrameSurface1 ** L0 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L0, NULL);
    MSDK_ZERO_ARRAY(L0, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list0[0].Begin(); task_processed < size && instance != eTask->m_list0[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L0);
            return NULL;
        }
        L0[task_processed++] = fair_reconstruct ? ref_task->outPAK.OutSurface : ref_task->out.OutSurface;
    }

    return L0;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL1SurfacesEnc(iTask* eTask, bool fair_reconstruct)
{
    mfxU32 task_processed = 0, size = eTask->m_list1[0].Size();
    mfxFrameSurface1 ** L1 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L1, NULL);
    MSDK_ZERO_ARRAY(L1, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list1[0].Begin(); task_processed < size && instance != eTask->m_list1[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L1);
            return NULL;
        }
        L1[task_processed++] = fair_reconstruct ? ref_task->outPAK.OutSurface : ref_task->out.OutSurface;
    }

    return L1;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL0SurfacesPak(iTask* eTask)
{
    mfxU32 task_processed = 0, size = eTask->m_list0[0].Size();
    mfxFrameSurface1 ** L0 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L0, NULL);
    MSDK_ZERO_ARRAY(L0, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list0[0].Begin(); task_processed < size && instance != eTask->m_list0[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L0);
            return NULL;
        }
        L0[task_processed++] = ref_task->outPAK.OutSurface;
    }

    return L0;
}

mfxFrameSurface1 ** CEncodingPipeline::GetCurrentL1SurfacesPak(iTask* eTask)
{
    mfxU32 task_processed = 0, size = eTask->m_list1[0].Size();
    mfxFrameSurface1 ** L1 = new mfxFrameSurface1*[size];
    MSDK_CHECK_POINTER(L1, NULL);
    MSDK_ZERO_ARRAY(L1, size);
    iTask* ref_task;

    for (mfxU8 const * instance = eTask->m_list1[0].Begin(); task_processed < size && instance != eTask->m_list1[0].End(); instance++)
    {
        ref_task = GetTaskByFrameOrder(eTask->m_dpb[0][*instance & 127].m_frameOrder);
        if (ref_task == NULL){
            MSDK_SAFE_DELETE_ARRAY(L1);
            return NULL;
        }
        L1[task_processed++] = ref_task->outPAK.OutSurface;
    }

    return L1;
}

/* initialization functions */

mfxStatus CEncodingPipeline::InitPreEncFrameParams(iTask* eTask)
{
    mfxExtFeiPreEncMVPredictors* pMvPredBuf = NULL;
    mfxExtFeiEncQP*              pMbQP      = NULL;
    mfxExtFeiPreEncCtrl*         preENCCtr  = NULL;

    eTask->preenc_bufs = getFreeBufSet(m_preencBufs);
    MSDK_CHECK_POINTER(eTask->preenc_bufs, MFX_ERR_NULL_PTR);

    switch (ExtractFrameType(*eTask) & MFX_FRAMETYPE_IPB) {
    case MFX_FRAMETYPE_I:
        eTask->in.NumFrameL0 = 0;
        eTask->in.NumFrameL1 = 0;
        eTask->in.L0Surface = eTask->in.L1Surface = NULL;
        //in data
        eTask->in.NumExtParam = eTask->preenc_bufs->I_bufs.in.NumExtParam;
        eTask->in.ExtParam    = eTask->preenc_bufs->I_bufs.in.ExtParam;
        //out data
        //exclude MV output from output buffers
        eTask->out.NumExtParam = eTask->preenc_bufs->I_bufs.out.NumExtParam;
        eTask->out.ExtParam    = eTask->preenc_bufs->I_bufs.out.ExtParam;
        break;

    case MFX_FRAMETYPE_P:
        eTask->in.NumFrameL0 = 1;// eTask->m_list0[0].Size();
        eTask->in.NumFrameL1 = 0;
        eTask->in.L0Surface = GetCurrentL0SurfacesPreEnc(eTask, m_encpakParams.bENCPAK);
        eTask->in.L1Surface = NULL;
        //in data
        eTask->in.NumExtParam = eTask->preenc_bufs->PB_bufs.in.NumExtParam;
        eTask->in.ExtParam    = eTask->preenc_bufs->PB_bufs.in.ExtParam;
        //out data
        eTask->out.NumExtParam = eTask->preenc_bufs->PB_bufs.out.NumExtParam;
        eTask->out.ExtParam    = eTask->preenc_bufs->PB_bufs.out.ExtParam;
        break;

    case MFX_FRAMETYPE_B:
        eTask->in.NumFrameL0 = 1;// eTask->m_list0[0].Size();
        eTask->in.NumFrameL1 = 1;// eTask->m_list1[0].Size();
        eTask->in.L0Surface = GetCurrentL0SurfacesPreEnc(eTask, m_encpakParams.bENCPAK);
        eTask->in.L1Surface = GetCurrentL1SurfacesPreEnc(eTask, m_encpakParams.bENCPAK);
        //in data
        eTask->in.NumExtParam = eTask->preenc_bufs->PB_bufs.in.NumExtParam;
        eTask->in.ExtParam    = eTask->preenc_bufs->PB_bufs.in.ExtParam;
        //out data
        eTask->out.NumExtParam = eTask->preenc_bufs->PB_bufs.out.NumExtParam;
        eTask->out.ExtParam    = eTask->preenc_bufs->PB_bufs.out.ExtParam;
        break;
    }

    for (int i = 0; i < eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_PREENC_CTRL:
            for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                preENCCtr = (mfxExtFeiPreEncCtrl*)(eTask->in.ExtParam[i]);
                preENCCtr->DisableMVOutput = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I) ? 1 : m_disableMVoutPreENC;
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
            if (pMvPred)
            {
                if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiPreEncMVPredictors*)(eTask->in.ExtParam[i]);
                    fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                }
                else{
                    fseek(pMvPred, sizeof(mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB)*m_numMB, SEEK_CUR);
                }
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP)
            {
                pMbQP = (mfxExtFeiEncQP*)(eTask->in.ExtParam[i]);
                fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
            }
            break;
        }
    } // for (int i = 0; i<eTask->in.NumExtParam; i++)

    mdprintf(stderr, "enc: %d t: %d %d\n", eTask->m_frameOrder, (eTask->m_type[0] & MFX_FRAMETYPE_IPB), (eTask->m_type[1] & MFX_FRAMETYPE_IPB));
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitEncFrameParams(iTask* eTask)
{
    mfxExtFeiEncMVPredictors* pMvPredBuf       = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl       = NULL;
    mfxExtFeiEncQP*           pMbQP            = NULL;
    mfxExtFeiSPS*             m_feiSPS         = NULL;
    mfxExtFeiPPS*             m_feiPPS         = NULL;
    mfxExtFeiSliceHeader*     m_feiSliceHeader = NULL;
    mfxExtFeiEncFrameCtrl*    feiEncCtrl       = NULL;

    eTask->bufs = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(eTask->bufs, MFX_ERR_NULL_PTR);

        if (m_encpakParams.bPassHeaders)
        {
            for (int i = 0; i < eTask->in.NumExtParam; i++)
            {
                switch (eTask->in.ExtParam[i]->BufferId){
                case MFX_EXTBUFF_FEI_PPS:
                    if (!m_feiPPS){
                        m_feiPPS = (mfxExtFeiPPS*)(eTask->in.ExtParam[i]);
                    }
                    break;
                case MFX_EXTBUFF_FEI_SPS:
                    m_feiSPS = (mfxExtFeiSPS*)(eTask->in.ExtParam[i]);
                    break;
                case MFX_EXTBUFF_FEI_SLICE:
                    if (!m_feiSliceHeader){
                        m_feiSliceHeader = (mfxExtFeiSliceHeader*)(eTask->in.ExtParam[i]);
                    }
                    break;
                }
            }
        }
        switch (ExtractFrameType(*eTask) & MFX_FRAMETYPE_IPB) {
            case MFX_FRAMETYPE_I:
                /* ENC data */
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = eTask->in.NumFrameL1 = 0;
                    eTask->in.L0Surface = eTask->in.L1Surface = NULL;
                    eTask->in.NumExtParam = eTask->bufs->I_bufs.in.NumExtParam;
                    eTask->in.ExtParam    = eTask->bufs->I_bufs.in.ExtParam;
                    eTask->out.NumExtParam = eTask->bufs->I_bufs.out.NumExtParam;
                    eTask->out.ExtParam    = eTask->bufs->I_bufs.out.ExtParam;
                }
                /* PAK data */
                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = eTask->inPAK.NumFrameL1 = 0;
                    eTask->inPAK.L0Surface = eTask->inPAK.L1Surface = NULL;
                    eTask->inPAK.NumExtParam = eTask->bufs->I_bufs.out.NumExtParam;
                    eTask->inPAK.ExtParam    = eTask->bufs->I_bufs.out.ExtParam;
                    eTask->outPAK.NumExtParam = eTask->bufs->I_bufs.in.NumExtParam;
                    eTask->outPAK.ExtParam    = eTask->bufs->I_bufs.in.ExtParam;
                }
                /* Process SPS, and PPS only*/
                if (m_encpakParams.bPassHeaders && m_feiPPS && m_feiSPS && m_feiSliceHeader)
                {
                    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
                    {
                        m_feiPPS[fieldId].NumRefIdxL0Active = 0;
                        m_feiPPS[fieldId].NumRefIdxL1Active = 0;
                        /*I is always reference */
                        m_feiPPS[fieldId].ReferencePicFlag = 1;
                        if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR)
                        {
                            m_feiPPS[fieldId].IDRPicFlag = 1;
                            m_feiPPS[fieldId].FrameNum = 0;
                            m_feiSPS->Pack = 1;
                            m_refFrameCounter = 1;
                        }
                        else
                        {
                            m_feiPPS[fieldId].IDRPicFlag = 0;
                            m_feiPPS[fieldId].FrameNum = m_refFrameCounter;
                            m_feiSPS->Pack = 0;
                        }

                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL0, 32);
                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL1, 32);
                    }
                    if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR))
                        m_refFrameCounter++;
                }
                break;

            case MFX_FRAMETYPE_P:
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = (mfxU16)eTask->m_list0[0].Size();//1;
                    eTask->in.NumFrameL1 = 0;
                    eTask->in.L0Surface = GetCurrentL0SurfacesEnc(eTask, m_encpakParams.bENCPAK);
                    eTask->in.L1Surface = NULL;
                    eTask->in.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
                    eTask->in.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
                    eTask->out.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
                    eTask->out.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
                }

                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = (mfxU16)eTask->m_list0[0].Size();//1;
                    eTask->inPAK.NumFrameL1 = 0;
                    eTask->inPAK.L0Surface = GetCurrentL0SurfacesPak(eTask);
                    eTask->inPAK.L1Surface = NULL;
                    eTask->inPAK.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
                    eTask->inPAK.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
                    eTask->outPAK.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
                    eTask->outPAK.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
                }

                /* PPS only */
                if (m_encpakParams.bPassHeaders && m_feiPPS && m_feiSPS && m_feiSliceHeader)
                {
                    m_feiSPS->Pack = 0;

                    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
                    {
                        m_feiPPS[fieldId].IDRPicFlag = 0;
                        m_feiPPS[fieldId].NumRefIdxL0Active = eTask->in.NumFrameL0;
                        m_feiPPS[fieldId].NumRefIdxL1Active = 0;
                        m_feiPPS[fieldId].ReferencePicFlag = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_REF) ? 1 : 0;

                        if (ExtractFrameType(*(eTask->prevTask)) & MFX_FRAMETYPE_REF)
                        {
                            m_feiPPS[fieldId].FrameNum = m_refFrameCounter;
                        }

                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL0, 32);
                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL1, 32);

                        for (mfxU16 k = 0; k < eTask->in.NumFrameL0; k++)
                        {
                            m_feiSliceHeader[fieldId].Slice->RefL0[0].Index = k;
                            m_feiSliceHeader[fieldId].Slice->RefL0[0].PictureType = (mfxU16)((m_numOfFields == 1) ? MFX_PICTYPE_FRAME :
                                ((eTask->m_list0[fieldId][k] & 128) ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                    }
                    if (ExtractFrameType(*(eTask->prevTask)) & MFX_FRAMETYPE_REF)
                        m_refFrameCounter++;
                }
                break;

            case MFX_FRAMETYPE_B:
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = (mfxU16)eTask->m_list0[0].Size();//1;
                    eTask->in.NumFrameL1 = (mfxU16)eTask->m_list1[0].Size();//1;
                    eTask->in.L0Surface = GetCurrentL0SurfacesEnc(eTask, m_encpakParams.bENCPAK);
                    eTask->in.L1Surface = GetCurrentL1SurfacesEnc(eTask, m_encpakParams.bENCPAK);
                    eTask->in.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
                    eTask->in.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
                    eTask->out.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
                    eTask->out.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
                }

                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = (mfxU16)eTask->m_list0[0].Size();//1;
                    eTask->inPAK.NumFrameL1 = (mfxU16)eTask->m_list1[0].Size();//1;
                    eTask->inPAK.L0Surface = GetCurrentL0SurfacesPak(eTask);
                    eTask->inPAK.L1Surface = GetCurrentL1SurfacesPak(eTask);
                    eTask->inPAK.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
                    eTask->inPAK.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
                    eTask->outPAK.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
                    eTask->outPAK.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
                }

                /* PPS only */
                if (m_encpakParams.bPassHeaders && m_feiPPS && m_feiSPS && m_feiSliceHeader)
                {
                    m_feiSPS->Pack = 0;

                    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
                    {
                        m_feiPPS[fieldId].IDRPicFlag = 0;
                        m_feiPPS[fieldId].NumRefIdxL0Active = eTask->in.NumFrameL0;
                        m_feiPPS[fieldId].NumRefIdxL1Active = eTask->in.NumFrameL1;
                        m_feiPPS[fieldId].ReferencePicFlag = 0;
                        m_feiPPS[fieldId].IDRPicFlag = 0;
                        m_feiPPS[fieldId].FrameNum = m_refFrameCounter;

                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL0, 32);
                        MSDK_ZERO_ARRAY(m_feiSliceHeader[fieldId].Slice->RefL1, 32);

                        for (mfxU16 k = 0; k < eTask->in.NumFrameL0; k++)
                        {
                            m_feiSliceHeader[fieldId].Slice->RefL0[0].Index = k;
                            m_feiSliceHeader[fieldId].Slice->RefL0[0].PictureType = (mfxU16)((m_numOfFields == 1) ? MFX_PICTYPE_FRAME :
                                ((eTask->m_list0[fieldId][k] & 128) ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                        for (mfxU16 k = 0; k < eTask->in.NumFrameL1; k++)
                        {
                            m_feiSliceHeader[fieldId].Slice->RefL1[0].Index = k;
                            m_feiSliceHeader[fieldId].Slice->RefL1[0].PictureType = (mfxU16)((m_numOfFields == 1) ? MFX_PICTYPE_FRAME :
                                ((eTask->m_list1[fieldId][k] & 128) ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                    }
                }
                break;
        }

    for (int i = 0; i<eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_CTRL:

            feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(eTask->in.ExtParam[i]);

            feiEncCtrl->MVPredictor = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I) ? 0 : (m_encpakParams.mvinFile != NULL) || m_encpakParams.bPREENC;

            // adjust ref window size if search window is 0
            if (m_encpakParams.SearchWindow == 0 && m_pmfxENC)
            {
                // window size is limited to 1024 for bi-prediction
                if ((ExtractFrameType(*eTask) & MFX_FRAMETYPE_B) && m_encpakParams.RefHeight * m_encpakParams.RefWidth > 1024)
                {
                    feiEncCtrl->RefHeight = 32;
                    feiEncCtrl->RefWidth  = 32;
                }
                else{
                    feiEncCtrl->RefHeight = m_encpakParams.RefHeight;
                    feiEncCtrl->RefWidth  = m_encpakParams.RefWidth;
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (pMvPred && !m_encpakParams.bPREENC)
            {
                if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(eTask->in.ExtParam[i]);
                    if (m_encpakParams.bRepackPreencMV)
                    {
                        fread(m_tmpForReading, sizeof(*m_tmpForReading)*m_numMB, 1, pMvPred);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, m_numMB, m_tmpForMedian);
                    }
                    else {
                        fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                    }
                }
                else{
                    int shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    fseek(pMvPred, shft*m_numMB, SEEK_CUR);
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs)
            {
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]);
                fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP)
            {
                pMbQP = (mfxExtFeiEncQP*)(eTask->in.ExtParam[i]);
                fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
            }
            break;
        } // switch (eTask->in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<eTask->in.NumExtParam; i++)

    mdprintf(stderr, "enc: %d t: %d %d\n", eTask->m_frameOrder, (eTask->m_type[0] & MFX_FRAMETYPE_IPB), (eTask->m_type[1] & MFX_FRAMETYPE_IPB));
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, int frameType)
{
    bufSet * freeSet = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(freeSet, MFX_ERR_NULL_PTR);
    pCurrentTask->bufs.push_back(std::pair<bufSet*, mfxFrameSurface1*>(freeSet, encodeSurface));

    /* Load input Buffer for FEI ENCODE */
    mfxExtFeiEncMVPredictors* pMvPredBuf = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
    mfxExtFeiEncQP*           pMbQP      = NULL;
    mfxExtFeiEncFrameCtrl*    feiEncCtrl = NULL;


    for (int i = 0; i < freeSet->PB_bufs.in.NumExtParam; i++)
    {
        switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId){

        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (!m_encpakParams.bPREENC && pMvPred)
            {
                if (!(frameType & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(freeSet->PB_bufs.in.ExtParam[i]);
                    if (m_encpakParams.bRepackPreencMV)
                    {
                        fread(m_tmpForReading, sizeof(*m_tmpForReading)*m_numMB, 1, pMvPred);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, m_numMB, m_tmpForMedian);
                    }
                    else {
                        fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                    }
                }
                else{
                    int shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    fseek(pMvPred, shft*m_numMB, SEEK_CUR);
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs){
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
                fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP){
                pMbQP = (mfxExtFeiEncQP*)(freeSet->PB_bufs.in.ExtParam[i]);
                fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_CTRL:
            feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);

            // adjust ref window size if search window is 0
            if (m_encpakParams.SearchWindow == 0)
            {
                if ((frameType & MFX_FRAMETYPE_B) && m_encpakParams.RefHeight * m_encpakParams.RefWidth > 1024)
                {
                    feiEncCtrl->RefHeight = 32;
                    feiEncCtrl->RefWidth  = 32;
                }
                else{
                    feiEncCtrl->RefHeight = m_encpakParams.RefHeight;
                    feiEncCtrl->RefWidth  = m_encpakParams.RefWidth;
                }
            }

            feiEncCtrl->MVPredictor = (!(frameType & MFX_FRAMETYPE_I) && (m_encpakParams.mvinFile != NULL || m_encpakParams.bPREENC)) ? 1 : 0;
            break;
        } // switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<freeSet->PB_bufs.in.NumExtParam; i++)

    // Add input buffers
    if (freeSet->PB_bufs.in.NumExtParam > 0) {
        if (frameType & MFX_FRAMETYPE_I){
            encodeSurface->Data.NumExtParam = freeSet->I_bufs.in.NumExtParam;
            encodeSurface->Data.ExtParam    = freeSet->I_bufs.in.ExtParam;
            m_ctr->NumExtParam = freeSet->I_bufs.in.NumExtParam;
            m_ctr->ExtParam = freeSet->I_bufs.in.ExtParam;
        }
        else
        {
            encodeSurface->Data.NumExtParam = freeSet->PB_bufs.in.NumExtParam;
            encodeSurface->Data.ExtParam    = freeSet->PB_bufs.in.ExtParam;
            m_ctr->NumExtParam = freeSet->PB_bufs.in.NumExtParam;
            m_ctr->ExtParam = freeSet->PB_bufs.in.ExtParam;
        }
    }

    // Add output buffers
    if (freeSet->PB_bufs.out.NumExtParam > 0) {
        pCurrentTask->mfxBS.NumExtParam = freeSet->PB_bufs.out.NumExtParam;
        pCurrentTask->mfxBS.ExtParam    = freeSet->PB_bufs.out.ExtParam;
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::ReadPAKdata(iTask* eTask)
{
    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;

    for (int i = 0; i < eTask->inPAK.NumExtParam; i++){
        switch (eTask->inPAK.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout){
                mvBuf = (mfxExtFeiEncMV*)(eTask->inPAK.ExtParam[i]);
                fread(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(eTask->inPAK.ExtParam[i]);
                fread(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
            }
            break;
        } // switch (eTask->inPAK.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->inPAK.NumExtParam; i++)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::DropENCPAKoutput(iTask* eTask)
{
    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiEncMBStat* mbstatBuf = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
    for (int i = 0; i < eTask->out.NumExtParam; i++){

        switch (eTask->out.ExtParam[i]->BufferId){

        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout)
            {
                if (!(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
                    mvBuf = (mfxExtFeiEncMV*)(eTask->out.ExtParam[i]);
                    fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                }
                else {
                    for (int k = 0; k < m_numMB; k++)
                    {
                        fwrite(m_tmpMBenc, sizeof(*m_tmpMBenc), 1, mvENCPAKout);
                    }
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (mbstatout){
                mbstatBuf = (mfxExtFeiEncMBStat*)(eTask->out.ExtParam[i]);
                fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(eTask->out.ExtParam[i]);
                fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
            }
            break;
        } // switch (eTask->out.ExtParam[i]->BufferId)
    } // for(int i=0; i<eTask->out.NumExtParam; i++)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::DropPREENCoutput(iTask* eTask)
{
    mfxExtFeiPreEncMBStat* mbdata = NULL;
    mfxExtFeiPreEncMV*     mvs    = NULL;
    for (int i = 0; i < eTask->out.NumExtParam; i++)
    {

        switch (eTask->out.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_PREENC_MB:
            if (mbstatout){
                mbdata = (mfxExtFeiPreEncMBStat*)(eTask->out.ExtParam[i]);
                fwrite(mbdata->MB, sizeof(mbdata->MB[0]) * mbdata->NumMBAlloc, 1, mbstatout);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_MV:
            if (mvout){
                if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I){
                    /*for (int k = 0; k < numMB; k++){                       // we never get here because mvout buffer
                        fwrite(tmpMBpreenc, sizeof(*tmpMBpreenc), 1, mvout); // detached for I frames
                    }*/
                }
                else{
                    mvs = (mfxExtFeiPreEncMV*)(eTask->out.ExtParam[i]);
                    fwrite(mvs->MB, sizeof(mvs->MB[0]) * mvs->NumMBAlloc, 1, mvout);
                }
            }
            break;
        } // switch (eTask->out.ExtParam[i]->BufferId)
    } //for (int i = 0; i < eTask->out.NumExtParam; i++)

    if (mvout && (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
        for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
        {
            for (int k = 0; k < m_numMB; k++){
                fwrite(m_tmpMBpreenc, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB), 1, mvout);
            }
        }
    }

    return MFX_ERR_NONE;
}


mfxStatus dropENCODEoutput(mfxBitstream& bs)
{
    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiEncMBStat* mbstatBuf = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;

    for (int i = 0; i < bs.NumExtParam; i++)
    {
        switch (bs.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout)
            {
                mvBuf = (mfxExtFeiEncMV*)(bs.ExtParam[i]);
                if (!(bs.FrameType & MFX_FRAMETYPE_I)){
                    fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                }
                else{
                    mfxExtFeiEncMV::mfxExtFeiEncMVMB tmpMB;
                    memset(&tmpMB, 0x8000, sizeof(tmpMB));
                    for (mfxU32 k = 0; k < mvBuf->NumMBAlloc; k++)
                        fwrite(&tmpMB, sizeof(tmpMB), 1, mvENCPAKout);
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (mbstatout){
                mbstatBuf = (mfxExtFeiEncMBStat*)(bs.ExtParam[i]);
                fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(bs.ExtParam[i]);
                fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
            }
            break;
        } // switch (bs.ExtParam[i]->BufferId)
    } // for (int i = 0; i < bs.NumExtParam; i++)

    return MFX_ERR_NONE;
}

void CEncodingPipeline::UpdateTaskQueue(iTask* eTask){
    eTask->encoded = 1;
    if (!m_last_task)
        m_last_task = new iTask;

    CopyState(eTask);
    if (this->m_refDist*2 <= m_inputTasks.size())
    {
        RemoveOneTask();
    }
}

void CEncodingPipeline::CopyState(iTask* eTask)
{
    m_last_task->m_frameOrderIdr   = eTask->m_frameOrderIdr;
    m_last_task->m_frameOrderI     = eTask->m_frameOrderI;
    m_last_task->m_nalRefIdc       = eTask->m_nalRefIdc;
    m_last_task->m_frameNum        = eTask->m_frameNum;
    m_last_task->m_type            = eTask->m_type;
    m_last_task->m_dpbPostEncoding = eTask->m_dpbPostEncoding;
}

void CEncodingPipeline::RemoveOneTask()
{
    ArrayDpbFrame & dpb = m_last_task->m_dpbPostEncoding;
    std::list<mfxU32> FramesInDPB;

    for (mfxU32 i = 0; i < dpb.Size(); i++)
        FramesInDPB.push_back(dpb[i].m_frameOrder);

    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        if ((*it) && std::find(FramesInDPB.begin(), FramesInDPB.end(), (*it)->m_frameOrder) == FramesInDPB.end() && (*it)->encoded == 1) // delete task
        {
            if ((*it)->bufs){
                (*it)->bufs->vacant = true;
                (*it)->bufs = NULL;
            }
            if ((*it)->preenc_bufs){
                (*it)->preenc_bufs->vacant = true;
                (*it)->preenc_bufs = NULL;
            }
            MSDK_SAFE_DELETE_ARRAY((*it)->in.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->in.L1Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L1Surface);

            if ((*it)->outPAK.OutSurface && (*it)->outPAK.OutSurface->Data.Locked){
                /* reconstructed surface may be used by other tasks
                 * so only decrement */
                (*it)->outPAK.OutSurface->Data.Locked--;
            }
            if ((*it)->in.InSurface && (*it)->in.InSurface->Data.Locked){
                /* Source surface is free to use*/
                (*it)->in.InSurface->Data.Locked = 0;
            }

            delete (*it);
            m_inputTasks.erase(it);
            return;
        }
    }
}

void CEncodingPipeline::ClearTasks()
{
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        if (*it)
        {
            if ((*it)->bufs){
                (*it)->bufs->vacant = true;
                (*it)->bufs = NULL;
            }
            if ((*it)->preenc_bufs){
                (*it)->preenc_bufs->vacant = true;
                (*it)->preenc_bufs = NULL;
            }

            MSDK_SAFE_DELETE_ARRAY((*it)->in.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->in.L1Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L1Surface);

            if ((*it)->in.InSurface)
                (*it)->in.InSurface->Data.Locked = 0;
            if ((*it)->outPAK.OutSurface)
                (*it)->outPAK.OutSurface->Data.Locked = 0;

            delete (*it);
        }
    }

    m_inputTasks.clear();
}

mfxU32 CEncodingPipeline::CountUnencodedFrames()
{
    mfxU32 numUnencoded = 0;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it){
        if ((*it)->encoded == 0){
            numUnencoded++;
        }
    }

    return numUnencoded;
}

bufSet* getFreeBufSet(std::list<bufSet*> bufs)
{
    for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it){
        if ((*it)->vacant)
        {
            (*it)->vacant = false;
            return (*it);
        }
    }

    return NULL;
}

mfxStatus CEncodingPipeline::FreeBuffers(std::list<bufSet*> bufs)
{
    for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it)
    {
        (*it)->vacant = false;

        for (int i = 0; i < (*it)->PB_bufs.in.NumExtParam; /*i++*/)
        {
            switch ((*it)->PB_bufs.in.ExtParam[i]->BufferId)
            {
            case MFX_EXTBUFF_FEI_PREENC_CTRL:
            {
                mfxExtFeiPreEncCtrl* preENCCtr = (mfxExtFeiPreEncCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(preENCCtr);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
            {
                mfxExtFeiPreEncMVPredictors* mvPreds = (mfxExtFeiPreEncMVPredictors*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mvPreds[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mvPreds);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_PREENC_QP:
            {
                mfxExtFeiEncQP* qps = (mfxExtFeiEncQP*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(qps[fieldId].QP);
                }
                MSDK_SAFE_DELETE_ARRAY(qps);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_ENC_CTRL:
            {
                mfxExtFeiEncFrameCtrl* feiEncCtrl = (mfxExtFeiEncFrameCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(feiEncCtrl);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_SPS:
            {
                mfxExtFeiSPS* m_feiSPS = (mfxExtFeiSPS*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE(m_feiSPS);
                i++;
            }
                break;

            case MFX_EXTBUFF_FEI_PPS:
            {
                mfxExtFeiPPS* m_feiPPS = (mfxExtFeiPPS*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(m_feiPPS);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_SLICE:
            {
                mfxExtFeiSliceHeader* m_feiSliceHeader = (mfxExtFeiSliceHeader*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(m_feiSliceHeader[fieldId].Slice);
                }
                MSDK_SAFE_DELETE_ARRAY(m_feiSliceHeader);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            {
                mfxExtFeiEncMVPredictors* feiEncMVPredictors = (mfxExtFeiEncMVPredictors*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_ENC_MB:
            {
                mfxExtFeiEncMBCtrl* feiEncMBCtrl = (mfxExtFeiEncMBCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl);
                i += m_numOfFields;
            }
                break;

            default:
                i++;
                break;
            } // switch ((*it)->PB_bufs.in.ExtParam[i]->BufferId)
        } // for (int i = 0; i < (*it)->PB_bufs.in.NumExtParam; )

        MSDK_SAFE_DELETE_ARRAY((*it)->PB_bufs.in.ExtParam);
        MSDK_SAFE_DELETE_ARRAY((*it)-> I_bufs.in.ExtParam);

        for (int i = 0; i < (*it)->PB_bufs.out.NumExtParam; /*i++*/)
        {
            switch ((*it)->PB_bufs.out.ExtParam[i]->BufferId)
            {
            case MFX_EXTBUFF_FEI_PREENC_MV:
            {
                mfxExtFeiPreEncMV* mvs = (mfxExtFeiPreEncMV*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mvs[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mvs);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_PREENC_MB:
            {
                mfxExtFeiPreEncMBStat* mbdata = (mfxExtFeiPreEncMBStat*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mbdata[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mbdata);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            {
                mfxExtFeiEncMBStat* feiEncMbStat = (mfxExtFeiEncMBStat*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMbStat[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMbStat);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_ENC_MV:
            {
                mfxExtFeiEncMV* feiEncMV = (mfxExtFeiEncMV*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMV[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMV);
                i += m_numOfFields;
            }
                break;

            case MFX_EXTBUFF_FEI_PAK_CTRL:
            {
                mfxExtFeiPakMBCtrl* feiEncMBCode = (mfxExtFeiPakMBCtrl*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCode[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMBCode);
                i += m_numOfFields;
            }
                break;

            default:
                i++;
                break;
            } // switch ((*it)->PB_bufs.out.ExtParam[i]->BufferId)
        } // for (int i = 0; i < (*it)->PB_bufs.out.NumExtParam; )

        MSDK_SAFE_DELETE_ARRAY((*it)->PB_bufs.out.ExtParam);
        MSDK_SAFE_DELETE_ARRAY((*it)-> I_bufs.out.ExtParam);

    } // for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it)

    bufs.clear();

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1 *pDecSurf = NULL;
    pExtSurface->pSurface      = NULL;

    while (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            // Wait 1ms will be probably enough to device release
            mfxStatus stsSync = m_decode_mfxSession.SyncOperation(m_LastDecSyncPoint, 1);
            if (stsSync < 0)
            {
                sts = stsSync;
            }
        }
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_BSReader.ReadNextFrame(&m_mfxBS); // read more data to input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            // find free surface for decoder input
            mfxU16 nDecSurfIdx = GetFreeSurface(m_pDecSurfaces, m_DecResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nDecSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
            pDecSurf = &m_pDecSurfaces[nDecSurfIdx];

            MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found
        }

        sts = m_pmfxDECODE->DecodeFrameAsync(&m_mfxBS, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);

        if (!sts)
        {
            m_LastDecSyncPoint = pExtSurface->Syncp;
        }
        // ignore warnings if output is available,
        if (MFX_ERR_NONE < sts && pExtSurface->Syncp)
        {
            sts = MFX_ERR_NONE;
        }

    } //while processing

    return sts;
}

mfxStatus CEncodingPipeline::DecodeLastFrame(ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxFrameSurface1 *pDecSurf = NULL;
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    pExtSurface->pSurface = NULL;

    // retrieve the buffered decoded frames
    while (MFX_ERR_MORE_SURFACE == sts || MFX_WRN_DEVICE_BUSY == sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            // Wait 1ms will be probably enough to device release
            mfxStatus stsSync = m_decode_mfxSession.SyncOperation(m_LastDecSyncPoint, 1);
            if (stsSync < 0)
            {
                sts = stsSync;
            }
        }
        // find free surface for decoder input
        mfxU16 nDecSurfIdx = GetFreeSurface(m_pDecSurfaces, m_DecResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nDecSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        pDecSurf = &m_pDecSurfaces[nDecSurfIdx];

        MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found

        sts = m_pmfxDECODE->DecodeFrameAsync(NULL, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);
    }
    return sts;
}

mfxStatus CEncodingPipeline::VPPOneFrame(mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);
    mfxFrameSurface1 *pmfxSurface = NULL;

    // find/wait for a free working surface
    mfxU16 nVPPSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
    MSDK_CHECK_ERROR(nVPPSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
    pmfxSurface = &m_pEncSurfaces[nVPPSurfIdx];

    MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC);

    // make sure picture structure has the initial value
    // surfaces are reused and VPP may change this parameter in certain configurations
    pmfxSurface->Info.PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct;

    pExtSurface->pSurface = pmfxSurface;
    mfxStatus sts = MFX_ERR_NONE;
    for (;;)
    {
        sts = m_pmfxVPP->RunFrameVPPAsync(pSurfaceIn, pmfxSurface, NULL, &pExtSurface->Syncp);

        if (!pExtSurface->Syncp)
        {
            if (MFX_WRN_DEVICE_BUSY == sts)
                MSDK_SLEEP(1); // wait if device is busy
            else
                return sts;
        }
        break;
    }

    while (true)
    {
        sts =  m_pVPP_mfxSession->SyncOperation(pExtSurface->Syncp, MSDK_WAIT_INTERVAL);

        if (!pExtSurface->Syncp)
        {
            if (MFX_WRN_DEVICE_BUSY == sts)
                MSDK_SLEEP(1); // wait if device is busy
            else
                return sts;
        }
        else if (MFX_ERR_NONE <= sts) {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            break;
        }
    }

    return sts;
}

