// Copyright (c) 2019-2020 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_impl.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_general.h"
#include "av1ehw_base_superres.h"
#include "av1ehw_base_packer.h"
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_task.h"
#include "av1ehw_base_iddi.h"
#include "av1ehw_base_iddi_packer.h"
#include "av1ehw_base_segmentation.h"

#include <algorithm>

using namespace AV1EHW;
using namespace AV1EHW::Base;

MFXVideoENCODEAV1_HW::MFXVideoENCODEAV1_HW(VideoCORE& core)
    : m_core(core)
    , m_runtimeErr(MFX_ERR_NONE)
{
}

void MFXVideoENCODEAV1_HW::InternalInitFeatures(
    mfxStatus& status
    , eFeatureMode mode)
{
    status = MFX_ERR_UNKNOWN;

    for (auto& pFeature : m_features)
        pFeature->Init(mode, *this);

    if (mode & INIT)
    {
        Reorder(
            BQ<BQ_SetDefaults>::Get(*this)
            , { FEATURE_GENERAL, AV1EHW::Base::General::BLK_SetDefaults }
            , { FEATURE_SUPERRES, AV1EHW::Base::Superres::BLK_SetDefaults });

        Reorder(
            BQ<BQ_SetDefaults>::Get(*this)
            , { FEATURE_GENERAL, General::BLK_SetDefaults }
            , { FEATURE_DDI, IDDI::BLK_QueryCaps });

        Reorder(
            BQ<BQ_InitExternal>::Get(*this)
            , { FEATURE_DDI, IDDI::BLK_CreateDevice }
            , { FEATURE_GENERAL, General::BLK_Query1NoCaps });

        auto& qIA = BQ<BQ_InitAlloc>::Get(*this);
        Reorder(qIA
            , { FEATURE_GENERAL, General::BLK_AllocRec }
            , { FEATURE_DDI, IDDI::BLK_CreateService }
            , PLACE_AFTER);
        qIA.splice(qIA.end(), qIA, Get(qIA, { FEATURE_DDI_PACKER, IDDIPacker::BLK_Init }));
        qIA.splice(qIA.end(), qIA, Get(qIA, { FEATURE_DDI, IDDI::BLK_Register }));

    }

    status = MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEAV1_HW::InternalQuery(
    VideoCORE& core
    , mfxVideoParam *in
    , mfxVideoParam& out)
{

    if (!in)
    {
        return RunBlocks(IgnoreSts, BQ<BQ_Query0>::Get(*this), out);
    }

    mfxStatus
        sts = MFX_ERR_NONE
        , wrn = MFX_ERR_NONE;
    StorageRW& strg = m_storage;
    strg.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(core));

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_Query1NoCaps>::Get(*this), *in, out, strg);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);
    wrn = sts;

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_Query1WithCaps>::Get(*this), *in, out, strg);

    return GetWorstSts(wrn, sts);
}

mfxStatus MFXVideoENCODEAV1_HW::InternalQueryIOSurf(
    VideoCORE& core
    , mfxVideoParam& par
    , mfxFrameAllocRequest& request)
{
    StorageRW& strg = m_storage;
    strg.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(core));
    return RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_QueryIOSurf>::Get(*this), par, request, strg);
}

mfxStatus MFXVideoENCODEAV1_HW::QueryImplsDescription(
    VideoCORE& core
    , mfxEncoderDescription::encoder& caps
    , mfx::PODArraysHolder& ah)
{
    auto& queue = BQ<BQ_QueryImplsDescription>::Get(*this);
    MFX_CHECK(!queue.empty(), MFX_ERR_UNSUPPORTED);
    return RunBlocks(Check<mfxStatus, MFX_ERR_NONE>, queue, core, caps, ah, m_storage);
}

mfxStatus MFXVideoENCODEAV1_HW::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_storage.Empty(), MFX_ERR_UNDEFINED_BEHAVIOR);
    mfxStatus sts = MFX_ERR_NONE, wrn = MFX_ERR_NONE;
    StorageRW local, global;

    global.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(m_core));
    global.Insert(Glob::RTErr::Key, new StorableRef<mfxStatus>(m_runtimeErr));

    wrn = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_InitExternal>::Get(*this), *par, global, local);
    MFX_CHECK(wrn >= MFX_ERR_NONE, wrn);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_InitInternal>::Get(*this), global, local);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);
    wrn = GetWorstSts(sts, wrn);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_InitAlloc>::Get(*this), global, local);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);
    wrn = GetWorstSts(sts, wrn);

    m_storage = std::move(global);
    m_runtimeErr = MFX_ERR_NONE;

    for (auto& pFeature : m_features)
        pFeature->Init(RUNTIME, *this);

    {
        auto& queue = BQ<BQ_ResetState>::Get(*this);

        auto it = Find(queue, { FEATURE_DDI, IDDI::BLK_Reset });
        if (it != queue.end())
            queue.splice(queue.end(), queue, it);
    }

    {
        auto& queue = BQ<BQ_SubmitTask>::Get(*this);
        queue.splice(queue.end(), queue, Get(queue, { FEATURE_DDI_PACKER, IDDIPacker::BLK_SubmitTask }));
        queue.splice(queue.end(), queue, Get(queue, { FEATURE_DDI, IDDI::BLK_SubmitTask }));
    }

    {
        auto& queue = BQ<BQ_PostReorderTask>::Get(*this);
        auto it = Find(queue, { FEATURE_SEGMENTATION, Segmentation::BLK_ConfigureTask });

        if (it != queue.end())
        {
            Reorder(queue
                , { FEATURE_GENERAL, General::BLK_ConfigureTask }
                , { FEATURE_SEGMENTATION, Segmentation::BLK_ConfigureTask }
            , PLACE_AFTER);
        }
    }

    return wrn;
}

mfxStatus MFXVideoENCODEAV1_HW::EncodeFrameCheck(
    mfxEncodeCtrl *ctrl
    , mfxFrameSurface1 *surface
    , mfxBitstream *bs
    , mfxFrameSurface1 ** /*reordered_surface*/
    , mfxEncodeInternalParams * /*pInternalParams*/
    , MFX_ENTRY_POINT *pEntryPoint)
{
    MFX_CHECK(!m_storage.Empty(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR2(bs, pEntryPoint);
    MFX_CHECK_STS(m_runtimeErr);

    mfxStatus sts = MFX_ERR_NONE;
    StorageRW local;

    auto BreakAtSts = [](mfxStatus x)
    {
        return
            (x < MFX_ERR_NONE && x != MFX_ERR_MORE_DATA_SUBMIT_TASK)
            || x == MFX_WRN_DEVICE_BUSY;
    };
    sts = RunBlocks(BreakAtSts, BQ<BQ_FrameSubmit>::Get(*this), ctrl, surface, *bs, m_storage, local);
    MFX_CHECK(!BreakAtSts(sts), sts);

    pEntryPoint->pState = this;
    pEntryPoint->pRoutine = Execute;
    pEntryPoint->pCompleteProc = FreeResources;
    pEntryPoint->requiredNumThreads = 1;
    pEntryPoint->pParam = &Tmp::CurrTask::Get(local);

    return sts;
}

mfxStatus MFXVideoENCODEAV1_HW::Execute(mfxThreadTask ptask, mfxU32 /*uid_p*/, mfxU32 /*uid_a*/)
{
    MFX_CHECK(!m_storage.Empty(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(ptask);
    MFX_CHECK_STS(m_runtimeErr);

    auto& task = *(StorageRW*)ptask;

    return RunBlocks(Check<mfxStatus, MFX_ERR_NONE>, BQ<BQ_AsyncRoutine>::Get(*this), m_storage, task);
}

mfxStatus MFXVideoENCODEAV1_HW::FreeResources(mfxThreadTask /*task*/, mfxStatus /*sts*/)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEAV1_HW::Close(void)
{
    MFX_CHECK(!m_storage.Empty(), MFX_ERR_NOT_INITIALIZED);

    auto sts = RunBlocks(IgnoreSts, BQ<BQ_Close>::Get(*this), m_storage);

    m_storage.Clear();

    return sts;
}

mfxStatus MFXVideoENCODEAV1_HW::Reset(mfxVideoParam* par)
{
    MFX_CHECK(!m_storage.Empty(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    mfxStatus sts = MFX_ERR_NONE, wrn = MFX_ERR_NONE;
    StorageRW global, local;

    global.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(m_core));
    global.Insert(Glob::RealState::Key, new StorableRef<StorageW>(m_storage));

    wrn = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_Reset>::Get(*this), *par, global, local);
    MFX_CHECK(wrn >= MFX_ERR_NONE, wrn);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, BQ<BQ_ResetState>::Get(*this), global, local);

    return GetWorstSts(wrn, sts);
}

mfxStatus MFXVideoENCODEAV1_HW::GetVideoParam(mfxVideoParam* par)
{
    MFX_CHECK(!m_storage.Empty(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    return RunBlocks(IgnoreSts, BQ<BQ_GetVideoParam>::Get(*this), *par, m_storage);
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
