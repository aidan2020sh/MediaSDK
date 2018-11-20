//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA_DXVA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_hevc_ddi.h"
#include "umc_h265_va_packer_dxva.h"
#include "umc_h265_widevine_slice_decoding.h"

#include "umc_va_dxva2_protected.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{

    class PackerDXVA2_Widevine
        : public PackerDXVA2
    {

    public:

        PackerDXVA2_Widevine(VideoAccelerator * va)
            : PackerDXVA2(va)
        {}

    private:

        void PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier) override;
        void PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier) override;
        bool PackSliceParams(H265Slice*, uint32_t& /*sliceNum*/, bool /*isLastSlice*/)
        { return true; }
        void PackQmatrix(const H265Slice* /*pSlice*/) {}
    };

    Packer* CreatePackerWidevine(UMC::VideoAccelerator* va)
    { return new PackerDXVA2_Widevine(va); }

    void PackerDXVA2_Widevine::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        if (!m_va || !frame || !supplier)
            return;

        H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
        if (!sliceInfo)
            return;

        int sliceCount = sliceInfo->GetSliceCount();
        H265Slice *pSlice = sliceInfo->GetSlice(0);
        if (!pSlice || !sliceCount)
            return;

        H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

        PackPicParams(pCurrentFrame, sliceInfo, supplier);

            Status s = m_va->Execute();
            if(s != UMC_OK)
                throw h265_exception(s);

        if (m_va->GetProtectedVA())
        {
            mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

            if (bs && bs->EncryptedData)
            {
                int32_t count = sliceInfo->GetSliceCount();
                m_va->GetProtectedVA()->MoveBSCurrentEncrypt(count);
            }
        }
    }

    void PackerDXVA2_Widevine::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
    {
        UMCVACompBuffer *compBuf;
        DXVA_Intel_PicParams_HEVC *pPicParam = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        compBuf->SetDataSize(sizeof(DXVA_Intel_PicParams_HEVC));

        memset(pPicParam, 0, sizeof(DXVA_Intel_PicParams_HEVC));

        H265WidevineSlice* pSlice = pSliceInfo ? (H265WidevineSlice*)pSliceInfo->GetSlice(0) : nullptr;
        if (!pSlice)
            return;

        H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();
        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();

        //
        //
        pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;    // ?
        pPicParam->CurrPicOrderCntVal   = sliceHeader->slice_pic_order_cnt_lsb;

        int count = 0;
        int cntRefPicSetStCurrBefore = 0,
            cntRefPicSetStCurrAfter  = 0,
            cntRefPicSetLtCurr = 0;
        H265DBPList *dpb = supplier->GetDPBList();
        if (!dpb)
            throw h265_exception(UMC_ERR_FAILED);
        ReferencePictureSet *rps = pSlice->getRPS();
        for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]) ; frame = frame->future())
        {
            if(frame != pCurrentFrame)
            {
                int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

                if(refType != NO_REFERENCE)
                {
                    pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                    pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                    pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                    count++;
                }
            }
        }

        uint32_t index;
        int pocList[3*8];
        int numRefPicSetStCurrBefore = 0,
            numRefPicSetStCurrAfter  = 0,
            numRefPicSetLtCurr       = 0;
        for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
                pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            int32_t poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = supplier->GetDPBList()->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }

        for(int n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
        {
            if (!rps->getUsed(n))
                continue;

            for(int k=0;k < count;k++)
            {
                if(pocList[n] == pPicParam->PicOrderCntValList[k])
                {
                    if(n < numRefPicSetStCurrBefore)
                        pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                        pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                        pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
                }
            }
        }

        m_refFrameListCacheSize = count;
        for(int n=count;n < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]);n++)
        {
            pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
            pPicParam->PicOrderCntValList[n] = -1;
        }

        for(int i=0;i < 8;i++)
        {
            if(i >= cntRefPicSetStCurrBefore)
                pPicParam->RefPicSetStCurrBefore[i] = 0xff;
            if(i >= cntRefPicSetStCurrAfter)
                pPicParam->RefPicSetStCurrAfter[i] = 0xff;
            if(i >= cntRefPicSetLtCurr)
                pPicParam->RefPicSetLtCurr[i] = 0xff;
        }

        pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        pPicParam->TotalNumEntryPointOffsets = pSlice->GetWidevineStatusReportNumber();
    }
} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA_DXVA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H265_VIDEO_DECODER