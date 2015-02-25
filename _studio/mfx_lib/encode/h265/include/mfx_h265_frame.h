//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include <list>
#include <vector>
#include "vm_interlocked.h"
#include "umc_mutex.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"

namespace H265Enc {

    struct H265VideoParam;

    struct H265MV
    {
        Ipp16s  mvx;
        Ipp16s  mvy;
    };

    class State
    {
    public:
        State()
            : m_free(true)
        {
        }

        bool IsFree() const
        {
            return m_free;
        }

        void SetFree(bool free)
        {
            m_free = free;
        }

    private:
        bool m_free; // resource in use or could be reused (m_free == true means could be reused).
    };

    struct H265CUData;
    class H265Frame
    {
    public:
        void *mem;
        H265CUData *cu_data;

        Ipp8u *y;
        Ipp8u *uv;

        Ipp32s width;
        Ipp32s height;
        Ipp32s padding;
        Ipp32s pitch_luma_pix;
        Ipp32s pitch_luma_bytes;
        Ipp32s pitch_chroma_pix;
        Ipp32s pitch_chroma_bytes;

        /* for 10 bit surfaces */
        Ipp8u *y_8bit;
        Ipp8u *uv_8bit;
        Ipp32s pitch_luma_bytes_8bit;
        Ipp32s pitch_chroma_bytes_8bit;

        Ipp8u  m_bitDepthLuma;
        Ipp8u  m_bdLumaFlag;
        Ipp8u  m_bitDepthChroma;
        Ipp8u  m_bdChromaFlag;
        Ipp8u  m_chromaFormatIdc;

        mfxU64 m_timeStamp;
        Ipp32u m_picCodeType;
        Ipp32s m_RPSIndex;

        // flags indicate that the stages were done on the frame
        Ipp8u  m_wasLookAheadProcessed;
        Ipp32u m_lookaheadRefCounter;

        Ipp32s m_pyramidLayer;
        Ipp32s m_miniGopCount;
        Ipp32s m_biFramesInMiniGop;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;
        Ipp32s m_frameOrderOfLastIntra;
        Ipp32s m_frameOrderOfLastIntraInEncOrder;
        Ipp32s m_frameOrderOfLastAnchor;
        Ipp32s m_poc;
        Ipp32s m_encOrder;
        Ipp8u  m_isShortTermRef;
        Ipp8u  m_isLongTermRef;
        Ipp8u  m_isIdrPic;
        Ipp8u  m_isRef;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        Ipp32s m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        Ipp32s m_mapListRefUnique[2][MAX_NUM_REF_IDX];
        Ipp32s m_numRefUnique;
        Ipp32s m_allRefFramesAreFromThePast;

        // for frame threading
        volatile Ipp32s m_codedRow; // sync info in case of frame threading
        volatile Ipp32u m_refCounter; // to prevent race condition in case of frame threading
        
        // complexity/content statistics
        std::vector<Ipp32s> m_interSad;
        std::vector<Ipp32s> m_interSad_pdist_past;
        std::vector<Ipp32s> m_interSad_pdist_future;

        std::vector<H265MV> m_mv;
        std::vector<H265MV> m_mv_pdist_future;
        std::vector<H265MV> m_mv_pdist_past;
        std::vector<Ipp64f> m_rs;
        std::vector<Ipp64f> m_cs;
        Ipp64f m_frameRs;
        Ipp64f m_frameCs;
        std::vector<Ipp32s> sc_mask;
        std::vector<Ipp32s> qp_mask;
        std::vector<Ipp32s> coloc_past;
        std::vector<Ipp32s> coloc_futr;

        Ipp64f SC;
        Ipp64f TSC;

        Ipp64f avgsqrSCpp;
        Ipp64f avgTSC;
        
        // BRC
        std::vector<Ipp32s> m_intraSatd;
        std::vector<Ipp32s> m_interSatd;
        Ipp32f              m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        Ipp32f              m_intraRatio;
        Ipp32s              m_sceneCut;
        Ipp64s              m_metric;// special metric per frame for SCD

        Ipp64f m_avCmplx;
        Ipp64f m_CmplxQstep;

        H265Frame* m_lowres;

        H265Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
            ResetCounters();
        }

        ~H265Frame() { Destroy(); mem = NULL;}

        Ipp8u wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }

        void Create(H265VideoParam *par, Ipp8u needExtData = 0);
        void Destroy();
        void CopyFrame(const mfxFrameSurface1 *surface, bool have8bitCopy = false);
        void doPadding();

        void ResetMemInfo();
        void ResetEncInfo();
        void ResetCounters();
        void CopyEncInfo(const H265Frame* src);

        void AddRef(void)  { vm_interlocked_inc32(&m_refCounter);};
        void Release(void) { vm_interlocked_dec32(&m_refCounter);}; // !!! here not to delete in case of refCounter == 0 !!!
    };


    struct Task
    {
        H265Frame* m_frameOrigin;
        H265Frame* m_frameRecon;

        Ipp32u     m_encOrder;
        Ipp32u     m_frameOrder;
        Ipp64u     m_timeStamp;
        Ipp32u     m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ?
        
        std::vector<H265Slice> m_slices;
        H265Frame* m_dpb[16];
        Ipp32s     m_dpbSize;

        Ipp8s     m_sliceQpY;
        std::vector<Ipp8s> m_lcuQps; // array for LCU QPs
        H265Slice m_dqpSlice[2*MAX_DQP+1];

        H265ShortTermRefPicSet m_shortRefPicSet[66];
        mfxBitstream *m_bs;

        // quick wa for GAAC
        void*      m_extParam;

        // for frame parallel
        volatile Ipp32u m_statusReport; // 0 - task submitted to FrameEncoder (not run!!), 1 - FrameEncoder was run (resource assigned), 2 - task ready, 7 - need repack
        Ipp32s m_encIdx; // we have "N" frameEncoders. this index indicates owner of the task [0, ..., N-1]

        // for threading control
        std::vector<Ipp32u> m_ithreadPool;

        // for BRC lookahead
        std::vector<H265Frame*> m_futureFrames;

        Task()
            : m_frameOrigin(NULL)
            , m_frameRecon(NULL)
            , m_encOrder(Ipp32u(-1))
            , m_frameOrder(Ipp32u(-1))
            , m_timeStamp(0)
        {
            m_bs       = NULL;
            m_extParam = NULL;
            m_statusReport = 0;
            m_encIdx= -1;
        }

        ~Task() { Destroy(); }

        void Reset()
        {
            m_frameOrigin = NULL;
            m_frameRecon  = NULL;
            m_bs          = NULL;
            m_encOrder    = Ipp32u(-1);
            m_frameOrder  = Ipp32u(-1);
            m_timeStamp   = 0;
            m_bs       = NULL;
            m_extParam = NULL;
            m_statusReport = 0;
            m_encIdx= -1;
            m_dpbSize     = 0;
        }

        void Create(Ipp32u numCtb, Ipp32u numThreadStructs, Ipp32s numSlices)
        {
            m_lcuQps.resize(numCtb);
            m_ithreadPool.resize(numThreadStructs, 0);
            m_slices.resize(numSlices);
        }

        void Destroy()
        {
            m_lcuQps.resize(0);
            m_ithreadPool.resize(0);
        }
    };

    typedef std::list<Task*> TaskList;
    typedef std::list<Task*>::iterator   TaskIter;

    typedef std::list<H265Frame*> FramePtrList;
    typedef std::list<H265Frame*>::iterator   FramePtrIter;


    FramePtrIter GetFreeFrame(FramePtrList & queue, H265VideoParam *par);
    void Dump(const vm_char* fname, H265VideoParam *par, H265Frame* frame, TaskList & dpb);
    void PadOneReconRow(H265Frame* frame, Ipp32u ctb_row, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs);

} // namespace

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
