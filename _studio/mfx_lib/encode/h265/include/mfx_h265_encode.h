// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "memory"
#include <condition_variable>
#include "deque"

#include "ippdefs.h"

#include "umc_thread.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_ext_buffers.h"

#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_encode_api.h"

#ifdef AMT_HROI_PSY_AQ
#include "FSapi.h"
#endif

class CmDevice;

namespace H265Enc {

    class H265FrameEncoder;
    class H265BRC;
    class Lookahead;
    class CmCopy;
    struct H265EncodeTaskInputParams;

    class Hrd
    {
    public:
        Hrd() { Zero(*this); }
        void Init(const H265SeqParameterSet &sps, Ipp32u initialDelay);
        void Update(Ipp32u sizeInbits, const Frame &pic);
        Ipp32u GetInitCpbRemovalDelay(const Frame &pic);
        Ipp32u GetInitCpbRemovalDelay() const;
        Ipp32u GetInitCpbRemovalOffset() const;
        Ipp32u GetAuCpbRemovalDelayMinus1(const Frame &pic) const;
        
    protected:
        Ipp8u  cbrFlag;
        Ipp32u bitrate;
        Ipp32u maxCpbRemovalDelay;
        double clockTick;
        double cpbSize90k;
        double initCpbRemovalDelay;

        Ipp32s prevAuCpbRemovalDelayMinus1;
        Ipp32u prevAuCpbRemovalDelayMsb;
        double prevAuFinalArrivalTime;
        double prevBuffPeriodAuNominalRemovalTime;
        Ipp32u prevBuffPeriodEncOrder;
        Ipp32s bpResetFlag;
    };

    class H265Encoder : public NonCopyable {
    public:
        explicit H265Encoder(MFXCoreInterface1 &core);
        ~H265Encoder();

        mfxStatus Init(const mfxVideoParam &par);
        mfxStatus Reset(const mfxVideoParam &par);
        mfxStatus ResetBRC(mfxVideoParam &par);

        const Ipp8u *GetVps(Ipp16u &size) const { assert(m_vpsBufSize<=0xffff); size = (Ipp16u)m_vpsBufSize; return m_vpsBuf; }
        const Ipp8u *GetSps(Ipp16u &size) const { assert(m_spsBufSize<=0xffff); size = (Ipp16u)m_spsBufSize; return m_spsBuf; }
        const Ipp8u *GetPps(Ipp16u &size) const { assert(m_ppsBufSize<=0xffff); size = (Ipp16u)m_ppsBufSize; return m_ppsBuf; }
        
        void GetEncodeStat(mfxEncodeStat &stat);

        mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint);

        void Close();
       
    protected:
        // ------ mfx level
        MFXCoreInterface1 &m_core;
        H265Enc::H265VideoParam m_videoParam;
        Hrd m_hrd;

        mfxEncodeStat m_stat;
        MfxMutex m_statMutex;

        // frame encoder
        std::vector<H265FrameEncoder*> m_frameEncoder;

        mfxU32  m_frameCountSync; // counter for sync. part
        mfxU32  m_frameCountBufferedSync;

        bool    m_useSysOpaq;
        bool    m_useVideoOpaq;
        bool    m_isOpaque;
    
        H265ProfileLevelSet m_profile_level;
        H265VidParameterSet m_vps;
        H265SeqParameterSet m_sps;
        H265PicParameterSet m_pps;
        ActiveParameterSets m_seiAps;
        Ipp8u m_vpsBuf[1024];
        Ipp8u m_spsBuf[1024];
        Ipp8u m_ppsBuf[1024];
        Ipp32u m_vpsBufSize;
        Ipp32u m_spsBufSize;
        Ipp32u m_ppsBufSize;

        // input frame control counters
        Ipp32s m_profileIndex;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;               // frame order of last IDR frame (in display order)
        Ipp32s m_frameOrderOfLastIntra;             // frame order of last I-frame (in display order)
        Ipp32s m_frameOrderOfLastIntraInEncOrder;   // frame order of last I-frame (in encoding order)
        Ipp32s m_frameOrderOfLastAnchor;            // frame order of last anchor (first in minigop) frame (in display order)
        Ipp32s m_frameOrderOfLastIdrB;              // (is used for encoded order) frame order of last IDR frame (in display order)
        Ipp32s m_frameOrderOfLastIntraB;            // (is used for encoded order) frame order of last I-frame (in display order)
        Ipp32s m_frameOrderOfLastAnchorB;           // (is used for encoded order)frame order of last anchor (first in minigop) frame (in display order)
        Ipp32s m_LastbiFramesInMiniGop;             // (is used for encoded order)
        Ipp32s m_miniGopCount;
        mfxU64 m_lastTimeStamp;
        Ipp32s m_lastEncOrder;
        Ipp32s m_sceneOrder;                        // in display order each frame belongs something scene (if sceneCut enabled)

        // AMT LTR
        Ipp32u m_currLtrStatus;

        // threading
        volatile Ipp32u   m_doStage;
        volatile Ipp32u   m_threadCount;
        volatile Ipp32u   m_reencode;     // BRC repack
        volatile Ipp32u   m_taskSubmitCount;
        volatile Ipp32u   m_taskEncodeCount;

        std::condition_variable m_condVar;
        std::mutex m_critSect;
        std::mutex m_prepCritSect;
        std::deque<ThreadingTask *> m_pendingTasks;
        std::deque<H265EncodeTaskInputParams *> m_inputTasks;
        H265EncodeTaskInputParams *m_inputTaskInProgress;
        volatile Ipp32u m_threadingTaskRunning;
        std::vector<Ipp32u> m_ithreadPool;
    
        // frame flow-control queues
        std::list<Frame *> m_free;            // _global_ free frame pool
        std::list<Frame *> m_inputQueue;      // _global_ input frame queue in _display_ order
        std::list<Frame *> m_lookaheadQueue;  // _global_ input frame queue in _display_ order
        std::list<Frame *> m_reorderedQueue;  // _global_ input frame queue in _encode_ order (ref list is _invalid_)
        std::list<Frame *> m_encodeQueue;     // _global_ queue in _encode_ order (ref list _valid_)
        std::list<Frame *> m_outputQueue;     // _global_ queue in _encode_ order ( submitted to encoding)

        std::list<Frame *> m_dpb;             // _global_ pool of frames: encoded reference frames + being encoded frames
        std::list<Frame *> m_actualDpb;       // reference frames for next frame to encode
        Frame *m_laFrame[2];

        ObjectPool<FrameData>     m_inputFrameDataPool;     // storage for full-sized original pixel data
        ObjectPool<FrameData>     m_reconFrameDataPool;     // storage for full-sized reconstructed/reference pixel data
        ObjectPool<FrameData>     m_frameDataLowresPool;    // storage for lowres original pixel data for lookahead
        ObjectPool<Statistics>    m_statsPool;              // storage for full-sized statistics per frame
        ObjectPool<Statistics>    m_statsLowresPool;        // storage for lowres statistics per frame
        ObjectPool<SceneStats>    m_sceneStatsPool;         // storage for scenecut statistics per frame
        ObjectPool<FeiInputData>  m_feiInputDataPool;       // storage for full-sized original pixel data
        ObjectPool<FeiReconData>  m_feiReconDataPool;       // storage for full-sized reconstructed/reference pixel data
        ObjectPool<FeiOutData>    m_feiAngModesPool[4];     // storage for angular intra modes output by fei (4x4, 8x8, 16x16, 32x32)
        ObjectPool<FeiOutData>    m_feiInterDataPool[4];    // storage for ME motion vectors and distortions output by fei (8x8, 16x16, 32x32, 64x64)
        ObjectPool<FeiBirefData>  m_feiBirefDataPool[4];    // storage for bi-refine data (mv,sad) output by fei (8x8, 16x16, 32x32, 64x64)
        ObjectPool<FeiBufferUp>   m_feiCuDataPool;          // storage for CU data shared between CPU and GPU (1 buffer/frame)
        ObjectPool<FeiBufferUp>   m_feiSaoModesPool;        // storage for sao modes chosen by GPU (1 buffer/frame)
        ObjectPool<ThreadingTask> m_ttHubPool;              // storage for threading tasks of type TT_HUB

        std::vector<ThreadingTask *> m_ttRootTasks;

        Ipp8u* m_memBuf;
        void *m_cu;
        Ipp16u *m_tile_ids;
        Ipp16u *m_tile_row_ids;
        Ipp16u *m_tile_col_ids;
        Segment *m_tiles;
        Ipp16u *m_slice_ids;
        Segment *m_slices;
        // perCU (num_threads_structs)
        H265BsFake *m_bsf;
        H265CUData *data_temp;

        BrcIface *m_brc;
        std::auto_ptr<Lookahead> m_la;

#ifdef AMT_HROI_PSY_AQ
        FSP m_faceSkinDet;
#endif

        static Ipp32u VM_THREAD_CALLCONVENTION FeiThreadRoutineStarter(void *p);
        void FeiThreadRoutine();
        void FeiThreadSubmit(ThreadingTask &task);
        bool FeiThreadWait(Ipp32u timeout);
        void *m_fei;
        UMC::Thread m_feiThread;
        Ipp32s m_pauseFeiThread;
        Ipp32s m_stopFeiThread;
        volatile Ipp32s m_feiThreadRunning;
        std::deque<ThreadingTask *> m_feiSubmitTasks;
        std::deque<ThreadingTask *> m_feiWaitTasks;
        std::condition_variable m_feiCondVar;
        std::mutex m_feiCritSect;

        mfxStatus InitInternal();

        CmDevice *m_cmDevice;
        mfxStatus CreateCmDevice();
        mfxStatus DestroyCmDevice();

        // ------SPS, PPS
        void SetProfileLevel();
        void SetVPS();
        void SetSPS();
        void SetPPS();
        void SetSeiAps();
        void SetSlice(H265Slice *slice, Ipp32u curr_slice, Frame* currentFrame);

        // ------ _global_ stages of Input Frame Control
        Frame *AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS, Ipp32s fieldNum);

        // new (incoming) frame
        void InitNewFrame(Frame *out, mfxFrameSurface1 *in);
        void PerformPadRecon(Frame *frame);

        friend class H265Enc::Lookahead;
        friend class H265Enc::H265FrameEncoder;
        void ConfigureInputFrame(Frame *frame, bool bEncOrder, bool bEncCtrl = false) const;
        void UpdateGopCounters(Frame *frame, bool bEncOrder);
        void RestoreGopCountersFromFrame(Frame *frame, bool bEncOrder);

        void ConfigureEncodeFrame(Frame *frame);
        void OnEncodingQueried(Frame *encoded);
    
        // ------ Ref List Managment
        void CreateRefPicList(Frame *frame, H265ShortTermRefPicSet *rps);
        void InitShortTermRefPicSet();
        void UpdateDpb(Frame *currTask);
        void CleanGlobalDpb();

        // AMT LTR
        void InitLongTermRefPicSet();

        // ------ threading
        static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
        static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);
        static void ProcessTaskDependencies(void *pState, ThreadingTask *task, ThreadingTask **taskNext, Ipp32s& distBest, int TaskFramePriority);

        void PrepareToEncodeNewTask(H265EncodeTaskInputParams *inputParam); // accept frame and find frame for lookahead
        void PrepareToEncode(H265EncodeTaskInputParams *inputParam); // build dependency graph for all parallel frames
        void EnqueueFrameEncoder(H265EncodeTaskInputParams *inputParam); // build dependency graph for one frames
        mfxStatus SyncOnFrameCompletion(H265EncodeTaskInputParams *inputParam, Frame *frame);

        void OnLookaheadStarting(); // no threas safety. some preparation work in single thread mode!
        void OnLookaheadCompletion(); // no threas safety. some post work in single thread mode!
    };

    template <class PixType> void CopyAndPad(const mfxFrameSurface1 &src, FrameData &dst, Ipp32u fourcc);

};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE