// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "mfx_h264_dispatcher.h"
#include "umc_h264_mfx_sw_supplier.h"
#include "umc_h264_task_broker_mt.h"
#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_dec_ippwrap.h"

namespace UMC
{

MFX_SW_TaskSupplier::MFX_SW_TaskSupplier()
{
    MFX_H264_PP::GetH264Dispatcher();
}
    
H264Slice * MFX_SW_TaskSupplier::CreateSlice()
{
    return m_ObjHeap.AllocateObject<H264SliceEx>();
}

void MFX_SW_TaskSupplier::CreateTaskBroker()
{
    switch(m_iThreadNum)
    {
    case 1:
        m_pTaskBroker = new TaskBrokerSingleThread(this);
        break;
    case 4:
    case 3:
    case 2:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    default:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    };

    for (uint32_t i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264SegmentDecoderMultiThreaded(m_pTaskBroker);
    }
}

void MFX_SW_TaskSupplier::SetMBMap(const H264Slice * slice, H264DecoderFrame *frame, LocalResources * localRes)
{
    if (!slice)
    {
        slice = frame->GetAU(0)->GetSlice(0);
        VM_ASSERT(slice);
        if (!slice)
            return;
    }

    const H264PicParamSet *pps = slice->GetPicParam();
    VM_ASSERT(frame->m_iResourceNumber >= 0);

    int32_t resource = frame->m_iResourceNumber;

    if (pps->num_slice_groups == 1)
    {
        localRes->GetMBInfo(resource).active_next_mb_table = localRes->GetDefaultMBMapTable();
        return;
    }

    int32_t additionalTable = resource + 1;
    const H264SeqParamSet *sps = slice->GetSeqParam();
    const H264SliceHeader * sliceHeader = slice->GetSliceHeader();

    uint32_t PicWidthInMbs = slice->GetSeqParam()->frame_width_in_mbs;
    uint32_t PicHeightInMapUnits = slice->GetSeqParam()->frame_height_in_mbs;
    if (sliceHeader->field_pic_flag)
        PicHeightInMapUnits >>= 1;

    uint32_t PicSizeInMbs = PicWidthInMbs*PicHeightInMapUnits;

    int32_t *mapUnitToSliceGroupMap = new int32_t[PicSizeInMbs];
    int32_t *MbToSliceGroupMap = new int32_t[PicSizeInMbs];

    for (uint32_t i = 0; i < PicSizeInMbs; i++)
        mapUnitToSliceGroupMap[i] = 0;

    switch (pps->SliceGroupInfo.slice_group_map_type)
    {
        case 0:
        {
            // interleaved slice groups: run_length for each slice group,
            // repeated until all MB's are assigned to a slice group
            uint32_t i = 0;
            do
                for(uint32_t iGroup = 0; iGroup < pps->num_slice_groups && i < PicSizeInMbs; i += pps->SliceGroupInfo.run_length[iGroup++])
                    for(uint32_t j = 0; j < pps->SliceGroupInfo.run_length[iGroup] && i + j < PicSizeInMbs; j++)
                        mapUnitToSliceGroupMap[i + j] = iGroup;
            while(i < PicSizeInMbs);
        }
        break;

    case 1:
        {
            // dispersed
            for(uint32_t i = 0; i < PicSizeInMbs; i++ )
                mapUnitToSliceGroupMap[i] = (((i % PicWidthInMbs) + (((i / PicWidthInMbs) * pps->num_slice_groups) / 2)) % pps->num_slice_groups);
        }
        break;

    case 2:
        {
            // foreground + leftover: Slice groups are rectangles, any MB not
            // in a defined rectangle is in the leftover slice group, a MB within
            // more than one rectangle is in the lower-numbered slice group.

            for(uint32_t i = 0; i < PicSizeInMbs; i++)
                mapUnitToSliceGroupMap[i] = (pps->num_slice_groups - 1);

            for(int32_t iGroup = pps->num_slice_groups - 2; iGroup >= 0; iGroup--)
            {
                uint32_t yTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] / PicWidthInMbs;
                uint32_t xTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] % PicWidthInMbs;
                uint32_t yBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] / PicWidthInMbs;
                uint32_t xBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] % PicWidthInMbs;

                for(uint32_t y = yTopLeft; y <= yBottomRight; y++)
                    for(uint32_t x = xTopLeft; x <= xBottomRight; x++)
                        mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = iGroup;
            }
        }
        break;

    case 3:
        {
            // Box-out, clockwise or counterclockwise. Result is two slice groups,
            // group 0 included by the box, group 1 excluded.

            int32_t x, y, leftBound, rightBound, topBound, bottomBound;
            int32_t mapUnitVacant = 0;
            int8_t xDir, yDir;
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;

            x = leftBound = rightBound = (PicWidthInMbs - dir_flag) / 2;
            y = topBound = bottomBound = (PicHeightInMapUnits - dir_flag) / 2;

            xDir  = dir_flag - 1;
            yDir  = dir_flag;

            uint32_t uNumInGroup0 = MFX_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);

            for(uint32_t i = 0; i < PicSizeInMbs; i++)
                mapUnitToSliceGroupMap[i] = 1;

            for(uint32_t k = 0; k < uNumInGroup0; k += mapUnitVacant)
            {
                mapUnitVacant = (mapUnitToSliceGroupMap[y * PicWidthInMbs + x] == 1);

                if(mapUnitVacant)
                    mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = 0;

                if(xDir == -1 && x == leftBound)
                {
                    leftBound = MFX_MAX(leftBound - 1, 0);
                    x = leftBound;
                    xDir = 0;
                    yDir = 2 * dir_flag - 1;
                }
                else if(xDir == 1 && x == rightBound)
                {
                    rightBound = MFX_MIN(rightBound + 1, (int32_t)PicWidthInMbs - 1);
                    x = rightBound;
                    xDir = 0;
                    yDir = 1 - 2 * dir_flag;
                }
                else if(yDir == -1 && y == topBound)
                {
                    topBound = MFX_MAX(topBound - 1, 0);
                    y = topBound;
                    xDir = 1 - 2 * dir_flag;
                    yDir = 0;
                }
                else if(yDir == 1 && y == bottomBound)
                {
                    bottomBound = MFX_MIN(bottomBound + 1, (int32_t)PicHeightInMapUnits - 1);
                    y = bottomBound;
                    xDir = 2 * dir_flag - 1;
                    yDir = 0;
                }
                else
                {
                    x += xDir;
                    y += yDir;
                }
            }
        }
        break;

    case 4:
        {
            // raster-scan: 2 slice groups
            uint32_t uNumInGroup0 = MFX_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            uint32_t sizeOfUpperLeftGroup = (dir_flag ? (PicSizeInMbs - uNumInGroup0) : uNumInGroup0);

            for(uint32_t i = 0; i < PicSizeInMbs; i++)
                if(i < sizeOfUpperLeftGroup)
                    mapUnitToSliceGroupMap[i] = dir_flag;
                else
                    mapUnitToSliceGroupMap[i] = 1 - dir_flag;
        }
        break;

    case 5:
        {
            // wipe: 2 slice groups, the vertical version of case 4.
            //  L L L L R R R R R
            //  L L L L R R R R R
            //  L L L R R R R R R
            //  L L L R R R R R R

            uint32_t uNumInGroup0 = MFX_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            uint32_t sizeOfUpperLeftGroup = (dir_flag ? (PicSizeInMbs - uNumInGroup0) : uNumInGroup0);

            uint32_t k = 0;
            for(uint32_t j = 0; j < PicWidthInMbs; j++)
                for(uint32_t i = 0; i < PicHeightInMapUnits; i++)
                    if(k++ < sizeOfUpperLeftGroup)
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = dir_flag;
                    else
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = 1 - dir_flag;
        }
        break;

    case 6:
        {
            // explicit map read from bitstream, contains slice group id for
            // each map unit
            for(uint32_t i = 0; i < pps->SliceGroupInfo.pSliceGroupIDMap.size(); i++)
                mapUnitToSliceGroupMap[i] = pps->SliceGroupInfo.pSliceGroupIDMap[i];
        }
        break;

    default:
        // can't happen
        break;
    }    // switch map type

    // Filling array groupMap as in H264 standart

    if (sps->frame_mbs_only_flag || sliceHeader->field_pic_flag)
    {
        for(uint32_t i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[i];
        }
    }
    else if (sliceHeader->MbaffFrameFlag)
    {
        for(uint32_t i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[i/2];
        }
    }
    else if(sps->frame_mbs_only_flag == 0 && !sliceHeader->MbaffFrameFlag && !sliceHeader->field_pic_flag)
    {
        for(uint32_t i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[(i / (2 * PicWidthInMbs)) * PicWidthInMbs + ( i % PicWidthInMbs )];
        }
    }

    H264DecoderMBAddr * next_mb_tables = localRes->next_mb_tables[additionalTable];

    for(uint32_t n = 0; n < PicSizeInMbs; n++)
    {
        uint32_t i = n + 1;
        while (i < PicSizeInMbs && MbToSliceGroupMap[i] != MbToSliceGroupMap[n])
            i++;

        next_mb_tables[n] = (i >= PicSizeInMbs) ? -1 : i;
    }

    if (sliceHeader->field_pic_flag)
    {
        for(uint32_t i = 0; i < PicSizeInMbs; i++ )
        {
            next_mb_tables[i + PicSizeInMbs] = (next_mb_tables[i] == -1) ? -1 : (next_mb_tables[i] + PicSizeInMbs);
        }
    }

    localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];

    delete[] mapUnitToSliceGroupMap;
    delete[] MbToSliceGroupMap;
}

void DefaultFill(H264DecoderFrame * frame, int32_t fields_mask, bool isChromaOnly, uint8_t defaultValue = 128)
{
    mfxSize roi;

    int32_t field_factor = fields_mask == 2 ? 0 : 1;
    int32_t field = field_factor ? fields_mask : 0;

    if (!isChromaOnly)
    {
        roi = frame->lumaSize();
        roi.height >>= field_factor;

        if (frame->m_pYPlane)
            SetPlane(defaultValue, frame->m_pYPlane + field*frame->pitch_luma(),
                frame->pitch_luma() << field_factor, roi);
    }

    roi = frame->chromaSize();
    roi.height >>= field_factor;

    if (frame->m_pUVPlane) // NV12
    {
        roi.width *= 2;
        SetPlane(defaultValue, frame->m_pUVPlane + field*frame->pitch_chroma(), frame->pitch_chroma() << field_factor, roi);
    }
    else
    {
        if (frame->m_pUPlane)
            SetPlane(defaultValue, frame->m_pUPlane + field*frame->pitch_chroma(), frame->pitch_chroma() << field_factor, roi);
        if (frame->m_pVPlane)
            SetPlane(defaultValue, frame->m_pVPlane + field*frame->pitch_chroma(), frame->pitch_chroma() << field_factor, roi);
    }
}

bool MFX_SW_TaskSupplier::ProcessNonPairedField(H264DecoderFrame * pFrame)
{
    if (MFXTaskSupplier::ProcessNonPairedField(pFrame))
    {
        H264Slice * pSlice = pFrame->GetAU(0)->GetSlice(0);
        int32_t isBottom = pSlice->IsBottomField() ? 0 : 1;
        DefaultFill(pFrame, isBottom, false);
        return true;
    }

    return false;
}

void MFX_SW_TaskSupplier::AddFakeReferenceFrame(H264Slice * pSlice)
{
    H264DecoderFrame *pFrame = GetFreeFrame(pSlice);
    if (!pFrame)
        return;

    Status umcRes = AllocateFrameData(pFrame);
    if (umcRes != UMC_OK)
    {
        return;
    }

    int32_t frame_num = pSlice->GetSliceHeader()->frame_num;
    if (pSlice->GetSliceHeader()->field_pic_flag == 0)
    {
        pFrame->setPicNum(frame_num, 0);
    }
    else
    {
        pFrame->setPicNum(frame_num*2+1, 0);
        pFrame->setPicNum(frame_num*2+1, 1);
    }

    pFrame->SetisShortTermRef(true, 0);
    pFrame->SetisShortTermRef(true, 1);

    pFrame->SetFrameAsNonExist();
    pFrame->DecrementReference();

    DefaultFill(pFrame, 2, false);

    H264SliceHeader* sliceHeader = pSlice->GetSliceHeader();
    ViewItem &view = GetView(sliceHeader->nal_ext.mvc.view_id);

    if (pSlice->GetSeqParam()->pic_order_cnt_type != 0)
    {
        int32_t tmp1 = sliceHeader->delta_pic_order_cnt[0];
        int32_t tmp2 = sliceHeader->delta_pic_order_cnt[1];
        sliceHeader->delta_pic_order_cnt[0] = sliceHeader->delta_pic_order_cnt[1] = 0;

        view.GetPOCDecoder(0)->DecodePictureOrderCount(pSlice, frame_num);

        sliceHeader->delta_pic_order_cnt[0] = tmp1;
        sliceHeader->delta_pic_order_cnt[1] = tmp2;
    }

    view.GetPOCDecoder(0)->DecodePictureOrderCountFakeFrames(pFrame, sliceHeader);

    // mark generated frame as short-term reference
    {
        // reset frame global data
        H264DecoderMacroblockGlobalInfo *pMBInfo = ((H264DecoderFrameEx *)pFrame)->m_mbinfo.mbs;
        memset(pMBInfo, 0, pFrame->totalMBs*sizeof(H264DecoderMacroblockGlobalInfo));
    }
}

void MFX_SW_TaskSupplier::OnFullFrame(H264DecoderFrame * pFrame)
{
    //fill chroma planes in case of 4:0:0
    if (pFrame->m_chroma_format == 0)
    {
        DefaultFill(pFrame, 2, true);
    }

    MFXTaskSupplier::OnFullFrame(pFrame);
}

H264DecoderFrame * MFX_SW_TaskSupplier::GetFreeFrame(const H264Slice *pSlice)
{
    AutomaticUMCMutex guard(m_mGuard);
    uint32_t view_id = pSlice ? pSlice->GetSliceHeader()->nal_ext.mvc.view_id : 0;
    ViewItem &view = GetView(view_id);

    H264DBPList *pDPB = view.GetDPBList(0);

    H264DecoderFrame *pFrame = pDPB->GetDisposable();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= view.maxDecFrameBuffering + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameEx(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        pDPB->append(pFrame);
    }

    DecReferencePictureMarking::Remove(pFrame);
    pFrame->Reset();

    if (view.pCurFrame == pFrame)
        view.pCurFrame = 0;

    InitFreeFrame(pFrame, pSlice);

    return pFrame;
}


Status MFX_SW_TaskSupplier::AllocateFrameData(H264DecoderFrame * pFrame)
{
    mfxSize dimensions = pFrame->lumaSize();
    VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, pFrame->GetColorFormat(), pFrame->m_bpp);

    FrameMemID frmMID;
    Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts == UMC_ERR_ALLOC)
        return UMC_ERR_ALLOC;

    if (sts != UMC_OK)
    {
        throw h264_exception(UMC_ERR_ALLOC);
    }

    const FrameData *frmData = m_pFrameAllocator->Lock(frmMID);

    if (!frmData)
        throw h264_exception(UMC_ERR_LOCK);

    pFrame->allocate(frmData, &info);

    pFrame->IncrementReference();
    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;
    pFrame->m_index = frmMID;

    Status umcRes = ((H264DecoderFrameEx *)pFrame)->allocateParsedFrameData();
    return umcRes;
}

}
#endif // MFX_ENABLE_H264_VIDEO_DECODE