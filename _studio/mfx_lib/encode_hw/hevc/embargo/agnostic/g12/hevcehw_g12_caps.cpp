// Copyright (c) 2019 Intel Corporation
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

#include "hevcehw_g12_caps.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

void Caps::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{

    Push(BLK_CheckLowPower,
        [this](const mfxVideoParam& par, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        if (Glob::VideoCore::Get(strg).GetHWType() == MFX_HW_DG2)
        {
            MFX_CHECK(par.mfx.LowPower != MFX_CODINGOPTION_OFF, MFX_ERR_UNSUPPORTED);
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetLowPower.Push([](
            Defaults::TGetHWDefault<mfxU16>::TExt prev
            , const mfxVideoParam& par
            , eMFXHWType hw)
        {
            if (hw == MFX_HW_DG2) {
                return mfxU16(MFX_CODINGOPTION_ON);
            }

            return prev(par, hw);
        });

        defaults.GetMaxNumRef.Push([](
            Gen11::Defaults::TChain<std::tuple<mfxU16, mfxU16>>::TExt
            , const Gen11::Defaults::Param& dpar)
        {
            const mfxU16 nRef[5][2][7] =
            {
                {   // VME
                    { 4, 4, 3, 3, 3, 1, 1 },
                    { 2, 2, 1, 1, 1, 1, 1 }
                },
                {   // VDENC P
                    { 3, 3, 2, 2, 2, 1, 1 },
                    { 3, 3, 2, 2, 2, 1, 1 }
                },
                {   // Gen12 VDENC RA B
                    { 2, 2, 1, 1, 1, 1, 1 },
                    { 1, 1, 1, 1, 1, 1, 1 }
                },
                {   // DG2 VDENC P
                    { 2, 2, 2, 2, 2, 1, 1 },
                    { 2, 2, 2, 2, 2, 1, 1 }
                },
                {   // DG2 VDENC RA B
                    { 1, 1, 1, 1, 1, 1, 1 },
                    { 1, 1, 1, 1, 1, 1, 1 }
                }
            };
            bool    bBFrames = (dpar.mvp.mfx.GopRefDist > 1);
            bool    bVDEnc   = IsOn(dpar.mvp.mfx.LowPower);
            bool    bDG2     = (dpar.hw == MFX_HW_DG2);
            mfxU16  tu       = dpar.mvp.mfx.TargetUsage;
            mfxU32  idx      = bVDEnc * (1 + bBFrames + (bDG2 * 2));

            CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, 4);
            --tu;

            return std::make_tuple(
                std::min<mfxU16>(nRef[idx][0][tu], dpar.caps.MaxNum_Reference0)
                , std::min<mfxU16>(nRef[idx][1][tu], dpar.caps.MaxNum_Reference1));
        });

        bSet = true;

        return MFX_ERR_NONE;
    });
}

void Caps::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_HardcodeCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        auto& caps = Glob::EncodeCaps::Get(strg);
        auto  hw   = Glob::VideoCore::Get(strg).GetHWType();

        caps.SliceIPOnly                = IsOn(par.mfx.LowPower) && (par.mfx.TargetUsage == 7);
        caps.msdk.bSingleSliceMultiTile = false;

        caps.YUV422ReconSupport |= (!caps.Color420Only && IsOff(par.mfx.LowPower));

        // For now the driver reports in caps 8 pipes for ATS while in fact there are 2
        // and more pipes are not expected to be supported.
        // Delete the code when caps are fixed.
        bool bMax2Pipes = hw >= MFX_HW_ATS && IsOn(par.mfx.LowPower);
        if (bMax2Pipes)
        {
            caps.NumScalablePipesMinus1 = std::max<uint32_t>(caps.NumScalablePipesMinus1, 1);
        }

        SetSpecificCaps(caps);

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)