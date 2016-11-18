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

#ifndef __SAMPLE_FEI_ENC_TASK_H__
#define __SAMPLE_FEI_ENC_TASK_H__

#include <mfxfei.h>
#include "sample_fei_defs.h"

// for ext buffers management
struct setElem
{
    mfxExtBuffer * getBufById(mfxU32 id, mfxU32 fieldId)
    {
        if (!buffers.empty())
        {
            for (mfxU16 i = 0; i < buffers.size() - fieldId; i++)
            {
                if (buffers[i]->BufferId == id)
                {
                    return (buffers[i + fieldId] && buffers[i + fieldId]->BufferId == id) ? buffers[i + fieldId] : NULL;
                }
            }
        }

        return NULL;
    }

    void ResetMBnum(mfxU32 new_numMB, mfxU16 increment)
    {
        for (mfxU16 i = 0; i < buffers.size(); i += increment)
        {
            switch (buffers[i]->BufferId)
            {
                /* Input buffers */
                case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
                {
                    mfxExtFeiPreEncMVPredictors* mvPreds = reinterpret_cast<mfxExtFeiPreEncMVPredictors*>(buffers[i]);
                    mvPreds->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MV_PRED:
                {
                    mfxExtFeiEncMVPredictors* pMvPredBuf = reinterpret_cast<mfxExtFeiEncMVPredictors*>(buffers[i]);
                    pMvPredBuf->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MB:
                {
                    mfxExtFeiEncMBCtrl* pMbEncCtrl = reinterpret_cast<mfxExtFeiEncMBCtrl*>(buffers[i]);
                    pMbEncCtrl->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_QP:
                {
                    mfxExtFeiEncQP* pMbQP = reinterpret_cast<mfxExtFeiEncQP*>(buffers[i]);
                    pMbQP->NumQPAlloc = new_numMB;
                }
                break;

                /* Output buffers */
                case MFX_EXTBUFF_FEI_PREENC_MV:
                {
                    mfxExtFeiPreEncMV* mvs = reinterpret_cast<mfxExtFeiPreEncMV*>(buffers[i]);
                    mvs->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_PREENC_MB:
                {
                    mfxExtFeiPreEncMBStat* mbdata = reinterpret_cast<mfxExtFeiPreEncMBStat*>(buffers[i]);
                    mbdata->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MV:
                {
                    mfxExtFeiEncMV* mvBuf = reinterpret_cast<mfxExtFeiEncMV*>(buffers[i]);
                    mvBuf->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MB_STAT:
                {
                    mfxExtFeiEncMBStat* mbstatBuf = reinterpret_cast<mfxExtFeiEncMBStat*>(buffers[i]);
                    mbstatBuf->NumMBAlloc = new_numMB;
                }
                break;

                case MFX_EXTBUFF_FEI_PAK_CTRL:
                {
                    mfxExtFeiPakMBCtrl* mbcodeBuf = reinterpret_cast<mfxExtFeiPakMBCtrl*>(buffers[i]);
                    mbcodeBuf->NumMBAlloc = new_numMB;
                }
                break;
            } // switch (ExtParam[i]->BufferId)
        } // for (mfxU16 i = 0; i < NumExtParam; i += increment)
    }

    void Destroy(mfxU16 num_of_fields)
    {
        for (mfxU16 i = 0; i < buffers.size(); /*i++*/)
        {
            switch (buffers[i]->BufferId)
            {
                case MFX_EXTBUFF_FEI_PREENC_CTRL:
                {
                    mfxExtFeiPreEncCtrl* preENCCtr = reinterpret_cast<mfxExtFeiPreEncCtrl*>(buffers[i]);
                    MSDK_SAFE_DELETE_ARRAY(preENCCtr);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_REPACK_CTRL:
                {
                    mfxExtFeiRepackCtrl* feiRepack = reinterpret_cast<mfxExtFeiRepackCtrl*>(buffers[i]);
                    MSDK_SAFE_DELETE_ARRAY(feiRepack);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
                {
                    mfxExtFeiPreEncMVPredictors* mvPreds = reinterpret_cast<mfxExtFeiPreEncMVPredictors*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(mvPreds[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(mvPreds);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_QP:
                {
                    mfxExtFeiEncQP* qps = reinterpret_cast<mfxExtFeiEncQP*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(qps[fieldId].QP);
                    }
                    MSDK_SAFE_DELETE_ARRAY(qps);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_CTRL:
                {
                    mfxExtFeiEncFrameCtrl* feiEncCtrl = reinterpret_cast<mfxExtFeiEncFrameCtrl*>(buffers[i]);
                    MSDK_SAFE_DELETE_ARRAY(feiEncCtrl);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_SPS:
                {
                    mfxExtFeiSPS* feiSPS = reinterpret_cast<mfxExtFeiSPS*>(buffers[i]);
                    MSDK_SAFE_DELETE(feiSPS);
                    i++;
                }
                break;

                case MFX_EXTBUFF_FEI_PPS:
                {
                    mfxExtFeiPPS* feiPPS = reinterpret_cast<mfxExtFeiPPS*>(buffers[i]);
                    MSDK_SAFE_DELETE_ARRAY(feiPPS);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_SLICE:
                {
                    mfxExtFeiSliceHeader* feiSliceHeader = reinterpret_cast<mfxExtFeiSliceHeader*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiSliceHeader[fieldId].Slice);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiSliceHeader);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MV_PRED:
                {
                    mfxExtFeiEncMVPredictors* feiEncMVPredictors = reinterpret_cast<mfxExtFeiEncMVPredictors*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MB:
                {
                    mfxExtFeiEncMBCtrl* feiEncMBCtrl = reinterpret_cast<mfxExtFeiEncMBCtrl*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_PREENC_MV:
                {
                    mfxExtFeiPreEncMV* mvs = reinterpret_cast<mfxExtFeiPreEncMV*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(mvs[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(mvs);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_PREENC_MB:
                {
                    mfxExtFeiPreEncMBStat* mbdata = reinterpret_cast<mfxExtFeiPreEncMBStat*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(mbdata[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(mbdata);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MB_STAT:
                {
                    mfxExtFeiEncMBStat* feiEncMbStat = reinterpret_cast<mfxExtFeiEncMBStat*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiEncMbStat[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiEncMbStat);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_ENC_MV:
                {
                    mfxExtFeiEncMV* feiEncMV = reinterpret_cast<mfxExtFeiEncMV*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiEncMV[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiEncMV);
                    i += num_of_fields;
                }
                break;

                case MFX_EXTBUFF_FEI_PAK_CTRL:
                {
                    mfxExtFeiPakMBCtrl* feiEncMBCode = reinterpret_cast<mfxExtFeiPakMBCtrl*>(buffers[i]);
                    for (mfxU32 fieldId = 0; fieldId < num_of_fields; fieldId++){
                        MSDK_SAFE_DELETE_ARRAY(feiEncMBCode[fieldId].MB);
                    }
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCode);
                    i += num_of_fields;
                }
                break;

                default:
                    ++i;
                    break;
            } // switch ((*it)->PB_bufs.in.ExtParam[i]->BufferId)
        } // for (int i = 0; i < (*it)->PB_bufs.in.NumExtParam; )

        buffers.clear();
    }

    void Release()
    {
        buffers.clear();
    }

    mfxU16 NumExtParam()
    {
        return mfxU16(buffers.size());
    }

    mfxExtBuffer **ExtParam()
    {
        return buffers.empty()? NULL : &buffers[0];
    }

    void Add(mfxExtBuffer* buf)
    {
        buffers.push_back(buf);
    }

    std::vector<mfxExtBuffer *> buffers;
};

struct IObuffs
{
    setElem in;
    setElem out;

    void Destroy(mfxU16 n_fields)
    {
        in.Destroy(n_fields);
        out.Destroy(n_fields);
    }

    void Release()
    {
        in.Release();
        out.Release();
    }

    void ResetMBnum(mfxU32 new_numMB, mfxU16 increment)
    {
        in.ResetMBnum(new_numMB, increment);
        out.ResetMBnum(new_numMB, increment);
    }
};

/* This structure holds sets of input and output extended buffers
   required for frame processing by one of the FEI interfaces */

struct bufSet
{
    bool    vacant;
    mfxU16  num_fields;
    IObuffs I_bufs;
    IObuffs PB_bufs;

    bufSet(mfxU16 n_fields = 1)
        : vacant(true)
        , num_fields(n_fields)
    {}

    ~bufSet() { Destroy(); }

    void Destroy()
    {
        vacant = false;
        PB_bufs.Destroy(num_fields);
        I_bufs.Release();
    }

    void ResetMBnum(mfxU32 new_numMB, bool both_fields)
    {
        PB_bufs.ResetMBnum(new_numMB, both_fields ? 1 : num_fields);
    }
};

struct bufList
{
    std::list<bufSet*> buf_list;

    mfxU16 num_of_fields;

    bufList(mfxU16 n_fields = 1)
        : num_of_fields(n_fields)
    {}

    ~bufList(){ Clear(); }

    void AddSet(bufSet* set) { buf_list.push_back(set); }

    void Clear()
    {
        for (std::list<bufSet*>::iterator it = buf_list.begin(); it != buf_list.end(); ++it)
        {
            MSDK_SAFE_DELETE(*it);
        }

        buf_list.clear();
    }

    bufSet* GetFreeSet()
    {
        for (std::list<bufSet*>::iterator it = buf_list.begin(); it != buf_list.end(); ++it){
            if ((*it)->vacant)
            {
                (*it)->vacant = false;
                return (*it);
            }
        }
        return NULL;
    }
};

struct PreEncOutput
{
    PreEncOutput()
        : output_bufs(NULL)
    {
        refIdx[0][0] = refIdx[0][1] = refIdx[1][0] = refIdx[1][1] = 0;
    }

    PreEncOutput(bufSet* bufs, mfxU8 idx[2][2])
        : output_bufs(bufs)
    {
        refIdx[0][0] = idx[0][0];
        refIdx[0][1] = idx[0][1];
        refIdx[1][0] = idx[1][0];
        refIdx[1][1] = idx[1][1];
    }

    bufSet* output_bufs;
    mfxU8 refIdx[2][2]; // [fieldId][L0L1]
};

struct iTaskParams
{
    mfxU16 PicStruct;
    mfxU16 BRefType;
    PairU8 FrameType;
    mfxU16 GopPicSize;
    mfxU16 GopRefDist;
    mfxU32 FrameCount;
    mfxU32 FrameOrderIdrInDisplayOrder;
    mfxU16 NumRefActiveP;
    mfxU16 NumRefActiveBL0;
    mfxU16 NumRefActiveBL1;

    mfxFrameSurface1 *InputSurf;
    mfxFrameSurface1 *ReconSurf;
    mfxFrameSurface1 *DSsurface;

    explicit iTaskParams()
        : PicStruct(MFX_PICSTRUCT_PROGRESSIVE)
        , BRefType(MFX_B_REF_OFF)
        , FrameType(PairU8(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF))
        , GopPicSize(1)
        , GopRefDist(1)
        , FrameCount(0)
        , FrameOrderIdrInDisplayOrder(0)
        , NumRefActiveP(0)
        , NumRefActiveBL0(0)
        , NumRefActiveBL1(0)
        , InputSurf(NULL)
        , ReconSurf(NULL)
        , DSsurface(NULL)
    {}
};

//for PreEnc; Enc; Pak reordering
struct iTask
{
    explicit iTask(const iTaskParams & task_params)
        : encoded(false)
        , bufs(NULL)
        , preenc_bufs(NULL)
        , fullResSurface(NULL)
        , PicStruct(task_params.PicStruct)
        , BRefType(task_params.BRefType)
        , NumRefActiveP(task_params.NumRefActiveP)
        , NumRefActiveBL0(task_params.NumRefActiveBL0)
        , NumRefActiveBL1(task_params.NumRefActiveBL1)
        , m_type(task_params.FrameType)
        , m_fid(PairU8(static_cast<mfxU8>(!!(task_params.PicStruct & MFX_PICSTRUCT_FIELD_BFF)),
                       static_cast<mfxU8>(!(task_params.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) - !!(task_params.PicStruct & MFX_PICSTRUCT_FIELD_BFF))))
        , m_fieldPicFlag(!(task_params.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))
        , m_frameOrderIdr(task_params.FrameOrderIdrInDisplayOrder)
        , m_frameOrderI(0)
        , m_frameOrder(task_params.FrameCount)
        , GopPicSize(task_params.GopPicSize)
        , GopRefDist(task_params.GopRefDist)
        , m_viewIdx(0)
        //, m_nalRefIdc(PairU8(0,0))
        , m_picNum(PairI32(0, 0))
        , m_frameNum(0)
        , m_frameNumWrap(0)
        , m_tid(0)
        , m_tidx(0)
        , m_longTermPicNum(PairU8(0, 0))
        , prevTask(NULL)
    {
        m_list0[0].Fill(0);
        m_list0[1].Fill(0);
        m_list1[0].Fill(0);
        m_list1[1].Fill(0);

        m_initSizeList0[0] = 0;
        m_initSizeList0[1] = 0;
        m_initSizeList1[0] = 0;
        m_initSizeList1[1] = 0;

        memset(&in,     0, sizeof(in));
        memset(&out,    0, sizeof(out));
        memset(&inPAK,  0, sizeof(inPAK));
        memset(&outPAK, 0, sizeof(outPAK));

        /* Below initialized structures required for frames reordering */
        if (m_type[m_fid[0]] & MFX_FRAMETYPE_B)
        {
            m_loc = GetBiFrameLocation(m_frameOrder - m_frameOrderIdr);
            m_type[0] |= m_loc.refFrameFlag;
            m_type[1] |= m_loc.refFrameFlag;
        }

        m_nalRefIdc[m_fid[0]] = m_reference[m_fid[0]] = !!(m_type[m_fid[0]] & MFX_FRAMETYPE_REF);
        m_nalRefIdc[m_fid[1]] = m_reference[m_fid[1]] = !!(m_type[m_fid[1]] & MFX_FRAMETYPE_REF);

        m_poc[0] = 2 * ((m_frameOrder - m_frameOrderIdr) & 0x7fffffff) + (TFIELD != m_fid[0]);
        m_poc[1] = 2 * ((m_frameOrder - m_frameOrderIdr) & 0x7fffffff) + (BFIELD != m_fid[0]);


        /* Fill information required for encoding */
        if (task_params.InputSurf)
        {
            in.InSurface = task_params.InputSurf;
            in.InSurface->Data.Locked++;
        }

        /* ENC and/or PAK */
        if (task_params.ReconSurf)
        {
            inPAK.InSurface = in.InSurface;
            inPAK.InSurface->Data.Locked++;

            /* ENC & PAK share same reconstructed/reference surface */
            out.OutSurface = task_params.ReconSurf;
            out.OutSurface->Data.Locked++;
            outPAK.OutSurface = task_params.ReconSurf;
            outPAK.OutSurface->Data.Locked++;
        }

        /* PreENC on downsampled surface */
        if (task_params.DSsurface)
        {
            // PREENC needs to be performed on downscaled surface
            // For simplicity, let's just replace the original surface
            fullResSurface = in.InSurface;

            in.InSurface = task_params.DSsurface;
            in.InSurface->Data.Locked++;

            // make sure picture structure has the initial value
            // surfaces are reused and VPP may change this parameter in certain configurations
            in.InSurface->Info.PicStruct = fullResSurface->Info.PicStruct & 0xf;
        }
    }

    ~iTask()
    {
        SAFE_RELEASE_EXT_BUFSET(bufs);
        SAFE_RELEASE_EXT_BUFSET(preenc_bufs);

        ReleasePreEncOutput();

        SAFE_DEC_LOCKER(in.InSurface);
        /* In case of preenc + enc (+ pak) pipeline we need to do decrement manually for both interfaces*/
        SAFE_DEC_LOCKER(inPAK.InSurface);
        SAFE_DEC_LOCKER(fullResSurface);
        SAFE_DEC_LOCKER(out.OutSurface);
        SAFE_DEC_LOCKER(outPAK.OutSurface);
    }

    /* This operator is used to store only necessary information from previous encoding */
    iTask& operator= (const iTask& task)
    {
        if (this == &task)
            return *this;

        m_frameOrderIdr   = task.m_frameOrderIdr;
        m_frameOrderI     = task.m_frameOrderI;
        m_frameIdrCounter = task.m_frameIdrCounter;
        m_nalRefIdc       = task.m_nalRefIdc;
        m_frameNum        = task.m_frameNum;
        m_type            = task.m_type;
        m_dpbPostEncoding = task.m_dpbPostEncoding;
        m_poc             = task.m_poc;
        PicStruct         = task.PicStruct;

        return *this;
    }

    /* Release all output buffers from PreENC multicalls.
       This performed after repacking / output dumping is finished */
    void ReleasePreEncOutput()
    {
        for (std::list<PreEncOutput>::iterator it = preenc_output.begin(); it != preenc_output.end(); ++it)
        {
            SAFE_RELEASE_EXT_BUFSET((*it).output_bufs);
        }

        preenc_output.clear();
    }

    /* These two functions are used to get location of B frame in current mini-GOP.
       Output depends from B-pyramid settings */
    BiFrameLocation GetBiFrameLocation(mfxU32 frameOrder)
    {

        mfxU32 gopPicSize = GopPicSize;
        mfxU32 gopRefDist = GopRefDist;
        mfxU32 biPyramid  = BRefType;

        BiFrameLocation loc;

        if (gopPicSize == 0xffff) //infinite GOP
            gopPicSize = 0xffffffff;

        if (biPyramid != MFX_B_REF_OFF)
        {
            bool ref = false;
            mfxU32 orderInMiniGop = frameOrder % gopPicSize % gopRefDist - 1;

            loc.encodingOrder = GetEncodingOrder(orderInMiniGop, 0, gopRefDist - 1, 0, ref);
            loc.miniGopCount = frameOrder % gopPicSize / gopRefDist;
            loc.refFrameFlag = static_cast<mfxU16>(ref ? MFX_FRAMETYPE_REF : 0);
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

    /* These functions counts forward / backward references of current task */
    mfxU32 GetNBackward(mfxU32 fieldId)
    {
        mfxU32 fid = m_fid[fieldId];

        if (m_list0[fid].Size() == 0)
            return 0;

        if (m_list1[fid].Size() == 0 ||
            std::find(m_list0[fid].Begin(), m_list0[fid].End(), *m_list1[fid].Begin())
            == m_list0[fid].End())
        {
            // No forward ref in L0
            return m_list0[fid].Size();
        }
        else
        {
            return static_cast<mfxU32>(std::distance(m_list0[fid].Begin(),
                std::find(m_list0[fid].Begin(), m_list0[fid].End(), *m_list1[fid].Begin())));
        }
    }

    mfxU32 GetNForward(mfxU32 fieldId)
    {
        mfxU32 fid = m_fid[fieldId];

        if (m_list1[fid].Size() == 0)
            return 0;

        if (std::find(m_list1[fid].Begin(), m_list1[fid].End(), *m_list0[fid].Begin())
            == m_list1[fid].End())
        {
            // No backward ref in L1
            return m_list1[fid].Size();
        }
        else
        {
            return static_cast<mfxU32>(std::distance(m_list1[fid].Begin(),
                std::find(m_list1[fid].Begin(), m_list1[fid].End(), *m_list0[fid].Begin())));
        }
    }

    mfxENCInput  in;
    mfxENCOutput out;
    mfxPAKInput  inPAK;
    mfxPAKOutput outPAK;
    BiFrameLocation m_loc;
    bool encoded;
    bufSet* bufs;
    bufSet* preenc_bufs;
    std::list<PreEncOutput> preenc_output;
    mfxFrameSurface1 *fullResSurface;

    mfxU16 PicStruct;
    mfxU16 BRefType;

    mfxU16 NumRefActiveP;   // limits of active
    mfxU16 NumRefActiveBL0; // references for
    mfxU16 NumRefActiveBL1; // reflists management

    //..............................reflist control............................................
    ArrayDpbFrame   m_dpb[2];          // DPB state before encoding first and second fields
    ArrayDpbFrame   m_dpbPostEncoding; // DPB after encoding a frame (or 2 fields)
    ArrayU8x33      m_list0[2];        // L0 list for first and second field
    ArrayU8x33      m_list1[2];        // L1 list for first and second field
    PairU8          m_type;            // type of first and second field
    PairU8          m_fid;             // progressive fid=[0,0]; tff fid=[0,1]; bff fid=[1,0]
    bool            m_fieldPicFlag;    // is interlaced frame
    PairI32         m_poc;             // POC of first and second field

    mfxU32  m_frameOrderIdr;           // most recent idr frame in display order
    mfxU32  m_frameOrderI;             // most recent i frame in display order
    mfxU32  m_frameOrder;              // current frame order in display order
    mfxU16  m_frameIdrCounter;         // number of IDR frames encoded

    mfxU16  GopPicSize;                // GOP size
    mfxU16  GopRefDist;                // number of B frames in mini-GOP + 1

    ArrayRefListMod m_refPicList0Mod[2];
    ArrayRefListMod m_refPicList1Mod[2];
    mfxU32  m_initSizeList0[2];
    mfxU32  m_initSizeList1[2];

    DecRefPicMarkingInfo m_decRefPicMrk[2];    // dec_ref_pic_marking() for current frame

    mfxU32  m_viewIdx;

    PairU8  m_nalRefIdc;

    // from Reconstruct
    PairI32 m_picNum;

    mfxU16  m_frameNum;
    mfxI32  m_frameNumWrap;

    mfxU32  m_tid;              // temporal_id
    mfxU32  m_tidx;             // temporal layer index (in acsending order of temporal_id)

    PairU8  m_longTermPicNum;
    PairU8  m_reference;        // is refrence (short or long term) or not
    //.........................................................................................

    iTask* prevTask;
};

/* This structure represents state of DPB and reference lists of the task being processed */
struct RefInfo
{
    std::vector<mfxFrameSurface1*> reference_frames;
    struct{
        std::vector<mfxU16> dpb_idx;
        std::vector<mfxU16> l0_idx;
        std::vector<mfxU16> l1_idx;
        std::vector<mfxU16> l0_parity;
        std::vector<mfxU16> l1_parity;
    } state[2];

    void Clear()
    {
        reference_frames.clear();

        for (mfxU32 fieldId = 0; fieldId < 2; ++fieldId)
        {
            state[fieldId].dpb_idx.clear();
            state[fieldId].l0_idx.clear();
            state[fieldId].l1_idx.clear();
            state[fieldId].l0_parity.clear();
            state[fieldId].l1_parity.clear();
        }
    }
};

/* Group of functions below implements some usefull operations for current frame / field of the task:
   Frame type extraction, field parity, POC */

inline mfxU8 GetFirstField(const iTask& task)
{
    return (task.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
}

inline mfxI32 GetPoc(const iTask& task, mfxU32 parity)
{
    return 2 * ((task.m_frameOrder - task.m_frameOrderIdr) & 0x7fffffff) + (parity != GetFirstField(task));
}

inline mfxU8 ExtractFrameType(const iTask& task)
{
    return task.m_type[GetFirstField(task)];
}

inline mfxU8 ExtractFrameType(const iTask& task, mfxU32 fieldId)
{
    if (!fieldId){
        return task.m_type[GetFirstField(task)];
    }
    else{
        return task.m_type[!GetFirstField(task)];
    }
}

inline mfxU16 createType(const iTask& task)
{
    return ((mfxU16)task.m_type[!GetFirstField(task)] << 8) | task.m_type[GetFirstField(task)];
}

inline mfxU8 extractType(mfxU16 type, mfxU32 fieldId)
{
    return fieldId ? (type >> 8) : (type & 255);
}

inline void InitNewDpbFrame(
    DpbFrame &      ref,
    iTask &         task,
    mfxU32          fid)
{
    ref.m_poc[0] = GetPoc(task, TFIELD);
    ref.m_poc[1] = GetPoc(task, BFIELD);
    ref.m_frameOrder     = task.m_frameOrder;
    ref.m_frameNum       = task.m_frameNum;
    ref.m_frameNumWrap   = task.m_frameNumWrap;
    ref.m_longTermPicNum = task.m_longTermPicNum;
    ref.m_longterm       = 0;
    ref.m_refBase        = 0;

    ref.m_refPicFlag[fid]  = !!(task.m_type[fid]  & MFX_FRAMETYPE_REF);
    ref.m_refPicFlag[!fid] = !!(task.m_type[!fid] & MFX_FRAMETYPE_REF);
    if (task.m_fieldPicFlag)
        ref.m_refPicFlag[!fid] = 0;
}

#endif // __SAMPLE_FEI_ENC_TASK_H__