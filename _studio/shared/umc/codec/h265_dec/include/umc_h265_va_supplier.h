//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_VA_SUPPLIER_H
#define __UMC_H265_VA_SUPPLIER_H

#include "umc_h265_mfx_supplier.h"
#include "umc_h265_segment_decoder_dxva.h"

namespace UMC_HEVC_DECODER
{

class MFXVideoDECODEH265;

/****************************************************************************************************/
// TaskSupplier_H265
/****************************************************************************************************/
class VATaskSupplier :
          public MFXTaskSupplier_H265
        , public DXVASupport<VATaskSupplier>
{
    friend class TaskBroker_H265;
    friend class DXVASupport<VATaskSupplier>;
    friend class VideoDECODEH265;

public:

    VATaskSupplier();

    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    virtual void Reset();

    virtual void CreateTaskBroker();

    void SetBufferedFramesNumber(Ipp32u buffered);


protected:
    virtual UMC::Status AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *pPicParamSet);

    virtual void InitFrameCounter(H265DecoderFrame * pFrame, const H265Slice *pSlice);

    virtual void CompleteFrame(H265DecoderFrame * pFrame);

    virtual H265Slice * DecodeSliceHeader(UMC::MediaDataEx *nalUnit);

    virtual H265DecoderFrame *GetFrameToDisplayInternal(bool force);

    Ipp32u m_bufferedFrameNumber;

private:
    VATaskSupplier & operator = (VATaskSupplier &)
    {
        return *this;
    }
};

// this template class added to apply big surface pool workaround depends on platform
// because platform check can't be added inside VATaskSupplier
// for WidevineTaskSupplier the also can be applied
template <class BaseClass>
class VATaskSupplierBigSurfacePool:
    public BaseClass
{
public:
    VATaskSupplierBigSurfacePool()
    {};
    virtual ~VATaskSupplierBigSurfacePool()
    {};

protected:

    virtual UMC::Status AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet * pps)
    {
        UMC::Status ret = BaseClass::AllocateFrameData(pFrame, dimensions, pSeqParamSet, pps);

        if (ret == UMC::UMC_OK)
        {
            ViewItem_H265 *pView = BaseClass::GetView();
            H265DBPList *pDPB = pView->pDPB.get();

            pFrame->m_index = pDPB->GetFreeIndex();
        }

        return ret;
    }
};

}// namespace UMC_HEVC_DECODER


#endif // __UMC_H265_VA_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
