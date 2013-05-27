/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp8_encode_utils_hw.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE)
#include "assert.h"

namespace MFX_VP8ENC
{  
    bool isVideoSurfInput(mfxVideoParam const & video)
    {
        mfxExtOpaqueSurfaceAlloc * pOpaq = GetExtBuffer(video);

        if (video.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
            return true;
        if (isOpaq(video) && pOpaq)
        {
            if (pOpaq->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
            {
                return true;
            }         
        }
        return false;
    }


    mfxStatus CheckEncodeFrameParam(
        mfxVideoParam const & video,
        mfxEncodeCtrl       * ctrl,
        mfxFrameSurface1    * surface,
        mfxBitstream        * bs,
        bool                  isExternalFrameAllocator)
    {
        mfxStatus checkSts = MFX_ERR_NONE;
        MFX_CHECK_NULL_PTR1(bs);

        if (surface != 0)
        {
            MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(surface->Data.Y != 0 || isExternalFrameAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);

            if (surface->Info.Width != video.mfx.FrameInfo.Width || surface->Info.Height != video.mfx.FrameInfo.Height)
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        else
        {
            checkSts = MFX_ERR_MORE_DATA;
        }

        MFX_CHECK(((mfxI32)bs->MaxLength - ((mfxI32)bs->DataOffset + (mfxI32)bs->DataLength) >= (mfxI32)video.mfx.BufferSizeInKB*1000), MFX_ERR_NOT_ENOUGH_BUFFER);

        if (ctrl)
        {
            MFX_CHECK (ctrl->QP <= 63, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            MFX_CHECK (ctrl->FrameType <= MFX_FRAMETYPE_P, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);    
        }

        return checkSts;

    }
    mfxStatus GetVideoParam(mfxVideoParam * par, mfxVideoParam * parSrc)
    {
        MFX_CHECK_NULL_PTR1(par);

        mfxExtCodingOptionVP8 *   optDst  = (mfxExtCodingOptionVP8*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VP8_EX_CODING_OPT); 
        mfxExtOpaqueSurfaceAlloc* opaqDst = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION); 

        par->mfx        = parSrc->mfx;
        par->Protected  = parSrc->Protected;
        par->IOPattern  = parSrc->IOPattern;
        par->AsyncDepth = parSrc->AsyncDepth;

        if (optDst) 
        {
            mfxExtCodingOptionVP8 * optSrc = GetExtBuffer(*parSrc);
            if (optSrc)
            {
                *optDst = *optSrc;
            }
            else
            {
                mfxExtBuffer header = optDst->Header;
                memset(optDst,0,sizeof(mfxExtCodingOptionVP8));
                optDst->Header = header;
            }        
        }
        if (opaqDst)
        {
            mfxExtOpaqueSurfaceAlloc * opaqSrc = GetExtBuffer(*parSrc);
            if (opaqSrc)
            {
                *opaqDst = *opaqSrc;
            }
            else
            {
                mfxExtBuffer header = opaqDst->Header;
                memset(opaqDst,0,sizeof(mfxExtCodingOptionVP8));
                opaqDst->Header = header;
            }     
        }

        return MFX_ERR_NONE;

    } 
    mfxStatus SetFramesParams(mfxVideoParam * par, mfxU32 frameOrder, sFrameParams *pFrameParams)
    {
        pFrameParams->bAltRef = 0;
        pFrameParams->bGold   = 0;
        pFrameParams->bIntra  = (frameOrder % par->mfx.GopPicSize) == 0 ? true: false; 

        return MFX_ERR_NONE;  
    }

    //---------------------------------------------------------
    // service class: MfxFrameAllocResponse
    //---------------------------------------------------------

    MfxFrameAllocResponse::MfxFrameAllocResponse()
        : m_core (0)
        , m_numFrameActualReturnedByAllocFrames(0)
    {
        Zero(dynamic_cast<mfxFrameAllocResponse&>(*this));
    }

    MfxFrameAllocResponse::~MfxFrameAllocResponse()
    {
        if (m_core == 0)
            return;

        if (MFX_HW_D3D11  == m_core->GetVAType())
        {
            for (size_t i = 0; i < m_responseQueue.size(); i++)
            {
                m_core->FreeFrames(&m_responseQueue[i]);
            }
        }
        else
        {
            if (mids)
            {
                NumFrameActual = m_numFrameActualReturnedByAllocFrames;
                m_core->FreeFrames(this);
            }
        }

    } 


    mfxStatus MfxFrameAllocResponse::Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req)
    {
        req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

        if (MFX_HW_D3D11  == core->GetVAType())
        {
            mfxFrameAllocRequest tmp = req;
            tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

            m_responseQueue.resize(req.NumFrameMin);
            m_mids.resize(req.NumFrameMin);

            for (int i = 0; i < req.NumFrameMin; i++)
            {
                mfxStatus sts = core->AllocFrames(&tmp, &m_responseQueue[i]);
                MFX_CHECK_STS(sts);
                m_mids[i] = m_responseQueue[i].mids[0];
            }

            mids = &m_mids[0];
            NumFrameActual = req.NumFrameMin;
        }
        else
        {
            mfxStatus sts = core->AllocFrames(&req, this);
            MFX_CHECK_STS(sts);
        }

        if (NumFrameActual < req.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        m_core = core;
        m_numFrameActualReturnedByAllocFrames = NumFrameActual;
        NumFrameActual = req.NumFrameMin; // no need in redundant frames
        m_info = req.Info;

        return MFX_ERR_NONE;

    } 


    mfxStatus MfxFrameAllocResponse::Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req,
        mfxFrameSurface1 **    opaqSurf,
        mfxU32                 numOpaqSurf)
    {
        req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

        mfxStatus sts = core->AllocFrames(&req, this, opaqSurf, numOpaqSurf);
        MFX_CHECK_STS(sts);

        if (NumFrameActual < req.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        m_core = core;
        m_numFrameActualReturnedByAllocFrames = NumFrameActual;
        NumFrameActual = req.NumFrameMin; // no need in redundant frames

        return MFX_ERR_NONE;
    } 

    //---------------------------------------------------------
    // service class: VP8MfxParam
    //---------------------------------------------------------

    VP8MfxParam::VP8MfxParam()
    {
        memset(this, 0, sizeof(*this));
    }

    VP8MfxParam::VP8MfxParam(VP8MfxParam const & par)
    {
        Construct(par);
    }

    VP8MfxParam::VP8MfxParam(mfxVideoParam const & par)
    {
        Construct(par);
    }

    VP8MfxParam& VP8MfxParam::operator=(VP8MfxParam const & par)
    {
        Construct(par);

        return *this;
    }

    VP8MfxParam& VP8MfxParam::operator=(mfxVideoParam const & par)
    {
        Construct(par);

        return *this;
    }

    void VP8MfxParam::Construct(mfxVideoParam const & par)
    {
        mfxVideoParam & base = *this;
        base = par;

        Zero(m_extParam);

        InitExtBufHeader(m_extOpt);    
        InitExtBufHeader(m_extOpaque);

        if (mfxExtCodingOptionVP8 * opts = GetExtBuffer(par))
            m_extOpt = *opts;

        if (mfxExtOpaqueSurfaceAlloc * opts = GetExtBuffer(par))
            m_extOpaque = *opts;


        m_extParam[0]  = &m_extOpt.Header;    
        m_extParam[1]  = &m_extOpaque.Header;

        ExtParam = m_extParam;
        NumExtParam = mfxU16(sizeof m_extParam / sizeof m_extParam[0]);
        assert(NumExtParam == 2);
    }

    //---------------------------------------------------------
    // service class: ExternalFrames
    //---------------------------------------------------------

    void ExternalFrames::Init()
    {
        m_frames.resize(1000);
        Zero(m_frames);
        {
            mfxU32 i = 0;
            std::vector<sFrameEx>::iterator frame = m_frames.begin();
            for ( ;frame!= m_frames.end(); frame++)
            {
                frame->idInPool = i++;
            }
        }
    }
    mfxStatus ExternalFrames::GetFrame(mfxFrameSurface1 *pInFrame, sFrameEx *&pOutFrame )
    {
        mfxStatus sts = MFX_ERR_UNDEFINED_BEHAVIOR;

        std::vector<sFrameEx>::iterator frame = m_frames.begin();
        for ( ;frame!= m_frames.end(); frame++)
        {
            if (frame->pSurface == 0)
            {
                frame->pSurface = pInFrame; /*add frame to pool*/
                pOutFrame = &frame[0];
                return MFX_ERR_NONE;
            }
            if (frame->pSurface == pInFrame)
            {
                pOutFrame = &frame[0];
                return MFX_ERR_NONE;                
            }            
        }
        return sts;
    }
    //---------------------------------------------------------
    // service class: InternalFrames
    //---------------------------------------------------------

    mfxStatus InternalFrames::Init(VideoCORE *pCore, mfxFrameInfo *pFrameInfo, mfxU32 nFrames, bool bHW)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxFrameAllocRequest req = {0};
        MFX_CHECK_NULL_PTR2 (pCore, pFrameInfo);
        
        if (nFrames == 0) return sts;

        req.NumFrameMin  = req.NumFrameSuggested = (mfxU16) nFrames;
        req.Info = *pFrameInfo;
        req.Type = (mfxU16)(bHW ? MFX_MEMTYPE_D3D_INT: MFX_MEMTYPE_SYS_INT);

        sts = m_response.Alloc(pCore,req);
        MFX_CHECK_STS(sts);

        m_surfaces.resize(nFrames);
        Zero(m_surfaces);

        m_frames.resize(nFrames);
        Zero(m_frames);

        for (mfxU32 i = 0; i < nFrames; i++)
        {
            m_frames[i].idInPool  = i;
            m_surfaces[i].Data.MemId = m_response.mids[i]; 
            m_surfaces[i].Info = *pFrameInfo;
            m_frames[i].pSurface = &m_surfaces[i];
        }
        return sts;
    } 
    sFrameEx * InternalFrames::GetFreeFrame()
    {
        std::vector<sFrameEx>::iterator frame = m_frames.begin();
        for (;frame != m_frames.end(); frame ++)
        {
            if (isFreeSurface(&frame[0]))
            {
                return &frame[0]; 
            }            
        }
        return 0;
    }
    mfxStatus  InternalFrames::GetFrame(mfxU32 numFrame, sFrameEx * &Frame)
    {
        MFX_CHECK(numFrame < m_frames.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
        
        if (isFreeSurface(&m_frames[numFrame]))
        {
            Frame = &m_frames[numFrame]; 
        }
        return MFX_WRN_DEVICE_BUSY;   
    }

    //---------------------------------------------------------
    // service class: Task
    //---------------------------------------------------------

    mfxStatus Task::GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal)
    {
        if (m_bOpaqInput)
        {
            pSurface = m_pCore->GetNativeSurface(m_pRawFrame->pSurface);
            MFX_CHECK_NULL_PTR1(pSurface);
            bExternal = false;
        }
        else
        {
             pSurface = m_pRawFrame->pSurface;
             bExternal = true;        
        }
        return MFX_ERR_NONE;
    }
    mfxStatus Task::GetInputSurface(mfxFrameSurface1 *& pSurface, bool &bExternal)
    {
        if (m_pRawLocalFrame)
        {
            pSurface = m_pRawLocalFrame->pSurface;
            bExternal = false;        
        }
        else
        {
             MFX_CHECK_STS(GetOriginalSurface(pSurface, bExternal)) ;      
        }
        return MFX_ERR_NONE;
    }

    mfxStatus Task::CopyInput()
    {
        mfxStatus sts = MFX_ERR_NONE;

        if (m_pRawLocalFrame)
        {
            mfxFrameSurface1 src={0};

            mfxFrameSurface1 * pInput = 0;
            bool bExternal = true;

            sts = GetOriginalSurface(pInput,  bExternal);
            MFX_CHECK_STS(sts);

            src.Data = pInput->Data;
            src.Info = m_pRawLocalFrame->pSurface->Info;

            FrameLocker lockSrc(m_pCore, src.Data, bExternal);
            FrameLocker lockDst(m_pCore, m_pRawLocalFrame->pSurface->Data, false); 

            sts = m_pCore->DoFastCopy(m_pRawLocalFrame->pSurface, &src);
            MFX_CHECK_STS(sts);
        
        }
        return sts;        
    }
    mfxStatus Task::Init(VideoCORE * pCore, mfxVideoParam *par)
    {
        MFX_CHECK(m_status == TASK_FREE, MFX_ERR_UNDEFINED_BEHAVIOR);

        m_pCore       = pCore;
        m_bOpaqInput  = isOpaq(*par);

        return MFX_ERR_NONE;
    }
    mfxStatus Task::InitTask(   sFrameEx     *pRawFrame, 
                                mfxBitstream *pBitsteam,
                                mfxU32        frameOrder)
    {
        MFX_CHECK_NULL_PTR1(m_pCore);
        
        m_status        = TASK_FREE;
        m_pRawFrame     = pRawFrame;
        m_pBitsteam     = pBitsteam;
        m_frameOrder    = frameOrder;

        Zero(m_sFrameParams);

        MFX_CHECK_STS(LockSurface(m_pRawFrame,m_pCore));

        m_status = TASK_INITIALIZED;

        return MFX_ERR_NONE;       
    }

    mfxStatus Task::SubmitTask (sFrameEx*  pRecFrame, Task* pPrevTask, sFrameParams* pParams, sFrameEx* pRawLocalFrame)
    {
        MFX_CHECK(m_status == TASK_INITIALIZED, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_NULL_PTR2(pRecFrame, pParams);
        MFX_CHECK(isFreeSurface(pRecFrame),MFX_ERR_UNDEFINED_BEHAVIOR);

        m_sFrameParams   = *pParams;
        m_pRecFrame      = pRecFrame;
        m_pRawLocalFrame = pRawLocalFrame;

        MFX_CHECK_STS (CopyInput());

        m_pRecFrame->pSurface->Data.FrameOrder = m_frameOrder;

        if (pPrevTask && !m_sFrameParams.bIntra)
        {
            m_pRecRefFrames[REF_BASE] = pPrevTask->m_pRecFrame;

            m_pRecRefFrames[REF_GOLD] = (pPrevTask->m_sFrameParams.bGold) ? 
                pPrevTask->m_pRecFrame : pPrevTask->m_pRecRefFrames[REF_GOLD];

            m_pRecRefFrames[REF_ALT] = (pPrevTask->m_sFrameParams.bAltRef) ? 
                pPrevTask->m_pRecFrame : pPrevTask->m_pRecRefFrames[REF_ALT];

        }
        else
        {
            m_pRecRefFrames[REF_BASE] = pRecFrame;
            m_pRecRefFrames[REF_GOLD] = pRecFrame;
            m_pRecRefFrames[REF_ALT]  = pRecFrame;                
        }

        MFX_CHECK_STS(LockSurface(m_pRecFrame,m_pCore));
        MFX_CHECK_STS(LockSurface(m_pRawLocalFrame,m_pCore));
        MFX_CHECK_STS(LockSurface(m_pRecRefFrames[REF_BASE],m_pCore));
        MFX_CHECK_STS(LockSurface(m_pRecRefFrames[REF_GOLD],m_pCore));
        MFX_CHECK_STS(LockSurface(m_pRecRefFrames[REF_ALT],m_pCore));

        m_status = TASK_SUBMITTED;

        return MFX_ERR_NONE;        
    }
    mfxStatus Task::FreeTask()
    {
        MFX_CHECK_STS(FreeSurface(m_pRawFrame,m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRawLocalFrame,m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecFrame,m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_BASE],m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_GOLD],m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_ALT] ,m_pCore));

        m_pBitsteam     = 0;
        Zero(m_sFrameParams);
        m_status = TASK_FREE;

        return MFX_ERR_NONE; 
    }
}
#endif