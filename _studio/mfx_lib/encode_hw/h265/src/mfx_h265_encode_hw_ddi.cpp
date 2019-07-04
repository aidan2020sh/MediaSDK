// Copyright (c) 2014-2019 Intel Corporation
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
#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "libmfx_core_interface.h"

#include "mfx_h265_encode_hw_ddi.h"
#if defined (MFX_VA_WIN)
#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"

#elif defined (MFX_VA_LINUX)
#include "mfx_h265_encode_vaapi.h"
#endif
#include "mfx_h265_encode_hw_ddi_trace.h"
#include "ipps.h"

namespace MfxHwH265Encode
{

GUID GetGUID(MfxVideoParam const & par)
{
    GUID guid = DXVA2_Intel_Encode_HEVC_Main;

    mfxU16 bdId = 0, cfId = 0;

#if (MFX_VERSION >= 1027)
    if (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || par.m_ext.CO3.TargetBitDepthLuma == 10)
        bdId = 1;
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    if (par.m_ext.CO3.TargetBitDepthLuma == 12)
        bdId = 2;
#endif

    cfId = mfx::clamp<mfxU16>(par.m_ext.CO3.TargetChromaFormatPlus1 - 1, MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444) - MFX_CHROMAFORMAT_YUV420;

    if (par.m_platform && par.m_platform < MFX_HW_ICL)
        cfId = 0; // platforms below ICL do not support Main422/Main444 profile, using Main instead.
#else
    if (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || par.mfx.FrameInfo.BitDepthLuma == 10 || par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        bdId = 1;

     cfId = 0;
#endif
    if (par.m_platform && par.m_platform < MFX_HW_KBL)
        bdId = 0;

    mfxU16 cFamily = IsOn(par.mfx.LowPower);

#if defined(MFX_ENABLE_HEVCE_SCC)
    cFamily = (par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC) ? 2 : IsOn(par.mfx.LowPower);
#endif

    guid = GuidTable[cFamily][bdId] [cfId];
    DDITracer::TraceGUID(guid, stdout);
    return guid;
}

DriverEncoder* CreatePlatformH265Encoder(VideoCORE* core, ENCODER_TYPE type)
{
    (void)type;

    if (core)
    {
        switch(core->GetVAType())
        {
#if defined (MFX_VA_WIN)
        case MFX_HW_D3D9:
            if (type == ENCODER_REXT)
                return new D3D9EncoderREXT;
            return new D3D9EncoderDefault;
        case MFX_HW_D3D11:
            if (type == ENCODER_REXT)
                return new D3D11EncoderREXT;
            return new D3D11EncoderDefault;
#elif defined (MFX_VA_LINUX)
        case MFX_HW_VAAPI:
            return new VAAPIEncoder;
#endif
        default:
            return nullptr;
        }
    }

    return nullptr;
}

#if defined(MFX_ENABLE_MFE) && defined (PRE_SI_TARGET_PLATFORM_GEN12P5)
#if defined(MFX_VA_WIN)
MFEDXVAEncoder* CreatePlatformMFEEncoder(VideoCORE* core)
{

    if (core)
    {
        ComPtrCore<MFEDXVAEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEDXVAEncoder> >(core, MFXMFEHEVCENCODER_SEARCH_GUID);
        if (!pVideoEncoder) return nullptr;
        if (!pVideoEncoder->get())
            *pVideoEncoder = new MFEDXVAEncoder;

        return pVideoEncoder->get();
    }

    return nullptr;
}
#else
MFEVAAPIEncoder* CreatePlatformMFEEncoder(VideoCORE* core)
{

    if (core)
    {
        ComPtrCore<MFEVAAPIEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEVAAPIEncoder> >(core, MFXMFEHEVCENCODER_SEARCH_GUID);
        if (!pVideoEncoder) return nullptr;
        if (!pVideoEncoder->get())
            *pVideoEncoder = new MFEVAAPIEncoder;

        return pVideoEncoder->get();
    }

    return nullptr;
}
#endif
#endif

// this function is aimed to workaround all CAPS reporting problems in mainline driver
mfxStatus HardcodeCaps(MFX_ENCODE_CAPS_HEVC& caps, VideoCORE* core, MfxVideoParam const &par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(core);

    eMFXHWType platform = core->GetHWType();

#if defined (MFX_VA_LINUX)
    // common part for OS and CS Linux (moved from vaapi)
    caps.ddi_caps.BlockSize = 2;   // = 1 on Win (16x16); to clarify!!!
    caps.ddi_caps.UserMaxFrameSizeSupport = 1;
    caps.ddi_caps.MbQpDataSupport         = 1; // = 0 on Win; to clarify!!!
    caps.ddi_caps.TUSupport               = 73; // 1,
    caps.ddi_caps.SliceStructure          = 4;
    caps.ddi_caps.BRCReset                = 1;  // = 0 on Win (no bitrate resolution control); to clarify!!!
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    caps.ddi_caps.HRDConformanceSupport   = platform >= MFX_HW_TGL_LP ? 1 : 0;
#endif
    // Below caps are correct on Windows
    if (!caps.ddi_caps.BitDepth8Only && !caps.ddi_caps.MaxEncodedBitDepth)
        caps.ddi_caps.MaxEncodedBitDepth = 1;    // 8/10b
    if (!caps.ddi_caps.Color420Only && !(caps.ddi_caps.YUV444ReconSupport) && IsOn(par.mfx.LowPower))
        caps.ddi_caps.YUV444ReconSupport = 1;
    if (!caps.ddi_caps.Color420Only && !(caps.ddi_caps.YUV422ReconSupport) && IsOff(par.mfx.LowPower))
        caps.ddi_caps.YUV422ReconSupport = 1;
#else
    if ((platform >= MFX_HW_KBL) && (platform < MFX_HW_CNL) && !caps.ddi_caps.MaxEncodedBitDepth)
        caps.ddi_caps.MaxEncodedBitDepth = 1;    // 10b
    if (!caps.ddi_caps.Color420Only && !(caps.ddi_caps.YUV422ReconSupport) &&
        IsOff(par.mfx.LowPower) && (platform >= MFX_HW_TGL_LP))
        caps.ddi_caps.YUV422ReconSupport = 1;    // Win VME is not fixed yet
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    if (platform >= MFX_HW_ATS && IsOn(par.mfx.LowPower))
    {
        // For now the driver reports in caps 8 pipes for ATS while in fact there are 2
        // and more pipes are not expected to be supported.
        // Delete the code when caps are fixed.
        if (caps.ddi_caps.NumScalablePipesMinus1 > 1)
            caps.ddi_caps.NumScalablePipesMinus1 = 1;
    }
#endif // defined(PRE_SI_TARGET_PLATFORM_GEN12)
#endif

#if defined(_WIN32) || defined(_WIN64)
    caps.CBRSupport = 1;
    caps.VBRSupport = 1;
    caps.CQPSupport = 1;
    caps.ICQSupport = 1;
#endif

    if (platform < MFX_HW_CNL)
    {   // not set until CNL now
        caps.ddi_caps.LCUSizeSupported = 0b10;   // 32x32 lcu is only supported
        caps.ddi_caps.BlockSize = 0b10; // 32x32
    }

    caps.PSliceSupport = (IsOn(par.mfx.LowPower) || platform > MFX_HW_ICL) ? 0 : 1;

#if defined (MFX_VA_LINUX) && !defined (OPEN_SOURCE)
    // align with Windows for Gen12+
    if (platform >= MFX_HW_TGL_LP)
    {   // taken from  Windows TGLLP (temporarily)
        caps.ddi_caps.CodingLimitSet = 1; // = 0 now but should be always set to 1 according to DDI (what about Linux ???)
        caps.ddi_caps.Color420Only = 0;  // = 1 now
        caps.ddi_caps.YUV422ReconSupport = 1; // = 0 now
        caps.ddi_caps.SliceIPBOnly = 1;  // = 0 now (SliceIP is also 0)cz
        caps.ddi_caps.NoWeightedPred = 0; // = 1 now
        caps.ddi_caps.NoMinorMVs = 1;  // = 0 now
        caps.ddi_caps.RawReconRefToggle = 1;  // = 0 now
        caps.ddi_caps.NoInterlacedField = 1;  // = 0 now
//        caps.RollingIntraRefresh = 0;  // = 1 now
//        caps.VCMBitRateControl = 0;  // = 1 now
        caps.ddi_caps.ParallelBRC = 1;  // = 0 now
        caps.ddi_caps.LumaWeightedPred = 1; // = 0 now
        caps.ddi_caps.ChromaWeightedPred = 0; // = 0 now
        caps.ddi_caps.MaxEncodedBitDepth = 2;  // = 1 now (8/10b only)
        caps.ddi_caps.MaxPicWidth = 16384;  // = 8192 now
        caps.ddi_caps.MaxPicHeight = 16384;  // = 8192 now
        caps.ddi_caps.MaxNumOfROI = 16;  // = 0 now
        caps.ddi_caps.ROIDeltaQPSupport = 1;  // = 0 now
        caps.ddi_caps.BlockSize = 1;  // = 0 now (set to 16x16 like in Win)
        caps.ddi_caps.SliceLevelReportSupport = 1;  // = 0 now
        caps.ddi_caps.FrameSizeToleranceSupport = 1;  // = 0 now
        caps.ddi_caps.NumScalablePipesMinus1 = 1;  // = 0 now
        caps.ddi_caps.MaxNum_WeightedPredL0 = 4; // = 0 now
        caps.ddi_caps.MaxNum_WeightedPredL1 = 2; // = 0 now
        caps.ddi_caps.TileSupport = 1;
        caps.ddi_caps.IntraRefreshBlockUnitSize = 2;
    }
#endif

    return sts;
}

mfxStatus QueryMbProcRate(VideoCORE* core, mfxVideoParam const & par, mfxU32(&mbPerSec)[16], const MfxVideoParam * in)
{
    GUID guid = GetGUID(*in);
    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(core, MFXIHWMBPROCRATE_GUID);
    if (!pEncodeCaps)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    else
    {
        if (pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, 16) == MFX_ERR_NONE &&
            mbPerSec[(par.mfx.TargetUsage ? par.mfx.TargetUsage : 4) - 1] != 0) //check if MbPerSec for particular TU was already queried or need to save
            return MFX_ERR_NONE;
    }

    std::unique_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH265Encoder(core));
    if (ddi.get() == 0)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, *in);
    MFX_CHECK_STS(sts);

    mfxU32 tempMbPerSec[16] = { 0, };
    sts = ddi->QueryMbPerSec(par, tempMbPerSec);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_1, tempMbPerSec[0]);
    MFX_CHECK_STS(sts);

    mbPerSec[(par.mfx.TargetUsage ? par.mfx.TargetUsage : 4) - 1] = tempMbPerSec[0];
    sts = pEncodeCaps->SetHWCaps<mfxU32>(guid, mbPerSec, 16);

    return sts;
}

mfxStatus QueryHwCaps(VideoCORE* core, GUID guid, MFX_ENCODE_CAPS_HEVC & caps, MfxVideoParam const & par){
    std::unique_ptr<DriverEncoder> ddi;

    MFX_CHECK_NULL_PTR1(core);
    ddi.reset(CreatePlatformH265Encoder(core));
    MFX_CHECK(ddi.get(), MFX_ERR_UNSUPPORTED);

    mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, guid, par);
    MFX_CHECK_STS(sts);

    sts = ddi.get()->QueryEncodeCaps(caps);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus CheckHeaders(
    MfxVideoParam const & par,
    MFX_ENCODE_CAPS_HEVC const & caps)
{
    MFX_CHECK_COND(
           par.m_sps.log2_min_luma_coding_block_size_minus3 == 0
        && par.m_sps.separate_colour_plane_flag == 0
        && par.m_sps.pcm_enabled_flag == 0);

#if (MFX_VERSION >= 1025)
    if (par.m_platform >= MFX_HW_CNL)
    {
        MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 1);
    }
    else
#endif
    {
        MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 0);
        MFX_CHECK_COND(par.m_sps.sample_adaptive_offset_enabled_flag == 0);
    }

#if (MFX_VERSION < 1027)
    MFX_CHECK_COND(par.m_pps.tiles_enabled_flag == 0);
#endif

#if (MFX_VERSION >= 1027)
    MFX_CHECK_COND(
      !(   (!caps.ddi_caps.YUV444ReconSupport && (par.m_sps.chroma_format_idc == 3))
        || (!caps.ddi_caps.YUV422ReconSupport && (par.m_sps.chroma_format_idc == 2))
        || (caps.ddi_caps.Color420Only && (par.m_sps.chroma_format_idc != 1))));

    MFX_CHECK_COND(caps.ddi_caps.NumScalablePipesMinus1 == 0 || par.m_pps.num_tile_columns_minus1 <= caps.ddi_caps.NumScalablePipesMinus1);

    if (par.m_pps.tiles_enabled_flag)
    {
        MFX_CHECK_COND(par.m_pps.loop_filter_across_tiles_enabled_flag);
    }
#else
    MFX_CHECK_COND(par.m_sps.chroma_format_idc == 1);
#endif

    MFX_CHECK_COND(
      !(   par.m_sps.pic_width_in_luma_samples > caps.ddi_caps.MaxPicWidth
        || par.m_sps.pic_height_in_luma_samples > caps.ddi_caps.MaxPicHeight
        || (UINT)(((par.m_pps.num_tile_columns_minus1 + 1) * (par.m_pps.num_tile_rows_minus1 + 1)) > 1) > caps.ddi_caps.TileSupport));

    MFX_CHECK_COND(
      !(   (caps.ddi_caps.MaxEncodedBitDepth == 0 || caps.ddi_caps.BitDepth8Only)
        && (par.m_sps.bit_depth_luma_minus8 != 0 || par.m_sps.bit_depth_chroma_minus8 != 0)));

    MFX_CHECK_COND(
      !(   (caps.ddi_caps.MaxEncodedBitDepth == 2 || caps.ddi_caps.MaxEncodedBitDepth == 1 || !caps.ddi_caps.BitDepth8Only)
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4))));

    MFX_CHECK_COND(
      !(   caps.ddi_caps.MaxEncodedBitDepth == 2
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4))));

    MFX_CHECK_COND(
      !(   caps.ddi_caps.MaxEncodedBitDepth == 3
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4
            || par.m_sps.bit_depth_luma_minus8 == 8)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4
            || par.m_sps.bit_depth_chroma_minus8 == 8))));

    return MFX_ERR_NONE;
}

mfxU16 CodingTypeToSliceType(mfxU16 ct)
{
    switch (ct)
    {
     case CODING_TYPE_I : return 2;
     case CODING_TYPE_P : return 1;
     case CODING_TYPE_B :
     case CODING_TYPE_B1:
     case CODING_TYPE_B2: return 0;
    default: assert(!"invalid coding type"); return 0xFFFF;
    }
}

#if defined (MFX_ENABLE_HEVCE_ROI) || defined (MFX_ENABLE_HEVCE_DIRTY_RECT)
bool SkipRectangle(RectData* rect)
{
    mfxU16 changed = 0;

    if (rect->Left >= rect->Right || rect->Top >= rect->Bottom)
        changed++;

    return changed;
}
#endif

DDIHeaderPacker::DDIHeaderPacker()
{
}

DDIHeaderPacker::~DDIHeaderPacker()
{
}

void DDIHeaderPacker::Reset(MfxVideoParam const & par)
{
    m_buf.resize(6 + par.mfx.NumSlice);
    m_cur = m_buf.begin();
    m_packer.Reset(par);
}

void DDIHeaderPacker::ResetPPS(MfxVideoParam const & par)
{
    assert(!m_buf.empty());

    m_packer.ResetPPS(par);
}

void DDIHeaderPacker::NewHeader()
{
    assert(m_buf.size());

    if (++m_cur == m_buf.end())
        m_cur = m_buf.begin();

    Zero(*m_cur);
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackHeader(Task const & task, mfxU32 nut)
{
    NewHeader();

    switch(nut)
    {
    case VPS_NUT:
        m_packer.GetVPS(m_cur->pData, m_cur->DataLength);
        break;
    case SPS_NUT:
        m_packer.GetSPS(m_cur->pData, m_cur->DataLength);
        break;
    case PPS_NUT:
        m_packer.GetPPS(m_cur->pData, m_cur->DataLength);
        break;
    case AUD_NUT:
        {
            mfxU32 frameType = task.m_frameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);

            if (frameType == MFX_FRAMETYPE_I)
#if defined(MFX_ENABLE_HEVCE_SCC)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, task.m_isSCC ? 1 : 0);
#else
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 0);
#endif
            else if (frameType == MFX_FRAMETYPE_P)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 1);
            else
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 2);
        }
        break;
    case PREFIX_SEI_NUT:
        m_packer.GetPrefixSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    case SUFFIX_SEI_NUT:
        m_packer.GetSuffixSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    default:
        return 0;
    }
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 4;

    return &*m_cur;
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackSliceHeader(
    Task const & task,
    mfxU32 id,
    mfxU32* qpd_offset,
    bool dyn_slice_size,
    mfxU32* sao_offset,
    mfxU16* ssh_start_len,
    mfxU32* ssh_offset,
    mfxU32* pwt_offset,
    mfxU32* pwt_length)
{
    bool is1stNALU = (id == 0 && task.m_insertHeaders == 0);
    NewHeader();

    m_packer.GetSSH(task, id, m_cur->pData, m_cur->DataLength, qpd_offset, dyn_slice_size, sao_offset, ssh_start_len, ssh_offset, pwt_offset, pwt_length);
    m_cur->BufferSize = CeilDiv(m_cur->DataLength, 8);
    m_cur->SkipEmulationByteCount = 3 + is1stNALU;

    return &*m_cur;
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackSkippedSlice(Task const & task, mfxU32 id, mfxU32* qpd_offset)
{
    bool is1stNALU = (id == 0 && task.m_insertHeaders == 0);
    NewHeader();

    m_packer.GetSkipSlice(task, id, m_cur->pData, m_cur->DataLength, qpd_offset);
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 3 + is1stNALU;
    m_cur->DataLength *= 8;

    return &*m_cur;
}
#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, VideoCORE& core, mfxFrameInfo &CUQPFrameInfo)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (!task.m_bCUQPMap || (core.GetVAType() == MFX_HW_VAAPI))
        return MFX_ERR_NONE;

    mfxExtMBQP *mbqp = ExtBuffer::Get(task.m_ctrl);
#ifdef MFX_ENABLE_HEVCE_ROI
    mfxExtEncoderROI* roi = ExtBuffer::Get(task.m_ctrl);
#endif

    MFX_CHECK(CUQPFrameInfo.Width && CUQPFrameInfo.Height, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(CUQPFrameInfo.AspectRatioW && CUQPFrameInfo.AspectRatioH, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 drBlkW  = CUQPFrameInfo.AspectRatioW;  // block size of driver
    mfxU32 drBlkH  = CUQPFrameInfo.AspectRatioH;  // block size of driver
    mfxU16 inBlkSize = 16;                            //mbqp->BlockSize ? mbqp->BlockSize : 16;  //input block size

    mfxU32 pitch_MBQP = (par.mfx.FrameInfo.Width  + inBlkSize - 1)/ inBlkSize;

    if (mbqp && mbqp->NumQPAlloc)
    {
        if ((mbqp->NumQPAlloc *  inBlkSize *  inBlkSize) <
            (drBlkW  *  drBlkH  *  CUQPFrameInfo.Width  *  CUQPFrameInfo.Height))
        {
            task.m_bCUQPMap = false;
            return  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        FrameLocker lock(&core, task.m_midCUQp);
        MFX_CHECK(lock.Y, MFX_ERR_LOCK_MEMORY);

        for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
            for (mfxU32 j = 0; j < CUQPFrameInfo.Width; j++)
                    lock.Y[i * lock.Pitch + j] = mbqp->QP[i*drBlkH/inBlkSize * pitch_MBQP + j*drBlkW/inBlkSize];

    }
#ifdef MFX_ENABLE_HEVCE_ROI
    else if (roi)
    {
        FrameLocker lock(&core, task.m_midCUQp);
        MFX_CHECK(lock.Y, MFX_ERR_LOCK_MEMORY);
        for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
        {
            for (mfxU32 j = 0; j < CUQPFrameInfo.Width; j++)
            {
                mfxU8 qp = (mfxU8)task.m_qpY;
                mfxI32 diff = 0;
                for (mfxU32 n = 0; n < roi->NumROI; n++)
                {
                    mfxU32 x = i*drBlkW;
                    mfxU32 y = i*drBlkH;
                    if (x >= roi->ROI[n].Left  &&  x < roi->ROI[n].Right  && y >= roi->ROI[n].Top && y < roi->ROI[n].Bottom)
                    {
                        diff = (task.m_roiMode == MFX_ROI_MODE_PRIORITY ? (-1) : 1) * roi->ROI[n].Priority;
                        break;
                    }

                }
                lock.Y[i * lock.Pitch + j] = (mfxU8)(qp + diff);
            }
        }
    }
#endif
    return mfxSts;

}
#endif

#if defined(_WIN32) || defined(_WIN64)

void FeedbackStorage::Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices)
{
    if ((fbType != QUERY_STATUS_PARAM_FRAME) &&
        (fbType != QUERY_STATUS_PARAM_SLICE)) {
        assert(!"unknown query function");
        fbType = QUERY_STATUS_PARAM_FRAME;
    }

    m_type = fbType;
    m_pool_size = cacheSize;
    m_fb_size = (m_type == QUERY_STATUS_PARAM_SLICE) ? sizeof(ENCODE_QUERY_STATUS_SLICE_PARAMS) : sizeof(ENCODE_QUERY_STATUS_PARAMS);
    m_buf.resize(m_fb_size * m_pool_size);
    m_buf_cache.resize(m_fb_size * m_pool_size);

    for (size_t i = 0; i < m_pool_size; i++) {
        Feedback *c = (Feedback *)&m_buf_cache[i * m_fb_size];
        c->bStatus = ENCODE_NOTAVAILABLE;
    }

    if (m_type == QUERY_STATUS_PARAM_SLICE) {
        m_ssizes.resize(maxSlices * m_pool_size);
        m_ssizes_cache.resize(maxSlices * m_pool_size);
        for (size_t i = 0; i < m_pool_size; i++) {
            ENCODE_QUERY_STATUS_SLICE_PARAMS *pSliceInfo = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)&m_buf[i * m_fb_size];
            ENCODE_QUERY_STATUS_SLICE_PARAMS *pSliceInfoCache = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)&m_buf_cache[i * m_fb_size];
            pSliceInfo->SizeOfSliceSizesBuffer = maxSlices;
            pSliceInfoCache->SizeOfSliceSizesBuffer = maxSlices;
            pSliceInfo->pSliceSizes = &m_ssizes[i * maxSlices];
            pSliceInfoCache->pSliceSizes = &m_ssizes_cache[i * maxSlices];
        }
    }
}

const FeedbackStorage::Feedback* FeedbackStorage::Get(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_pool_size; i++) {
        Feedback *pFb = (Feedback *)&m_buf_cache[i * m_fb_size];
        if (pFb->StatusReportFeedbackNumber == feedbackNumber)
            return pFb;
    }
    return 0;
}

mfxStatus FeedbackStorage::Update()
{
    for (size_t i = 0; i < m_pool_size; i++) {
        Feedback *u = (Feedback *)&m_buf[i * m_fb_size];
        if (u->bStatus != ENCODE_NOTAVAILABLE) {
            Feedback *hit = 0;
            for (size_t j = 0; j < m_pool_size; j++) {
                Feedback *c = (Feedback *)&m_buf_cache[j * m_fb_size];
                if (c->StatusReportFeedbackNumber == u->StatusReportFeedbackNumber) {
                    hit = c;  // hit
                    break;
                }
                else if (hit == 0 && c->bStatus == ENCODE_NOTAVAILABLE) {
                    hit = c;  // first free
                }
            }
            MFX_CHECK(hit != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            CacheFeedback(hit, u);
        }
    }
    return MFX_ERR_NONE;
}

// copy fb into cache
inline void FeedbackStorage::CacheFeedback(Feedback *fb_dst, Feedback *fb_src)
{
    *fb_dst = *fb_src;
    if (m_type == QUERY_STATUS_PARAM_SLICE) {
        ENCODE_QUERY_STATUS_SLICE_PARAMS *pdst = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_dst;
        ENCODE_QUERY_STATUS_SLICE_PARAMS *psrc = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_src;
        std::copy(psrc->pSliceSizes, psrc->pSliceSizes + psrc->FrameLevelStatus.NumberSlices, pdst->pSliceSizes);
    }
}

mfxStatus FeedbackStorage::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_pool_size; i++) {
        Feedback *c = (Feedback *)&m_buf_cache[i * m_fb_size];
        if (c->StatusReportFeedbackNumber == feedbackNumber) {
            c->bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

ENCODE_FRAME_SIZE_TOLERANCE ConvertLowDelayBRCMfx2Ddi(mfxU16 type)
{
    switch (type) {
    case MFX_CODINGOPTION_ON:
        return eFrameSizeTolerance_ExtremelyLow;
    default:
        return eFrameSizeTolerance_Normal;
    }
}

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    slice.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id              = i;
        cs.slice_segment_address = par.m_slice[i].SegmentAddress;
        cs.NumLCUsInSlice        = par.m_slice[i].NumLCU;
        cs.bLastSliceOfPic       = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & pps,
    bool bLast,
    ENCODE_SET_SLICE_HEADER_HEVC & cs)
{
    cs.slice_type                           = task.m_sh.type;

#if defined(MFX_ENABLE_HEVCE_SCC)
    if ((task.m_frameType & MFX_FRAMETYPE_I) && task.m_isSCC) {
        for (int i = 0; i < 15; i++) cs.RefPicList[0][i].bPicEntry = cs.RefPicList[1][i].bPicEntry = 0xFF;
        cs.num_ref_idx_l0_active_minus1 = 0;
    } else
#endif
    if (cs.slice_type != 2)
    {
        const mfxU8 *src = reinterpret_cast <const mfxU8*> (&task.m_refPicList[0]);
        mfxU8 *dst = reinterpret_cast <mfxU8*>(&cs.RefPicList[0]);
        std::copy(src, src + sizeof(task.m_refPicList), dst);

        cs.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

        if (cs.slice_type == 0)
            cs.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
    }

    cs.bLastSliceOfPic                      = bLast;
    cs.dependent_slice_segment_flag         = task.m_sh.dependent_slice_segment_flag;
    cs.slice_temporal_mvp_enable_flag       = task.m_sh.temporal_mvp_enabled_flag;
    cs.slice_sao_luma_flag                  = task.m_sh.sao_luma_flag;
    cs.slice_sao_chroma_flag                = task.m_sh.sao_chroma_flag;
    cs.mvd_l1_zero_flag                     = task.m_sh.mvd_l1_zero_flag;
    cs.cabac_init_flag                      = task.m_sh.cabac_init_flag;
    cs.slice_deblocking_filter_disable_flag = task.m_sh.deblocking_filter_disabled_flag;
    cs.collocated_from_l0_flag              = task.m_sh.collocated_from_l0_flag;

    cs.slice_qp_delta       = task.m_sh.slice_qp_delta;
    cs.slice_cb_qp_offset   = task.m_sh.slice_cb_qp_offset;
    cs.slice_cr_qp_offset   = task.m_sh.slice_cr_qp_offset;
    cs.beta_offset_div2     = task.m_sh.beta_offset_div2;
    cs.tc_offset_div2       = task.m_sh.tc_offset_div2;

#if defined(MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
    if (cs.slice_type != 2 && (pps.weighted_pred_flag || pps.weighted_bipred_flag))
    {
        const mfxU16 Y = 0, Cb = 1, Cr = 2, W = 0, O = 1;

        cs.luma_log2_weight_denom           = (UCHAR)task.m_sh.luma_log2_weight_denom;
        cs.delta_chroma_log2_weight_denom   = (CHAR)(task.m_sh.chroma_log2_weight_denom - cs.luma_log2_weight_denom);

        mfxI16 wY = (1 << cs.luma_log2_weight_denom);
        mfxI16 wC = (1 << task.m_sh.chroma_log2_weight_denom);

        for (mfxU16 l = 0; l < 2; l++)
        {
            for (mfxU16 i = 0; i < 15; i++)
            {
                cs.luma_offset[l][i]            = (CHAR)(task.m_sh.pwt[l][i][Y][O]);
                cs.delta_luma_weight[l][i]      = (CHAR)(task.m_sh.pwt[l][i][Y][W] - wY);
                cs.chroma_offset[l][i][0]       = (CHAR)(task.m_sh.pwt[l][i][Cb][O]);
                cs.chroma_offset[l][i][1]       = (CHAR)(task.m_sh.pwt[l][i][Cr][O]);
                cs.delta_chroma_weight[l][i][0] = (CHAR)(task.m_sh.pwt[l][i][Cb][W] - wC);
                cs.delta_chroma_weight[l][i][1] = (CHAR)(task.m_sh.pwt[l][i][Cr][W] - wC);
            }
        }
    }
#else
    pps;
#endif //defined(MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)

    cs.MaxNumMergeCand = 5 - task.m_sh.five_minus_max_num_merge_cand;
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & pps,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        FillSliceBuffer(task, pps, (i == slice.size() - 1), slice[i]);
    }
}

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps)
{
    Zero(sps);

    sps.log2_min_coding_block_size_minus3 = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;

    sps.wFrameWidthInMinCbMinus1 = (mfxU16)(par.m_sps.pic_width_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.wFrameHeightInMinCbMinus1 = (mfxU16)(par.m_sps.pic_height_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.general_profile_idc = par.m_sps.general.profile_idc;
    sps.general_level_idc   = par.m_sps.general.level_idc;
    sps.general_tier_flag   = par.m_sps.general.tier_flag;

    mfxU8 nPicturesPerFrame = par.isField() ? 2 : 1;
    sps.GopPicSize          = par.mfx.GopPicSize * nPicturesPerFrame;
    sps.GopRefDist          = mfxU8(par.mfx.GopRefDist) * nPicturesPerFrame;
    sps.GopOptFlag          = mfxU8(par.mfx.GopOptFlag);

    sps.TargetUsage         = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod   = mfxU8(par.isSWBRC() ? MFX_RATECONTROL_CQP:par.mfx.RateControlMethod) ;
    if(par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        sps.ContentInfo = eContent_NonVideoScreen;
    }
    if (   par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        sps.MinBitRate      = 0;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.MaxKbps;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        sps.MinBitRate      = par.TargetKbps;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.TargetKbps;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        sps.TargetBitRate   = par.TargetKbps;
//        sps.AVBRAccuracy = par.mfx.Accuracy;
//        sps.AVBRConvergence = par.mfx.Convergence;
    }

    sps.FrameRate.Numerator     = par.mfx.FrameInfo.FrameRateExtN;
    sps.FrameRate.Denominator   = par.mfx.FrameInfo.FrameRateExtD;
    sps.InitVBVBufferFullnessInBit = 8000 * par.InitialDelayInKB;
    sps.VBVBufferSizeInBit         = 8000 * par.BufferSizeInKB;

    sps.bResetBRC        = 0;
    sps.GlobalSearch     = 0;
    sps.LocalSearch      = 0;
    sps.EarlySkip        = 0;
    sps.MBBRC            = IsOn(par.m_ext.CO2.MBBRC) ? 1 : IsOff(par.m_ext.CO2.MBBRC) ? 2 : 1; /*it should be zero by default (MBBRC == UNKNOWN), but to save previous behavior MBBRC is on by default */

    //WA:  Parallel BRC is switched on in sync & async mode (quality drop in noParallelBRC in driver)
    sps.ParallelBRC = (sps.GopRefDist > 1) && (sps.GopRefDist <= 8) && par.isBPyramid() && !IsOn(par.mfx.LowPower);

    if (par.m_ext.CO2.MaxSliceSize)
        sps.SliceSizeControl = 1;

    sps.UserMaxIFrameSize = par.m_ext.CO2.MaxFrameSize;
    sps.UserMaxPBFrameSize = par.m_ext.CO2.MaxFrameSize;

    sps.FrameSizeTolerance = ConvertLowDelayBRCMfx2Ddi(par.m_ext.CO3.LowDelayBRC);

    if ((par.mfx.RateControlMethod == MFX_RATECONTROL_VBR || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR) && par.m_ext.CO3.WinBRCSize)
        sps.FrameSizeTolerance = eFrameSizeTolerance_Low; //sliding window

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        sps.ICQQualityFactor = (mfxU8)par.mfx.ICQQuality;
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
        sps.ICQQualityFactor = (mfxU8)par.m_ext.CO3.QVBRQuality;
    else
        sps.ICQQualityFactor = 0;

    if (sps.ParallelBRC)
    {
        if (!par.isBPyramid())
        {
            sps.NumOfBInGop[0]  = (par.mfx.GopPicSize - 1) - (par.mfx.GopPicSize - 1)/par.mfx.GopRefDist;
            sps.NumOfBInGop[1]  = 0;
            sps.NumOfBInGop[2]  = 0;
        }
        else if (par.mfx.GopRefDist <= 8)
        {
            static UINT B[9]  = {0,0,1,1,1,1,1,1,1};
            static UINT B1[9] = {0,0,0,1,2,2,2,2,2};
            static UINT B2[9] = {0,0,0,0,0,1,2,3,4};

            mfxI32 numBpyr   = par.mfx.GopPicSize/par.mfx.GopRefDist;
            mfxI32 lastBpyrW = par.mfx.GopPicSize%par.mfx.GopRefDist;

            sps.NumOfBInGop[0]  = numBpyr*B[par.mfx.GopRefDist] + B[lastBpyrW];
            sps.NumOfBInGop[1]  = numBpyr*B1[par.mfx.GopRefDist]+ B1[lastBpyrW];
            sps.NumOfBInGop[2]  = numBpyr*B2[par.mfx.GopRefDist]+ B2[lastBpyrW];
        }
        else
        {
            assert(0);
        }
    }

    switch (par.mfx.FrameInfo.BitDepthLuma)
    {
    case 16: sps.SourceBitDepth = 3; break;
    case 12: sps.SourceBitDepth = 2; break;
    case 10: sps.SourceBitDepth = 1; break;
    default: assert(!"undefined SourceBitDepth");
    case  8: sps.SourceBitDepth = 0; break;
    }

    if (   par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4
        || par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10)
    {
        sps.SourceFormat = 3;
    }
    else
    {
        assert(par.mfx.FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV400 && par.mfx.FrameInfo.ChromaFormat <= MFX_CHROMAFORMAT_YUV444);
        sps.SourceFormat = par.mfx.FrameInfo.ChromaFormat - 1;
    }

    sps.scaling_list_enable_flag           = par.m_sps.scaling_list_enabled_flag;
    sps.sps_temporal_mvp_enable_flag       = par.m_sps.temporal_mvp_enabled_flag;
    sps.strong_intra_smoothing_enable_flag = par.m_sps.strong_intra_smoothing_enabled_flag;
    sps.amp_enabled_flag                   = par.m_sps.amp_enabled_flag;    // SKL: 0, CNL+: 1
    sps.SAO_enabled_flag                   = par.m_sps.sample_adaptive_offset_enabled_flag; // 0, 1
    sps.pcm_enabled_flag                   = par.m_sps.pcm_enabled_flag;
    sps.pcm_loop_filter_disable_flag       = 1;//par.m_sps.pcm_loop_filter_disabled_flag;
    sps.chroma_format_idc                  = par.m_sps.chroma_format_idc;
    sps.separate_colour_plane_flag         = par.m_sps.separate_colour_plane_flag;

    sps.log2_max_coding_block_size_minus3       = (mfxU8)(par.m_sps.log2_min_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_luma_coding_block_size);
    sps.log2_min_coding_block_size_minus3       = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;
    sps.log2_max_transform_block_size_minus2    = (mfxU8)(par.m_sps.log2_min_transform_block_size_minus2
        + par.m_sps.log2_diff_max_min_transform_block_size);
    sps.log2_min_transform_block_size_minus2    = (mfxU8)par.m_sps.log2_min_transform_block_size_minus2;
    sps.max_transform_hierarchy_depth_intra     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_intra;
    sps.max_transform_hierarchy_depth_inter     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_inter;
    sps.log2_min_PCM_cb_size_minus3             = (mfxU8)par.m_sps.log2_min_pcm_luma_coding_block_size_minus3;
    sps.log2_max_PCM_cb_size_minus3             = (mfxU8)(par.m_sps.log2_min_pcm_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_pcm_luma_coding_block_size);
    sps.bit_depth_luma_minus8                   = (mfxU8)par.m_sps.bit_depth_luma_minus8;
    sps.bit_depth_chroma_minus8                 = (mfxU8)par.m_sps.bit_depth_chroma_minus8;
    sps.pcm_sample_bit_depth_luma_minus1        = (mfxU8)par.m_sps.pcm_sample_bit_depth_luma_minus1;
    sps.pcm_sample_bit_depth_chroma_minus1      = (mfxU8)par.m_sps.pcm_sample_bit_depth_chroma_minus1;

    if (par.m_ext.ROI.NumROI && !par.bROIViaMBQP) {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP || par.isSWBRC()) {
            sps.ROIValueInDeltaQP = 1;
        } else {
            sps.ROIValueInDeltaQP = 0;  // 0 means Priorities (if supported in caps)
#if MFX_VERSION > 1021
            if(par.m_ext.ROI.ROIMode == MFX_ROI_MODE_QP_DELTA)
                sps.ROIValueInDeltaQP = 1;
#endif // MFX_VERSION > 1021
        }
    }

#if (MFX_VERSION >= 1025)
    if (par.m_platform >= MFX_HW_CNL)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            sps.QpAdjustment = IsOn(par.m_ext.DDI.QpAdjust) ? 1 : 0;
        }
    }
#endif //(MFX_VERSION >= 1025)

#if defined(MFX_ENABLE_HEVCE_SCC)
    sps.palette_mode_enabled_flag = par.m_sps.palette_mode_enabled_flag;
#endif

    sps.DisableHRDConformance = IsOff(par.m_ext.CO3.BRCPanicMode);

    if (par.m_ext.CO3.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
    {
        sps.ScenarioInfo = eScenario_GameStreaming;
    }
    else if (par.m_ext.CO3.ScenarioInfo == MFX_SCENARIO_REMOTE_GAMING)
    {
        sps.ScenarioInfo = eScenario_RemoteGaming;
    }

    // QpModulation support
    if ((par.m_platform >= MFX_HW_ICL) && (par.m_platform < MFX_HW_TGL_LP))
    {
        if (par.mfx.GopRefDist == 1)
            sps.LowDelayMode = 1;

        if ((par.m_ext.CO2.BRefType == MFX_B_REF_PYRAMID) &&
            ((par.mfx.GopRefDist == 4) || (par.mfx.GopRefDist == 8)))
            sps.HierarchicalFlag = 1;
    }
}

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    Zero(pps);

    pps.tiles_enabled_flag               = par.m_pps.tiles_enabled_flag;
    pps.entropy_coding_sync_enabled_flag = par.m_pps.entropy_coding_sync_enabled_flag;
    pps.sign_data_hiding_flag            = par.m_pps.sign_data_hiding_enabled_flag;
    pps.constrained_intra_pred_flag      = par.m_pps.constrained_intra_pred_flag;
    pps.transform_skip_enabled_flag      = par.m_pps.transform_skip_enabled_flag;
    pps.transquant_bypass_enabled_flag   = par.m_pps.transquant_bypass_enabled_flag;
    pps.cu_qp_delta_enabled_flag         = par.m_pps.cu_qp_delta_enabled_flag;
    pps.weighted_pred_flag               = par.m_pps.weighted_pred_flag;
    pps.weighted_bipred_flag             = par.m_pps.weighted_pred_flag;

#if defined(MFX_ENABLE_HEVCE_FADE_DETECTION)
    pps.bEnableGPUWeightedPrediction     = IsOn(par.m_ext.CO3.FadeDetection) && (pps.weighted_pred_flag || pps.weighted_bipred_flag);
#else
    pps.bEnableGPUWeightedPrediction     = 0;
#endif //defined(MFX_ENABLE_HEVCE_FADE_DETECTION)

    pps.loop_filter_across_slices_flag        = par.m_pps.loop_filter_across_slices_enabled_flag;
    pps.loop_filter_across_tiles_flag         = par.m_pps.loop_filter_across_tiles_enabled_flag;
#ifdef MFX_ENABLE_HEVC_CUSTOM_QMATRIX
    pps.scaling_list_data_present_flag = (par.m_sps.scaling_list_enabled_flag && par.m_sps.scaling_list_data_present_flag);
#else
    pps.scaling_list_data_present_flag        = par.m_pps.scaling_list_data_present_flag;
#endif
    pps.dependent_slice_segments_enabled_flag = par.m_pps.dependent_slice_segments_enabled_flag;
    pps.bLastPicInSeq                         = 0;
    pps.bLastPicInStream                      = 0;
    pps.bUseRawPicForRef                      = 0;
    pps.bEmulationByteInsertion               = 0; //!!!
    pps.bEnableRollingIntraRefresh            = 0;
    pps.BRCPrecision                          = 0;
    //pps.bScreenContent                        = 0;
    //pps.no_output_of_prior_pics_flag          = ;
    //pps.XbEnableRollingIntraRefreshX

    pps.QpY = mfxI8(par.m_pps.init_qp_minus26 + 26);

    pps.diff_cu_qp_delta_depth  = (mfxU8)par.m_pps.diff_cu_qp_delta_depth;
    pps.pps_cb_qp_offset        = (mfxU8)par.m_pps.cb_qp_offset;
    pps.pps_cr_qp_offset        = (mfxU8)par.m_pps.cr_qp_offset;
    pps.num_tile_columns_minus1 = (mfxU8)par.m_pps.num_tile_columns_minus1;
    pps.num_tile_rows_minus1    = (mfxU8)par.m_pps.num_tile_rows_minus1;

    std::copy(std::begin(par.m_pps.column_width), std::end(par.m_pps.column_width), std::begin(pps.tile_column_width));
    std::copy(std::begin(par.m_pps.row_height), std::end(par.m_pps.row_height), std::begin(pps.tile_row_height));
    pps.log2_parallel_merge_level_minus2 = (mfxU8)par.m_pps.log2_parallel_merge_level_minus2;

    pps.num_ref_idx_l0_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l1_default_active_minus1;

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.NumSlices                  = par.mfx.NumSlice;

    pps.bEnableSliceLevelReport =
#if defined (MFX_ENABLE_HEVCE_UNITS_INFO)
        IsOn(par.m_ext.CO3.EncodedUnitsInfo) ? 1 :
#endif
    0;

    if (par.m_ext.CO2.MaxSliceSize)
        pps.MaxSliceSizeInBytes = par.m_ext.CO2.MaxSliceSize;

    // Max/Min QP settings for BRC

    /*if (par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        pps.bScreenContent = 1;
    }*/

#ifdef MFX_ENABLE_HEVCE_ROI
    // ROI
    pps.NumROI = (par.bROIViaMBQP) ? 0 : (mfxU8)par.m_ext.ROI.NumROI;
    if (pps.NumROI)
    {
        mfxU32 blkSize = 1 << (caps.BlockSize + 3);
        for (mfxU16 i = 0; i < pps.NumROI; i ++)
        {
            RectData *rect = (RectData*)(&(par.m_ext.ROI.ROI[i]));
            if (SkipRectangle(rect))
                continue;
            // trimming should be done by driver
            pps.ROI[i].Roi.Left = (mfxU16)(rect->Left / blkSize);
            pps.ROI[i].Roi.Top = (mfxU16)(rect->Top / blkSize);
            // Driver expects a rect with the 'close' right bottom edge but
            // MSDK uses the 'open' edge rect, thus the right bottom edge which
            // is decreased by 1 below converts 'open' -> 'close' notation
            // We expect here boundaries are already aligned with the BlockSize
            // and Right > Left and Bottom > Top
            pps.ROI[i].Roi.Right = (mfxU16)(rect->Right / blkSize) - 1;
            pps.ROI[i].Roi.Bottom = (mfxU16)(rect->Bottom / blkSize) - 1;
            pps.ROI[i].PriorityLevelOrDQp = (mfxI8)((par.m_ext.ROI.ROIMode == MFX_ROI_MODE_PRIORITY ? (-1) : 1) * par.m_ext.ROI.ROI[i].DeltaQP);
        }
        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif // MFX_ENABLE_HEVCE_ROI

    pps.DisplayFormatSwizzle = (par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10) ||
                               (par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4);

#if defined(MFX_ENABLE_HEVCE_SCC)
    pps.pps_curr_pic_ref_enabled_flag = par.m_pps.curr_pic_ref_enabled_flag;
#endif
}

void FillPpsBuffer(
    Task const & task,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps,
    std::vector<ENCODE_RECT> & dirtyRects)
{
    pps.CurrOriginalPic.Index7Bits      = task.m_idxRec;
    pps.CurrOriginalPic.AssociatedFlag  = !!(task.m_frameType & MFX_FRAMETYPE_REF);

    pps.CurrReconstructedPic.Index7Bits     = task.m_idxRec;
    pps.CurrReconstructedPic.AssociatedFlag = !!task.m_ltr;

    if (task.m_sh.temporal_mvp_enabled_flag)
        pps.CollocatedRefPicIndex = task.m_refPicList[!task.m_sh.collocated_from_l0_flag][task.m_sh.collocated_ref_idx];
    else
        pps.CollocatedRefPicIndex = IDX_INVALID;

    for (mfxU16 i = 0; i < 15; i ++)
    {
        pps.RefFrameList[i].bPicEntry      = task.m_dpb[0][i].m_idxRec;

        // Value 1 mitigates some Windows driver bug
        pps.RefFrameList[i].AssociatedFlag = task.m_dpb[0][i].m_idxRec == IDX_INVALID ? 1 : !!task.m_dpb[0][i].m_ltr;

        pps.RefFramePOCList[i]             = task.m_dpb[0][i].m_poc;
    }

#ifdef MFX_ENABLE_HEVCE_ROI
    // ROI
    pps.NumROI = (mfxU8)task.m_numRoi;
    if (pps.NumROI) {
        mfxU32 blkSize = 1 << (caps.BlockSize + 3);
        for (mfxU16 i = 0; i < pps.NumROI; i++)
        {
            RectData *rect = (RectData*)(&(task.m_roi[i]));
            if (SkipRectangle(rect))
                continue;
            // trimming should be done by driver
            pps.ROI[i].Roi.Left = (mfxU16)(rect->Left / blkSize);
            pps.ROI[i].Roi.Top = (mfxU16)(rect->Top / blkSize);
            // Driver expects a rect with the 'close' right bottom edge but
            // MSDK uses the 'open' edge rect, thus the right bottom edge which
            // is decreased by 1 below converts 'open' -> 'close' notation
            // We expect here boundaries are already aligned with the BlockSize
            // and Right > Left and Bottom > Top
            pps.ROI[i].Roi.Right = (mfxU16)(rect->Right / blkSize) - 1;
            pps.ROI[i].Roi.Bottom = (mfxU16)(rect->Bottom / blkSize) - 1;
            pps.ROI[i].PriorityLevelOrDQp = (mfxI8)((task.m_roiMode == MFX_ROI_MODE_PRIORITY ? (-1): 1) * task.m_roi[i].DeltaQP);
        }

        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif // MFX_ENABLE_HEVCE_ROI

#ifdef MFX_ENABLE_HEVCE_DIRTY_RECT
    // DirtyRect
    pps.NumDirtyRects = task.m_numDirtyRect;
    if (task.m_numDirtyRect) {
        dirtyRects.resize(task.m_numDirtyRect);
        mfxU32 blkSize = 1 << (caps.BlockSize + 3);
        for (mfxU16 i = 0; i < task.m_numDirtyRect; i++) {
            RectData *rect = (RectData*)(&(task.m_dirtyRect[i]));
            if (SkipRectangle(rect))
                continue;
            dirtyRects[i].Left = (mfxU16)(rect->Left / blkSize);
            dirtyRects[i].Top = (mfxU16)(rect->Top / blkSize);
            // Driver expects a rect with the 'close' right bottom edge but
            // MSDK uses the 'open' edge rect, thus the right bottom edge which
            // is decreased by 1 below converts 'open' -> 'close' notation
            // We expect here boundaries are already aligned with the BlockSize
            // and Right > Left and Bottom > Top
            dirtyRects[i].Right = (mfxU16)(rect->Right / blkSize) - 1;
            dirtyRects[i].Bottom = (mfxU16)(rect->Bottom / blkSize) - 1;
        }
        pps.pDirtyRect = &(dirtyRects[0]);
    } else {
        pps.pDirtyRect = 0;
    }
#else
    dirtyRects;
    pps.NumDirtyRects = 0;
    pps.pDirtyRect = 0;
#endif  // MFX_ENABLE_HEVCE_DIRTY_RECT

    pps.CodingType      = task.m_codingType;
    pps.CurrPicOrderCnt = task.m_poc;
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    pps.FrameLevel      = (mfxU8)task.m_level; // QP modulation feature; used in low delay mode only
#endif

    pps.bEnableRollingIntraRefresh = task.m_IRState.refrType;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation = task.m_IRState.IntraLocation;
        pps.IntraInsertionSize = task.m_IRState.IntraSize;
        pps.QpDeltaForInsertedIntra = mfxU8(task.m_IRState.IntRefQPDelta);
    }
    pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

    mfxExtAVCEncoderWiDiUsage* extWiDi = ExtBuffer::Get(task.m_ctrl);

    if (extWiDi)
        pps.InputType = eType_DRM_SECURE;
    else
        pps.InputType = eType_DRM_NONE;
    pps.nal_unit_type = task.m_shNUT;
}

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice)
{
    slice.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slice.size(); i++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id = i;
        cs.slice_segment_address = par.m_slice[i].SegmentAddress;
        cs.NumLCUsInSlice = par.m_slice[i].NumLCU;
        cs.bLastSliceOfPic = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & pps,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC_REXT & cs = slice[i];

        FillSliceBuffer(task, pps, (i == slice.size() - 1), slice[i]);

        if (cs.slice_type != 2 && (pps.weighted_pred_flag || pps.weighted_bipred_flag))
        {
            const mfxU16 Y = 0, Cb = 1, Cr = 2, W = 0, O = 1;

            for (mfxU16 r = 0; r < 15; r++)
            {
                cs.luma_offset_l0[r]    = (SHORT)task.m_sh.pwt[0][r][Y][O];
                cs.ChromaOffsetL0[r][0] = (SHORT)task.m_sh.pwt[0][r][Cb][O];
                cs.ChromaOffsetL0[r][1] = (SHORT)task.m_sh.pwt[0][r][Cr][O];
                cs.luma_offset_l1[r]    = (SHORT)task.m_sh.pwt[1][r][Y][O];
                cs.ChromaOffsetL1[r][0] = (SHORT)task.m_sh.pwt[1][r][Cb][O];
                cs.ChromaOffsetL1[r][1] = (SHORT)task.m_sh.pwt[1][r][Cr][O];
            }
        }
    }
}

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT & sps)
{
    Zero(sps);
    FillSpsBuffer(par, caps, (ENCODE_SET_SEQUENCE_PARAMETERS_HEVC&)sps);

    sps.transform_skip_rotation_enabled_flag    = par.m_sps.transform_skip_rotation_enabled_flag;
    sps.transform_skip_context_enabled_flag     = par.m_sps.transform_skip_context_enabled_flag;
    sps.implicit_rdpcm_enabled_flag             = par.m_sps.implicit_rdpcm_enabled_flag;
    sps.explicit_rdpcm_enabled_flag             = par.m_sps.explicit_rdpcm_enabled_flag;
    sps.extended_precision_processing_flag      = par.m_sps.extended_precision_processing_flag;
    sps.intra_smoothing_disabled_flag           = par.m_sps.intra_smoothing_disabled_flag;
    sps.high_precision_offsets_enabled_flag     = par.m_sps.high_precision_offsets_enabled_flag;
    sps.persistent_rice_adaptation_enabled_flag = par.m_sps.persistent_rice_adaptation_enabled_flag;
    sps.cabac_bypass_alignment_enabled_flag     = par.m_sps.cabac_bypass_alignment_enabled_flag;
}

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT & pps)
{
    Zero(pps);
    FillPpsBuffer(par, caps, (ENCODE_SET_PICTURE_PARAMETERS_HEVC&)pps);

    pps.log2_max_transform_skip_block_size_minus2   = par.m_pps.log2_max_transform_skip_block_size_minus2;
    pps.cross_component_prediction_enabled_flag     = par.m_pps.cross_component_prediction_enabled_flag;
    pps.chroma_qp_offset_list_enabled_flag          = par.m_pps.chroma_qp_offset_list_enabled_flag;
    pps.diff_cu_chroma_qp_offset_depth              = par.m_pps.diff_cu_chroma_qp_offset_depth;
    pps.chroma_qp_offset_list_len_minus1            = par.m_pps.chroma_qp_offset_list_len_minus1;

    std::copy(std::begin(par.m_pps.cb_qp_offset_list), std::end(par.m_pps.cb_qp_offset_list), std::begin(pps.cb_qp_offset_list));
    std::copy(std::begin(par.m_pps.cr_qp_offset_list), std::end(par.m_pps.cr_qp_offset_list), std::begin(pps.cr_qp_offset_list));

    pps.log2_sao_offset_scale_luma                  = par.m_pps.log2_sao_offset_scale_luma;
    pps.log2_sao_offset_scale_chroma                = par.m_pps.log2_sao_offset_scale_chroma;
}

#endif //defined(_WIN32) || defined(_WIN64)

}; // namespace MfxHwH265Encode
#endif
