/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp8_encode_utils_hw.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE_HW)
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

        mfxExtVP8CodingOption *   pExtVP8OptDst  = (mfxExtVP8CodingOption*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VP8_CODING_OPTION); 
        mfxExtOpaqueSurfaceAlloc* opaqDst = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION); 

        par->mfx        = parSrc->mfx;
        par->mfx.BufferSizeInKB *= 3; // hybrid driver BRC can't fit HRD buffer sometimes - let's provide more space
        par->Protected  = parSrc->Protected;
        par->IOPattern  = parSrc->IOPattern;
        par->AsyncDepth = parSrc->AsyncDepth;

        if (pExtVP8OptDst) 
        {
            mfxExtVP8CodingOption * pExtVP8OptSrc = GetExtBuffer(*parSrc);
            if (pExtVP8OptSrc)
            {
                *pExtVP8OptDst = *pExtVP8OptSrc;
            }
            else
            {
                mfxExtBuffer header = pExtVP8OptDst->Header;
                memset(pExtVP8OptDst,0,sizeof(mfxExtVP8CodingOption));
                pExtVP8OptDst->Header = header;
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
                memset(opaqDst,0,sizeof(mfxExtOpaqueSurfaceAlloc));
                opaqDst->Header = header;
            }     
        }

        return MFX_ERR_NONE;

    }

    mfxU32 ModifyLoopFilterLevelQPBased(mfxU32 QP, mfxU32 loopFilterLevel, mfxU32 frameType)
    {
        if (loopFilterLevel)
            return loopFilterLevel;
        mfxU16 QPThr[7] = {15, 30, 45, 60, 75, 90, 105};
        mfxI16 DeltaLoopFilterLevelIntra[8] = {30, 31, 31, 32, 32, 33, 33, 34};
        mfxI16 DeltaLoopFilterLevelInter[8] = {26, 28, 30, 31, 32, 32, 32, 33}; 

        mfxU8 idx = 0;
        for (idx = 0; idx < 7; idx++)
        {
            if (QP <= QPThr[idx])
                break;
        } 
        mfxI32 loopFilterValue = ((QP * (frameType ? DeltaLoopFilterLevelInter[idx] : DeltaLoopFilterLevelIntra[idx]))>>6);
        return loopFilterValue > 63 ? 63 : loopFilterValue;
    }

    mfxStatus SetFramesParams(mfxVideoParam * par, mfxU16 forcedFrameType, mfxU32 frameOrder, sFrameParams *pFrameParams)
    {
        memset(pFrameParams, 0, sizeof(sFrameParams));
        pFrameParams->bIntra  = (frameOrder % par->mfx.GopPicSize) == 0 || (forcedFrameType & MFX_FRAMETYPE_I) ? true: false;
        if (pFrameParams->bIntra)
        {
            pFrameParams->bLastRef      = 1; // refresh last_ref with current frame
            pFrameParams->copyToGoldRef = 3; // refresh alt_ref with current frame
            pFrameParams->copyToAltRef  = 3; // refresh gold_ref with current frame
        }
        else
        {
            pFrameParams->bLastRef      = 1; // refresh last_ref with current frame
            pFrameParams->copyToGoldRef = 1; // copy every last_ref frame to gold_ref
            pFrameParams->copyToAltRef  = 2; // copy every gold_ref frame to alt_ref
        }
        mfxExtVP8CodingOption *pExtVP8Opt = GetExtBuffer(*par);
        pFrameParams->LFType  = pExtVP8Opt->LoopFilterType;
        for (mfxU8 i = 0; i < 4; i ++)
        {
            pFrameParams->LFLevel[i] = (mfxU8)ModifyLoopFilterLevelQPBased(
                pFrameParams->bIntra ? par->mfx.QPI : par->mfx.QPP,
                pExtVP8Opt->LoopFilterLevel[i],
                !pFrameParams->bIntra);
        }
        pFrameParams->Sharpness = pExtVP8Opt->SharpnessLevel;

        return MFX_ERR_NONE;  
    }

    //---------------------------------------------------------
    // service class: MfxFrameAllocResponse
    //---------------------------------------------------------

    MfxFrameAllocResponse::MfxFrameAllocResponse()
    {
        Zero(*this);
    }

    MfxFrameAllocResponse::~MfxFrameAllocResponse()
    {
        if (m_pCore == 0)
            return;

        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            m_pCore->FrameAllocator.Free(m_pCore->FrameAllocator.pthis, this);
        }
    } 


    mfxStatus MfxFrameAllocResponse::Alloc(
        mfxCoreInterface *     pCore,
        mfxFrameAllocRequest & req)
    {
        req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames
        mfxStatus sts = pCore->FrameAllocator.Alloc(pCore->FrameAllocator.pthis, &req, this);
        MFX_CHECK_STS(sts);

        if (NumFrameActual < req.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        m_pCore = pCore;
        m_numFrameActualReturnedByAllocFrames = NumFrameActual;
        NumFrameActual = req.NumFrameMin;
        m_info = req.Info;

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

        InitExtBufHeader(m_extOpaque);
        InitExtBufHeader(m_extVP8Opt);
        InitExtBufHeader(m_extROI);

        if (mfxExtOpaqueSurfaceAlloc * opts = GetExtBuffer(par))
            m_extOpaque = *opts;

        if (mfxExtVP8CodingOption * opts = GetExtBuffer(par))
            m_extVP8Opt = *opts;

        if (mfxExtEncoderROI * opts = GetExtBuffer(par))
            m_extROI = *opts;

        if (m_extVP8Opt.EnableMultipleSegments == MFX_CODINGOPTION_UNKNOWN && m_extROI.NumROI)
            m_extVP8Opt.EnableMultipleSegments = MFX_CODINGOPTION_ON;

        m_extParam[0]  = &m_extOpaque.Header;
        m_extParam[1]  = &m_extVP8Opt.Header;
        m_extParam[2]  = &m_extROI.Header;

        ExtParam = m_extParam;
        NumExtParam = mfxU16(sizeof m_extParam / sizeof m_extParam[0]);
        assert(NumExtParam == 3);
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

    mfxStatus InternalFrames::Init(mfxCoreInterface *pCore, mfxFrameAllocRequest *pAllocReq, bool bHW)
    {
        mfxStatus sts = MFX_ERR_NONE;
        MFX_CHECK_NULL_PTR2 (pCore, pAllocReq);
        mfxU32 nFrames = pAllocReq->NumFrameMin;

        if (nFrames == 0) return sts;
        pAllocReq->Type = (mfxU16)(bHW ? MFX_MEMTYPE_D3D_INT: MFX_MEMTYPE_SYS_INT);

        //printf("internal frames init %d (request)\n", req.NumFrameSuggested);

        sts = m_response.Alloc(pCore,*pAllocReq);
        MFX_CHECK_STS(sts);

        //printf("internal frames init %d (%d) [%d](response)\n", m_response.NumFrameActual,Num(),nFrames);

        m_surfaces.resize(nFrames);
        Zero(m_surfaces);

        //printf("internal frames init 1 [%d](response)\n", Num());

        m_frames.resize(nFrames);
        Zero(m_frames);

        //printf("internal frames init 2 [%d](response)\n", Num());

        for (mfxU32 i = 0; i < nFrames; i++)
        {
            m_frames[i].idInPool  = i;
            m_surfaces[i].Data.MemId = m_response.mids[i]; 
            m_surfaces[i].Info = pAllocReq->Info;
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
            return MFX_ERR_NONE;
        }
        return MFX_WRN_DEVICE_BUSY;   
    }

    //---------------------------------------------------------
    // service class: Task
    //---------------------------------------------------------

    mfxStatus Task::GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal)
    {
        pSurface = m_pRawFrame->pSurface;
        bExternal = true;        
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
            mfxFrameSurface1 src={};
            mfxFrameSurface1 dst = *(m_pRawLocalFrame->pSurface);

            mfxFrameSurface1 * pInput = 0;
            bool bExternal = true;

            sts = GetOriginalSurface(pInput,  bExternal);
            MFX_CHECK_STS(sts);

            src.Data = pInput->Data;
            src.Info = pInput->Info;

            FrameLocker lockSrc(m_pCore, src.Data, bExternal);
            FrameLocker lockDst(m_pCore, dst.Data, false);

            MFX_CHECK(src.Info.FourCC == MFX_FOURCC_YV12 || src.Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(dst.Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);

            MFX_CHECK_NULL_PTR1(src.Data.Y);
            if (src.Info.FourCC == MFX_FOURCC_NV12)
            {
                MFX_CHECK_NULL_PTR1(src.Data.UV);
            }
            else
            {
                MFX_CHECK_NULL_PTR2(src.Data.U, src.Data.V);
            }

            MFX_CHECK_NULL_PTR2(dst.Data.Y, dst.Data.UV);

            MFX_CHECK(dst.Info.Width >= src.Info.Width, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(dst.Info.Height >= src.Info.Height, MFX_ERR_UNDEFINED_BEHAVIOR);

            mfxU32 srcPitch = src.Data.PitchLow + ((mfxU32)src.Data.PitchHigh << 16);
            mfxU32 dstPitch = dst.Data.PitchLow + ((mfxU32)dst.Data.PitchHigh << 16);

            mfxU32 roiWidth = src.Info.Width;
            mfxU32 roiHeight = src.Info.Height;

            // copy luma
            mfxU8 * srcLine = src.Data.Y;
            mfxU8 * dstLine = dst.Data.Y;
            for (mfxU32 line = 0; line < roiHeight; line ++)
            {
                memcpy(dstLine, srcLine, roiWidth);
                srcLine += srcPitch;
                dstLine += dstPitch;
            }

            // copy chroma (with color conversion if required)
            dstLine = dst.Data.UV;
            roiHeight >>= 1;
            if (src.Info.FourCC == MFX_FOURCC_NV12)
            {
                // for input NV12 just copy chroma
                srcLine = src.Data.UV;
                for (mfxU32 line = 0; line < roiHeight; line ++)
                {
                    memcpy(dstLine, srcLine, roiWidth);
                    srcLine += srcPitch;
                    dstLine += dstPitch;
                }
            }
            else
            {
                // for YV12 color conversion is required
                mfxU8 * srcU = src.Data.U;
                mfxU8 * srcV = src.Data.V;
                roiWidth >>= 1;
                srcPitch >>= 1;
                for (mfxU32 line = 0; line < roiHeight; line ++)
                {
                    for (mfxU32 pixel = 0; pixel < roiWidth; pixel ++)
                    {
                        mfxU32 dstUVPosition = pixel << 1;
                        dstLine[dstUVPosition] = srcU[pixel];
                        dstLine[dstUVPosition + 1] = srcV[pixel];
                    }
                    srcU += srcPitch;
                    srcV += srcPitch;
                    dstLine += dstPitch;
                }
            }
        }
        return sts;        
    }
    mfxStatus Task::Init(mfxCoreInterface * pCore, mfxVideoParam *par)
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

    mfxStatus Task::SubmitTask (sFrameEx*  pRecFrame, std::vector<sFrameEx*> &dpb, sFrameParams* pParams, sFrameEx* pRawLocalFrame)
    {
        MFX_CHECK(m_status == TASK_INITIALIZED, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_NULL_PTR2(pRecFrame, pParams);
        MFX_CHECK(isFreeSurface(pRecFrame),MFX_ERR_UNDEFINED_BEHAVIOR);

        //printf("Task::SubmitTask\n");

        m_sFrameParams   = *pParams;
        m_pRecFrame      = pRecFrame;
        m_pRawLocalFrame = pRawLocalFrame;

        MFX_CHECK_STS (CopyInput());

        m_pRecFrame->pSurface->Data.FrameOrder = m_frameOrder;

        if (!m_sFrameParams.bIntra)
        {
            m_pRecRefFrames[REF_BASE] = dpb[REF_BASE];
            m_pRecRefFrames[REF_GOLD] = dpb[REF_GOLD];
            m_pRecRefFrames[REF_ALT]  = dpb[REF_ALT];
        }

        MFX_CHECK_STS(LockSurface(m_pRecFrame,m_pCore));
        MFX_CHECK_STS(LockSurface(m_pRawLocalFrame,m_pCore));

        m_status = TASK_SUBMITTED;

        return MFX_ERR_NONE;        
    }
    mfxStatus Task::FreeTask()
    {
        //printf("Task::FreeTask\n");

        MFX_CHECK_STS(FreeSurface(m_pRawFrame,m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRawLocalFrame,m_pCore));

        m_pBitsteam     = 0;
        Zero(m_sFrameParams);
        Zero(m_ctrl);
        m_status = TASK_FREE;

        return MFX_ERR_NONE; 
    }

    // There is no DPB management in current VP8 hybrid implementation.
    // This function is used to free surfaces containing reference frames
    // related to task: frame reconstruct and all it's references
    mfxStatus Task::FreeDPBSurfaces()
    {
        MFX_CHECK_STS(FreeSurface(m_pRecFrame,m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_BASE],m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_GOLD],m_pCore));
        MFX_CHECK_STS(FreeSurface(m_pRecRefFrames[REF_ALT] ,m_pCore));

        return MFX_ERR_NONE; 
    }
}
#endif
