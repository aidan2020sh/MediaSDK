// Copyright (c) 2019-2021 Intel Corporation
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

#include "hevcehw_g12dg2_caps.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12DG2;

void Caps::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    using Base::Glob;
    using Base::Defaults;

    Push(BLK_SetLowPower,
        [](const mfxVideoParam&, mfxVideoParam& par, StorageRW&) -> mfxStatus
    {
        par.mfx.LowPower = MFX_CODINGOPTION_ON;

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetLowPower.Push([](
            Defaults::TGetHWDefault<mfxU16>::TExt /*prev*/
            , const mfxVideoParam& /*par*/
            , eMFXHWType /*hw*/)
        {
            return mfxU16(MFX_CODINGOPTION_ON);
        });

        defaults.GetMaxNumRef.Push([](
            Base::Defaults::TChain<std::tuple<mfxU16, mfxU16, mfxU16>>::TExt
            , const Base::Defaults::Param& dpar)
        {
            // DG2 HEVC VDENC Maximum supported number
            const mfxU16 nRef[3] = { 3, 2, 1 };

            /* Same way like on previous platforms... */
            mfxU16 numRefFrame = dpar.mvp.mfx.NumRefFrame + !dpar.mvp.mfx.NumRefFrame * 16;

            return std::make_tuple(
                std::min<mfxU16>(nRef[0], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[1], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[2], std::min<mfxU16>(dpar.caps.MaxNum_Reference1, numRefFrame)));
        });

        defaults.GetNumRefActive.Push([](
            Base::Defaults::TGetNumRefActive::TExt
            , const Base::Defaults::Param& dpar
            , mfxU16(*pP)[8]
            , mfxU16(*pBL0)[8]
            , mfxU16(*pBL1)[8])
        {
            bool bExternal = false;
            mfxU16 defaultP = 0, defaultBL0 = 0, defaultBL1 = 0;

            const mfxU16 nRef[3][7] =
            {
                // DG2 HEVC VDENC default reference number with TU
                { 3, 3, 2, 2, 2, 1, 1 },
                { 2, 2, 1, 1, 1, 1, 1 },
                { 1, 1, 1, 1, 1, 1, 1 }

            };
            mfxU16 tu = dpar.mvp.mfx.TargetUsage;

            CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, 4);
            --tu;

            /* Same way like on previous platforms... */
            mfxU16 numRefFrame = dpar.mvp.mfx.NumRefFrame + !dpar.mvp.mfx.NumRefFrame * 16;

            // Get default active frame number
            std::tie(defaultP, defaultBL0, defaultBL1) = std::make_tuple(
                std::min<mfxU16>(nRef[0][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[1][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[2][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference1, numRefFrame)));

            auto SetDefaultNRef =
                [](const mfxU16(*extRef)[8], mfxU16 defaultRef, mfxU16(*NumRefActive)[8])
            {
                bool bExternal = false;
                bool bDone = false;

                bDone |= !NumRefActive;
                bDone |= !bDone && !extRef && std::fill_n(*NumRefActive, 8, defaultRef);
                bDone |= !bDone && std::transform(
                    *extRef
                    , std::end(*extRef)
                    , *NumRefActive
                    , [&](mfxU16 ext)
                    {
                        bExternal |= SetIf(defaultRef, !!ext, ext);
                        return defaultRef;
                    });

                return bExternal;
            };

            const mfxU16(*extRefP)[8]   = nullptr;
            const mfxU16(*extRefBL0)[8] = nullptr;
            const mfxU16(*extRefBL1)[8] = nullptr;

            if (const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(dpar.mvp))
            {
                extRefP   = &pCO3->NumRefActiveP;
                extRefBL0 = &pCO3->NumRefActiveBL0;
                extRefBL1 = &pCO3->NumRefActiveBL1;
            }

            bExternal |= SetDefaultNRef(extRefP, defaultP, pP);
            bExternal |= SetDefaultNRef(extRefBL0, defaultBL0, pBL0);
            bExternal |= SetDefaultNRef(extRefBL1, defaultBL1, pBL1);

            return bExternal;
        });

        bSet = true;

        return MFX_ERR_NONE;
    });

    Push(BLK_SetGUID
        , [](const mfxVideoParam&, mfxVideoParam& out, StorageRW& strg) -> mfxStatus
    {
        MFX_CHECK(!strg.Contains(Glob::GUID::Key), MFX_ERR_NONE);

        if (strg.Contains(Glob::RealState::Key))
        {
            //don't change GUID in Reset
            auto& initPar = Glob::RealState::Get(strg);
            strg.Insert(Glob::GUID::Key, make_storable<GUID>(Glob::GUID::Get(initPar)));
            return MFX_ERR_NONE;
        }

        VideoCORE& core = Glob::VideoCore::Get(strg);
        auto pGUID = make_storable<GUID>();
        auto& defaults = Glob::Defaults::Get(strg);
        Base::EncodeCapsHevc fakeCaps;
        Defaults::Param defPar(out, fakeCaps, core.GetHWType(), defaults);
        fakeCaps.MaxEncodedBitDepth = true;
        fakeCaps.YUV422ReconSupport = true;
        fakeCaps.YUV444ReconSupport = true;

        MFX_CHECK(defaults.GetGUID(defPar, *pGUID), MFX_ERR_NONE);
        strg.Insert(Glob::GUID::Key, std::move(pGUID));

        return MFX_ERR_NONE;
    });
}

void Caps::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_HardcodeCaps
        , [](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        auto& caps = HEVCEHW::Gen12::Glob::EncodeCaps::Get(strg);
        caps.SliceIPOnly = (par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC);
        caps.msdk.PSliceSupport = true;

        return MFX_ERR_NONE;
    });
}

void Caps::GetVideoParam(const FeatureBlocks& /*blocks*/, TPushGVP Push)
{
    Push(BLK_FixParam
        , [](mfxVideoParam& par, StorageR& /*global*/) -> mfxStatus
    {
        par.mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
