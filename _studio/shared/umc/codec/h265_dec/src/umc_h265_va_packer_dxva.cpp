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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_h265_va_packer_dxva.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{
    PackerDXVA2::PackerDXVA2(VideoAccelerator * va)
        : Packer(va)
        , m_statusReportFeedbackCounter(1)
        , m_refFrameListCacheSize(0)
    {
    }

    Status PackerDXVA2::GetStatusReport(void * pStatusReport, size_t size)
    {
        return m_va->ExecuteStatusReportBuffer(pStatusReport, (Ipp32u)size);
    }

    Status PackerDXVA2::SyncTask(Ipp32s index, void * error)
    {
        return m_va->SyncTask(index, error);
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    bool PackerDXVA2::IsGPUSyncEventDisable()
    {
        return m_va->IsGPUSyncEventDisable();
    }
#endif

    void PackerDXVA2::BeginFrame(H265DecoderFrame*)
    {
        m_statusReportFeedbackCounter++;
        // WA: DXVA_Intel_Status_HEVC::StatusReportFeedbackNumber - USHORT, can't be 0 - reported status will be ignored
        if (m_statusReportFeedbackCounter > USHRT_MAX)
            m_statusReportFeedbackCounter = 1;
    }

    void PackerDXVA2::EndFrame()
    {
    }

    void PackerDXVA2::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
        int sliceCount = sliceInfo->GetSliceCount();

        if (!sliceCount)
            return;

        H265Slice *pSlice = sliceInfo->GetSlice(0);
        if (!pSlice)
            return;

        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        if (!pSeqParamSet || !pPicParamSet)
            throw h265_exception(UMC_ERR_FAILED);

        H265DecoderFrame const* pCurrentFrame = pSlice->GetCurrentFrame();
        if (!pCurrentFrame)
            throw h265_exception(UMC_ERR_FAILED);

        Ipp32s first_slice = 0;

        for (;;)
        {
            bool notchopping = true;
            PackPicParams(pCurrentFrame, sliceInfo, supplier);
            if (pSeqParamSet->scaling_list_enabled_flag)
            {
                PackQmatrix(pSlice);
            }

            if (pPicParamSet->tiles_enabled_flag)
            {
                PackSubsets(frame);
            }

            Ipp32u sliceNum = 0;
            for (Ipp32s n = first_slice; n < sliceCount; n++)
            {
                notchopping = PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
                if (!notchopping)
                {
                    //dependent slices should be with first independent slice
                    for (Ipp32s i = n; i >= first_slice; i--)
                    {
                        if (!sliceInfo->GetSlice(i)->GetSliceHeader()->dependent_slice_segment_flag)
                            break;

                        UMCVACompBuffer *headVABffr = 0;

                        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
                        Ipp32s headerSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Intel_Slice_HEVC_Long) : sizeof(DXVA_Slice_HEVC_Short);
                        headVABffr->SetDataSize(headVABffr->GetDataSize() - headerSize); //remove one slice

                        n--;
                    }

                    if (n <= first_slice) // avoid splitting of slice
                    {
                        m_va->Execute(); //for free picParam and Quant buffers
                        return;
                    }

                    first_slice = n;
                    break;
                }

                sliceNum++;
            }

            Status s = m_va->Execute();
            if(s != UMC_OK)
                throw h265_exception(s);

            if (!notchopping)
                continue;

            break;
        }
    }

    void PackerDXVA2::PackSubsets(const H265DecoderFrame *)
    { /* Nothing to do here, derived classes could extend behavior */ }

    template <typename T>
    void PackerDXVA2::PackQmatrix(H265Slice const* pSlice, T* pQmatrix)
    {
        const H265ScalingList *scalingList = 0;
        if (pSlice->GetPicParam()->pps_scaling_list_data_present_flag)
        {
            scalingList = pSlice->GetPicParam()->getScalingList();
        }
        else if (pSlice->GetSeqParam()->sps_scaling_list_data_present_flag)
        {
            scalingList = pSlice->GetSeqParam()->getScalingList();
        }
        else
        {
            // TODO: build default scaling list in target buffer location
            static bool doInit = true;
            static H265ScalingList sl;

            if (doInit)
            {
                for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
                {
                    for (Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
                    {
                        const int *src = getDefaultScalingList(sizeId, listId);
                        int *dst = sl.getScalingListAddress(sizeId, listId);
                        int count = min(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]);
                        MFX_INTERNAL_CPY(dst, src, sizeof(Ipp32s) * count);
                        sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                    }
                }

                doInit = false;
            }

            scalingList = &sl;
        }

        //new driver want list to be in raster scan, but we made it flatten during [H265HeadersBitstream::xDecodeScalingList]
        //so we should restore it now
        bool force_upright_scan =
            m_va->ScalingListScanOrder() == 0;

        initQMatrix<16>(scalingList, SCALING_LIST_4x4, pQmatrix->ucScalingLists0, force_upright_scan);    // 4x4
        initQMatrix<64>(scalingList, SCALING_LIST_8x8, pQmatrix->ucScalingLists1, force_upright_scan);    // 8x8
        initQMatrix<64>(scalingList, SCALING_LIST_16x16, pQmatrix->ucScalingLists2, force_upright_scan);    // 16x16
        initQMatrix(scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3, force_upright_scan);    // 32x32

        for (Ipp32u sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
        {
            for (Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
            {
                if (sizeId == SCALING_LIST_16x16)
                    pQmatrix->ucScalingListDCCoefSizeID2[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
                else if (sizeId == SCALING_LIST_32x32)
                    pQmatrix->ucScalingListDCCoefSizeID3[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
            }
        }
    }

    //explicit instantiate PackQmatrix for MS's & Intel's Qmatrix type
    template void PackerDXVA2::PackQmatrix<DXVA_Qmatrix_HEVC>(H265Slice const*, DXVA_Qmatrix_HEVC*);
    template void PackerDXVA2::PackQmatrix<DXVA_Intel_Qmatrix_HEVC>(H265Slice const*, DXVA_Intel_Qmatrix_HEVC*);
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER