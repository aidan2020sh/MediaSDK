/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_TASK_SUPPLIER_H
#define __UMC_H264_TASK_SUPPLIER_H

#include <vector>
#include <list>
#include "umc_h264_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h264_heap.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_frame_info.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_headers.h"

#include "umc_frame_allocator.h"

#include "umc_h264_au_splitter.h"

namespace UMC
{
class TaskBroker;

class H264DBPList;
class H264DecoderFrame;
class MediaData;

class BaseCodecParams;
class H264SegmentDecoderMultiThreaded;
class TaskBrokerSingleThreadDXVA;

class MemoryAllocator;
struct H264IntraTypesProp;
class LocalResources;

enum
{
    BASE_VIEW                   = 0,
    INVALID_VIEW_ID             = 0xffffffff
};

class ErrorStatus
{
public:

    bool IsExistHeadersError() const 
    {
        return isSPSError || isPPSError;
    }

protected:
    Ipp32s isSPSError;
    Ipp32s isPPSError;

    ErrorStatus()
    {
        Reset();
    }

    void Reset()
    {
        isSPSError = 0;
        isPPSError = 0;
    }
};

/****************************************************************************************************/
// DPBOutput class routine
/****************************************************************************************************/
class DPBOutput
{
public:
    DPBOutput();

    void Reset(bool disableDelayFeature = false);
    
    bool IsUseSEIDelayOutputValue() const;
    bool IsUsePicOrderCnt() const;

    bool IsUseDelayOutputValue() const;

    Ipp32s GetDPBOutputDelay(H264SEIPayLoad * payload);

    void OnNewSps(H264SeqParamSet * sps);

private:

    struct
    {
        Ipp32u use_payload_sei_delay : 1;
        Ipp32u use_pic_order_cnt_type : 1;
    } m_isUseFlags;
};
/****************************************************************************************************/
// Skipping class routine
/****************************************************************************************************/
class Skipping
{
public:
    Skipping();
    virtual ~Skipping();

    void PermanentDisableDeblocking(bool disable);
    bool IsShouldSkipDeblocking(H264DecoderFrame * pFrame, Ipp32s field);
    bool IsShouldSkipFrame(H264DecoderFrame * pFrame, Ipp32s field);
    void ChangeVideoDecodingSpeed(Ipp32s& num);
    void Reset();

    struct SkipInfo
    {
        bool isDeblockingTurnedOff;
        Ipp32s numberOfSkippedFrames;
    };

    SkipInfo GetSkipInfo() const;

private:

    Ipp32s m_VideoDecodingSpeed;
    Ipp32s m_SkipCycle;
    Ipp32s m_ModSkipCycle;
    Ipp32s m_PermanentTurnOffDeblocking;
    Ipp32s m_SkipFlag;

    Ipp32s m_NumberOfSkippedFrames;
};

/****************************************************************************************************/
// POCDecoder
/****************************************************************************************************/
class POCDecoder
{
public:

    POCDecoder();

    virtual ~POCDecoder();
    void DecodePictureOrderCount(const H264Slice *slice, Ipp32s frame_num);

    Ipp32s DetectFrameNumGap(H264Slice *slice);

    // Set the POCs when frame gap is processed
    void DecodePictureOrderCountFrameGap(H264DecoderFrame *pFrame,
                                         const H264SliceHeader *pSliceHeader,
                                         Ipp32s frameNum);
    // Set the POCs when fake frames are imagined
    void DecodePictureOrderCountFakeFrames(H264DecoderFrame *pFrame,
                                           const H264SliceHeader *pSliceHeader);
    // Set the POCs when the frame is initialized
    void DecodePictureOrderCountInitFrame(H264DecoderFrame *pFrame,
                                          Ipp32s fieldIdx);

    void Reset(Ipp32s IDRFrameNum = 0);

protected:
    Ipp32s                     m_PrevFrameRefNum;
    Ipp32s                     m_FrameNum;
    Ipp32s                     m_PicOrderCnt;
    Ipp32s                     m_PicOrderCntMsb;
    Ipp32s                     m_PicOrderCntLsb;
    Ipp32s                     m_FrameNumOffset;
    Ipp32u                     m_TopFieldPOC;
    Ipp32u                     m_BottomFieldPOC;
};

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
class SEI_Storer
{
public:

    struct SEI_Message
    {
        H264DecoderFrame * frame;
        size_t             msg_size;
        size_t             offset;
        Ipp8u            * data;
        Ipp64f             timestamp;
        SEI_TYPE           type;
        Ipp32s             isUsed;
        Ipp32s             auID;

        SEI_Message()
            : frame(0)
            , msg_size(0)
            , offset(0)
            , data(0)
            , timestamp(0)
            , type(SEI_RESERVED)
            , isUsed(0)
            , auID(0)
        {
        }
    };

    SEI_Storer();

    virtual ~SEI_Storer();

    void Init();

    void Close();

    void Reset();

    void SetTimestamp(H264DecoderFrame * frame);

    SEI_Message* AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type, Ipp32s auIndex);

    const SEI_Message * GetPayloadMessage();

    void SetFrame(H264DecoderFrame * frame, Ipp32s auIndex);
    void SetAUID(Ipp32s auIndex);

private:

    enum
    {
        MAX_BUFFERED_SIZE = 16 * 1024, // 16 kb
        START_ELEMENTS = 10,
        MAX_ELEMENTS = 128
    };

    std::vector<Ipp8u>  m_data;
    std::vector<SEI_Message> m_payloads;

    size_t m_offset;
    Ipp32s m_lastUsed;
};

/****************************************************************************************************/
// ViewItem class routine
/****************************************************************************************************/
struct ViewItem
{
    // Default constructor
    ViewItem(void);

    // Copy constructor
    ViewItem(const ViewItem &src);

    ~ViewItem();

    // Initialize th view, allocate resources
    Status Init(Ipp32u view_id);

    // Close the view and release all resources
    void Close(void);

    // Reset the view and reset all resource
    void Reset(void);

    // Reset the size of DPB for particular view item
    void SetDPBSize(H264SeqParamSet *pSps, Ipp8u & level_idc);

    H264DBPList *GetDPBList(Ipp32s dIdRev = 0)
    {
        return pDPB[dIdRev].get();
    }

    POCDecoder *GetPOCDecoder(Ipp32s dIdRev = 0)
    {
        return pPOCDec[dIdRev].get();
    }

    // Keep view ID to identify initialized views
    Ipp32s viewId;

    // Pointer to the view's DPB
    mutable std::auto_ptr<H264DBPList> pDPB[MAX_NUM_LAYERS];

    // Pointer to the POC processing object
    mutable std::auto_ptr<POCDecoder> pPOCDec[MAX_NUM_LAYERS];

    bool m_isDisplayable;

    // Size of DPB capacity in frames
    Ipp32u dpbSize;
    // Maximum number frames used semultaneously
    Ipp32u maxDecFrameBuffering;

    // Maximum possible long term reference index
    Ipp32s MaxLongTermFrameIdx[MAX_NUM_LAYERS];

    // Pointer to the frame being processed
    H264DecoderFrame *pCurFrame;

    Ipp64f localFrameTime;
};

typedef std::list<ViewItem> ViewList;

/****************************************************************************************************/
// MVC extension class routine
/****************************************************************************************************/
class MVC_Extension
{
public:
    MVC_Extension();
    virtual ~MVC_Extension();

    Status Init();
    virtual void Close();
    virtual void Reset();

    virtual bool IsShouldSkipSlice(H264Slice * slice);

    virtual bool IsShouldSkipSlice(H264NalExtension *nal_ext);

    Ipp32u GetLevelIDC() const;

    void SetTemporalId(Ipp32u temporalId);

    Status SetViewList(const std::vector<Ipp32u> & targetView, const std::vector<Ipp32u> & dependencyList);

    ViewItem *FindView(Ipp32s viewId);
    
    ViewItem &GetView(Ipp32s viewId = -1);

    ViewItem &GetViewByNumber(Ipp32s viewNum);

    void MoveViewToHead(Ipp32s view_id);

    Ipp32s GetViewCount() const;

    bool IncreaseCurrentView();

    Ipp32s GetBaseViewId() const;

    bool IsExtension() const; // MVC or SVC

protected:
    Ipp32u m_temporal_id;
    Ipp32u m_priority_id;

    Ipp8u  m_level_idc;
    Ipp32u m_currentDisplayView;

    Ipp32u m_currentView;

    enum DecodingMode
    {
        UNKNOWN_DECODING_MODE,
        AVC_DECODING_MODE,
        MVC_DECODING_MODE,
        SVC_DECODING_MODE
    };

    DecodingMode m_decodingMode;

    typedef std::list<Ipp32u> ViewIDsList;
    ViewIDsList  m_viewIDsList;

    // Array of pointers to views and their components
    ViewList m_views;

    void ChooseLevelIdc(const H264SeqParamSetMVCExtension * extension, Ipp8u baseViewLevelIDC, Ipp8u extensionLevelIdc);
    void AnalyzeDependencies(const H264SeqParamSetMVCExtension * extension);
    Status AllocateView(Ipp32s view_id);
    ViewItem & AllocateAndInitializeView(H264Slice * slice);
};

/****************************************************************************************************/
// SVC extension class routine
/****************************************************************************************************/
class SVC_Extension : public MVC_Extension
{
public:

    SVC_Extension();
    virtual ~SVC_Extension();

    virtual void Reset();
    virtual void Close();

    virtual bool IsShouldSkipSlice(H264Slice * slice);
    virtual bool IsShouldSkipSlice(H264NalExtension *nal_ext);
    void ChooseLevelIdc(const H264SeqParamSetSVCExtension * extension, Ipp8u baseViewLevelIDC, Ipp8u extensionLevelIdc);
    void SetSVCTargetLayer(Ipp32u dependency_id, Ipp32u quality_id, Ipp32u temporal_id);

protected:
    Ipp32u m_dependency_id;
    Ipp32u m_quality_id;
};

/****************************************************************************************************/
// DecReferencePictureMarking
/****************************************************************************************************/
class DecReferencePictureMarking
{
public:

    DecReferencePictureMarking();

    Status UpdateRefPicMarking(ViewItem &view, H264DecoderFrame * pFrame, H264Slice * pSlice, Ipp32s field_index);

    void SlideWindow(ViewItem &view, H264Slice * pSlice, Ipp32s field_index);

    void Reset();

    void CheckSEIRepetition(ViewItem &view, H264DecoderFrame * frame);
    Status CheckSEIRepetition(ViewItem &view, H264SEIPayLoad *payload);

    Ipp32u  GetDPBError() const;

protected:

    Ipp32u  m_isDPBErrorFound;

    enum ChangeItemFlags
    {
        SHORT_TERM      = 0x00000001,
        LONG_TERM       = 0x00000002,

        FULL_FRAME      = 0x00000010,
        BOTTOM_FIELD    = 0x00000020,
        TOP_FIELD       = 0x00000040,

        SET_REFERENCE   = 0x00000100,
        UNSET_REFERENCE = 0x00000200
    };

    struct DPBChangeItem
    {
        struct
        {
            Ipp8u isShortTerm   : 1; // short or long
            Ipp8u isSet         : 1; // set or free
            Ipp8u isFrame       : 1; // full frame
            Ipp8u isBottom      : 1; // if doesn't full frame is it top or bottom?
        } m_type;

        H264DecoderFrame *  m_pRefFrame;
        H264DecoderFrame *  m_pCurrentFrame;
    };

    typedef std::list<DPBChangeItem> DPBCommandsList;
    DPBCommandsList m_commandsList;
    Ipp32s   m_frameCount;

    DPBChangeItem* AddItem(DPBCommandsList & list, H264DecoderFrame * currentFrame, H264DecoderFrame * refFrame, Ipp32u flags);
    DPBChangeItem* AddItemAndRun(H264DecoderFrame * currentFrame, H264DecoderFrame * refFrame, Ipp32u flags);

    void Undo(const DPBChangeItem * item);
    void Redo(const DPBChangeItem * item);
    void MakeChange(bool isUndo, const DPBChangeItem * item);

    void Undo(const H264DecoderFrame * frame);
    void Redo(const H264DecoderFrame * frame);
    void MakeChange(bool isUndo, const H264DecoderFrame * frame);
    void Remove(H264DecoderFrame * frame);
    void RemoveOld();

#ifdef _DEBUG
    void DebugPrintItems();
#endif

    bool CheckUseless(DPBChangeItem * item);
    void ResetError();
};

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
class TaskSupplier : public Skipping, public AU_Splitter, public SVC_Extension, public DecReferencePictureMarking, public DPBOutput,
        public ErrorStatus
{
    friend class TaskBroker;
    friend class TaskBrokerSingleThreadDXVA;

public:

    TaskSupplier();
    virtual ~TaskSupplier();

    virtual Status Init(VideoDecoderParams *pInit);

    virtual Status PreInit(VideoDecoderParams *pInit);

    virtual void Reset();
    virtual void Close();

    Status GetInfo(VideoDecoderParams *lpInfo);

    virtual Status AddSource(MediaData * pSource, MediaData *dst);

    Status ProcessNalUnit(MediaDataEx *nalUnit);

    void SetMemoryAllocator(MemoryAllocator *pMemoryAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
    }

    void SetFrameAllocator(FrameAllocator *pFrameAllocator)
    {
        m_pFrameAllocator = pFrameAllocator;
    }

    virtual H264DecoderFrame *GetFrameToDisplayInternal(bool force);
    virtual bool GetFrameToDisplay(MediaData *dst, bool force);
    Status  SetParams(BaseCodecParams* params);

    Status GetUserData(MediaData * pUD);

    bool IsWantToShowFrame(bool force = false);

    H264DBPList *GetDPBList(Ipp32u viewId, Ipp32s dIdRev)
    {
        ViewItem *pView = FindView(viewId);
        if (!pView)
            return NULL;
        return pView->GetDPBList(dIdRev);
    }

    TaskBroker * GetTaskBroker()
    {
        return m_pTaskBroker;
    }

    virtual Status RunDecoding_1();
    virtual H264DecoderFrame * FindSurface(FrameMemID id);

    void PostProcessDisplayFrame(MediaData *dst, H264DecoderFrame *pFrame);

    virtual void AfterErrorRestore();

    SEI_Storer * GetSEIStorer() const { return m_sei_messages;}

    Headers * GetHeaders() { return &m_Headers;}

    inline
    const H264SeqParamSet *GetCurrentSequence(void) const
    {
        return m_Headers.m_SeqParams.GetCurrentHeader();
    }

    virtual H264Slice * DecodeSliceHeader(MediaDataEx *nalUnit);

    H264_Heap_Objects * GetObjHeap()
    {
        return &m_ObjHeap;
    }

    // Initialize MB ordering for the picture using slice groups as
    // defined in the picture parameter set.
    void SetMBMap(const H264Slice * slice, H264DecoderFrame *frame, LocalResources * localRes);

protected:

    virtual void CreateTaskBroker();

    void InitColorConverter(H264DecoderFrame *source, VideoData * videoData, Ipp8u force_field);

    void AddSliceToFrame(H264DecoderFrame *pFrame, H264Slice *pSlice);

    // Initialize just allocated frame with slice parameters
    virtual
    Status InitFreeFrame(H264DecoderFrame *pFrame, const H264Slice *pSlice);
    // Initialize frame's counter and corresponding parameters
    virtual
    void InitFrameCounter(H264DecoderFrame *pFrame, const H264Slice *pSlice);

    Status AddSlice(H264Slice * pSlice, bool force);
    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s m_field_index);
    virtual void OnFullFrame(H264DecoderFrame * pFrame);
    virtual bool ProcessNonPairedField(H264DecoderFrame * pFrame);

    void DBPUpdate(H264DecoderFrame * pFrame, Ipp32s field);

    virtual void AddFakeReferenceFrame(H264Slice * pSlice);

    Status AddOneFrame(MediaData * pSource, MediaData *dst);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat color_format);

    virtual Status DecodeHeaders(MediaDataEx *nalUnit);
    virtual Status DecodeSEI(MediaDataEx *nalUnit);

    Status ProcessFrameNumGap(H264Slice *slice, Ipp32s field, Ipp32s did, Ipp32s maxDid);

    // Obtain free frame from queue
    virtual H264DecoderFrame *GetFreeFrame(const H264Slice *pSlice = NULL);

    Status CompleteDecodedFrames(H264DecoderFrame ** decoded);

    H264DecoderFrame *GetAnyFrameToDisplay(bool force);
    void SetFrameDisplayed(Ipp32s poc);

    void PreventDPBFullness();

    H264DecoderFrame *AllocateNewFrame(const H264Slice *pSlice);
    H264DecoderFrame *AddLayerFrameSVC(H264DBPList *dpb, H264Slice *pSlice);
    void CalculateResizeParameters(H264DecoderFrame * pFrame, Ipp32s field);
    Status InitializeLayers(H264DecoderFrame * pFrame, Ipp32s field);
    Status AllocateDataForFrameSVC(H264DecoderFrame * pFrame, H264Slice *pSlice);

    Status InitializeLayers(AccessUnit *accessUnit, H264DecoderFrame * pFrame, Ipp32s field);
    void ApplyPayloadsToFrame(H264DecoderFrame * frame, H264Slice *slice, SeiPayloadArray * payloads);

    H264_Heap_Objects           m_ObjHeap;

    H264SegmentDecoderBase **m_pSegmentDecoder;
    Ipp32u m_iThreadNum;

    Ipp64f      m_local_delta_frame_time;
    bool        m_use_external_framerate;

    H264Slice * m_pLastSlice;

    H264DecoderFrame *m_pLastDisplayed;

    MemoryAllocator *m_pMemoryAllocator;
    FrameAllocator  *m_pFrameAllocator;

    // Keep track of which parameter set is in use.
    bool              m_WaitForIDR;

    Ipp32u            m_DPBSizeEx;
    Ipp32s            m_TrickModeSpeed;
    Ipp32s            m_frameOrder;

    TaskBroker * m_pTaskBroker;

    VideoDecoderParams     m_initializationParams;
    VideoData              m_LastNonCropDecodedFrame;
    BaseCodec              *m_pPostProcessing;

    NotifiersChain          m_DefaultNotifyChain;

    Ipp32s m_UIDFrameCounter;

    H264SEIPayLoad m_UserData;
    SEI_Storer *m_sei_messages;

    AccessUnit m_accessUnit;

    bool m_isInitialized;

    Mutex m_mGuard;

private:
    TaskSupplier & operator = (TaskSupplier &)
    {
        return *this;

    } // TaskSupplier & operator = (TaskSupplier &)

};

// can increase level_idc to hold num_ref_frames
inline Ipp32s CalculateDPBSize(Ipp8u & level_idc, Ipp32s width, Ipp32s height, Ipp32u num_ref_frames)
{
    Ipp32u dpbSize;

    num_ref_frames = IPP_MIN(16, num_ref_frames);

    for (;;)
    {
        Ipp32u MaxDPBx2;
        // MaxDPB, per Table A-1, Level Limits
        switch (level_idc)
        {
        case H264VideoDecoderParams::H264_LEVEL_1:
            MaxDPBx2 = 297;
            break;
        case H264VideoDecoderParams::H264_LEVEL_11:
            MaxDPBx2 = 675;
            break;
        case H264VideoDecoderParams::H264_LEVEL_12:
        case H264VideoDecoderParams::H264_LEVEL_13:
        case H264VideoDecoderParams::H264_LEVEL_2:
            MaxDPBx2 = 891*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_21:
            MaxDPBx2 = 1782*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_22:
        case H264VideoDecoderParams::H264_LEVEL_3:
            MaxDPBx2 = 6075;
            break;
        case H264VideoDecoderParams::H264_LEVEL_31:
            MaxDPBx2 = 6750*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_32:
            MaxDPBx2 = 7680*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_4:
        case H264VideoDecoderParams::H264_LEVEL_41:
        case H264VideoDecoderParams::H264_LEVEL_42:
            MaxDPBx2 = 12288*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_5:
            MaxDPBx2 = 41400*2;
            break;
        case H264VideoDecoderParams::H264_LEVEL_51:
        case H264VideoDecoderParams::H264_LEVEL_52:
            MaxDPBx2 = 69120*2;
            break;
        default:
            MaxDPBx2 = 69120*2; //get as much as we may
            break;
        }

        if (width == 0 && height == 0)
        {
            throw h264_exception(UMC_ERR_INVALID_PARAMS);
        }

        Ipp32u dpbLevel = (MaxDPBx2 * 512) / ((width * height) + ((width * height)>>1));
        dpbSize = IPP_MIN(16, dpbLevel);

        if (num_ref_frames <= dpbSize || level_idc == H264VideoDecoderParams::H264_LEVEL_MAX)
            break;

        switch (level_idc) // increase level_idc
        {
        case H264VideoDecoderParams::H264_LEVEL_1:
            level_idc = H264VideoDecoderParams::H264_LEVEL_11;
            break;
        case H264VideoDecoderParams::H264_LEVEL_11:
            level_idc = H264VideoDecoderParams::H264_LEVEL_12;
            break;
        case H264VideoDecoderParams::H264_LEVEL_12:
        case H264VideoDecoderParams::H264_LEVEL_13:
        case H264VideoDecoderParams::H264_LEVEL_2:
            level_idc = H264VideoDecoderParams::H264_LEVEL_21;
            break;
        case H264VideoDecoderParams::H264_LEVEL_21:
            level_idc = H264VideoDecoderParams::H264_LEVEL_22;
            break;
        case H264VideoDecoderParams::H264_LEVEL_22:
        case H264VideoDecoderParams::H264_LEVEL_3:
            level_idc = H264VideoDecoderParams::H264_LEVEL_31;
            break;
        case H264VideoDecoderParams::H264_LEVEL_31:
            level_idc = H264VideoDecoderParams::H264_LEVEL_32;
            break;
        case H264VideoDecoderParams::H264_LEVEL_32:
            level_idc = H264VideoDecoderParams::H264_LEVEL_4;
            break;
        case H264VideoDecoderParams::H264_LEVEL_4:
        case H264VideoDecoderParams::H264_LEVEL_41:
        case H264VideoDecoderParams::H264_LEVEL_42:
            level_idc = H264VideoDecoderParams::H264_LEVEL_5;
            break;
        case H264VideoDecoderParams::H264_LEVEL_5:
            level_idc = H264VideoDecoderParams::H264_LEVEL_51;
            break;
        case H264VideoDecoderParams::H264_LEVEL_51:
        case H264VideoDecoderParams::H264_LEVEL_52:
            level_idc = H264VideoDecoderParams::H264_LEVEL_MAX;
            break;
        }
    }

    return dpbSize;
}


inline H264DBPList *GetDPB(ViewList &views, Ipp32s viewId, Ipp32s dIdRev = 0)
{
    ViewList::iterator iter = views.begin();
    ViewList::iterator iter_end = views.end();

    for (; iter != iter_end; ++iter)
    {
        if (viewId == (*iter).viewId)
        {
            return (*iter).GetDPBList(dIdRev);
        }
    }

    throw h264_exception(UMC_ERR_FAILED);
} // H264DBPList *GetDPB(ViewList &views)

inline Ipp32u GetVOIdx(const H264SeqParamSetMVCExtension *pSeqParamSetMvc, Ipp32u viewId)
{
    for (Ipp32u i = 0; i <= pSeqParamSetMvc->num_views_minus1; ++i)
    {
        if (pSeqParamSetMvc->viewInfo[i].view_id == viewId)
        {
            return i;
        }
    }

    return 0;

} // Ipp32u GetVOIdx(const H264SeqParamSetMVCExtension *pSeqParamSetMvc, Ipp32u viewId)

inline Ipp32u GetInterViewFrameRefs(ViewList &views, Ipp32s viewId, Ipp32s auIndex, Ipp32u bottomFieldFlag)
{
    ViewList::iterator iter = views.begin();
    ViewList::iterator iter_end = views.end();
    Ipp32u numInterViewRefs = 0;

    for (; iter != iter_end; ++iter)
    {
        if (iter->viewId == viewId)
            break;

        // take into account DPBs with lower view ID
        H264DecoderFrame *pInterViewRef = iter->GetDPBList()->findInterViewRef(auIndex, bottomFieldFlag);
        if (pInterViewRef)
        {
            ++numInterViewRefs;
        }
    }

    return numInterViewRefs;

}

} // namespace UMC

#endif // __UMC_H264_TASK_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
