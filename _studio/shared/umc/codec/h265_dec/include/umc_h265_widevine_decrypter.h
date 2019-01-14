// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_WIDEVINE_DECRYPTER_H
#define __UMC_H265_WIDEVINE_DECRYPTER_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#ifndef MFX_ENABLE_CPLIB

#include "umc_h265_headers.h"
#include "huc_based_drm_common.h"

namespace UMC_HEVC_DECODER
{
class H265WidevineSlice;
class DecryptParametersWrapper : public DECRYPT_QUERY_STATUS_PARAMS_HEVC
{
public:
    DecryptParametersWrapper(void);
    DecryptParametersWrapper(const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters);
    virtual ~DecryptParametersWrapper(void);

    DecryptParametersWrapper & operator = (const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters);
    DecryptParametersWrapper & operator = (const DecryptParametersWrapper & pDecryptParametersWrapper);

    UMC::Status GetVideoParamSet(H265VideoParamSet *pcVPS);
    UMC::Status GetSequenceParamSet(H265SeqParamSet *pcSPS);
    UMC::Status GetPictureParamSetFull(H265PicParamSet *pcPPS);

    UMC::Status GetSliceHeaderPart1(H265SliceHeader * sliceHdr);
    UMC::Status GetSliceHeaderFull(H265WidevineSlice *, const H265SeqParamSet *, const H265PicParamSet *);

    void ParseSEIBufferingPeriod(const Headers & headers, H265SEIPayLoad *spl);
    void ParseSEIPicTiming(const Headers & headers, H265SEIPayLoad *spl);

    double GetTime() const {return m_pts;}
    void SetTime(double pts) {m_pts = pts;}

private:
    // Parse profile tier layers header part in VPS or SPS
    void parsePTL(H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1);
    // Parse one profile tier layer
    void parseProfileTier(H265PTL *ptl, h265_layer_profile_tier_level *sourcePtl);
    // Parse RPS part in SPS
    void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, uint32_t idx);
    // Parse RPS part in slice header
    void parseRefPicSet(ReferencePictureSet* pRPS);
    // Parse video usability information block in SPS
    void parseVUI(H265SeqParamSet *sps);
    // Parse HRD information in VPS or in VUI block of SPS
    void parseHrdParameters(H265HRD *hrd, uint8_t commonInfPresentFlag, uint32_t vps_max_sub_layers);
    // Parse scaling list information in SPS or PPS
    void parseScalingList(H265ScalingList *, h265_scaling_list *);
    // Parse scaling list data block
    void xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId, h265_scaling_list *);
    // Parse remaining of slice header after GetSliceHeaderPart1
    void decodeSlice(H265WidevineSlice *, const H265SeqParamSet *, const H265PicParamSet *);

    void CopyDecryptParams(const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters);

private:
    double m_pts;
};


class WidevineDecrypter
{
public:

    WidevineDecrypter()
        : m_va(0)
        , m_bitstreamSubmitted(false)
        , m_PESPacketCounter(0)
    {
#ifdef UMC_VA_DXVA
        m_pDummySurface = NULL;
#endif
    }

    ~WidevineDecrypter()
    {
    }

    void Init()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
        if (!m_va)
            return;
    }

    void Reset()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
    }

    void SetVideoHardwareAccelerator(UMC::VideoAccelerator * va)
    {
        if (va)
            m_va = (UMC::VideoAccelerator*)va;
    }

    UMC::Status DecryptFrame(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams);

    void ReleaseForNewBitstream()
    {
        m_bitstreamSubmitted = false;
    }

protected:
    UMC::VideoAccelerator *m_va;
    bool m_bitstreamSubmitted;
    uint16_t m_PESPacketCounter;

#ifdef UMC_VA_DXVA
    IDirect3DSurface9* m_pDummySurface;
#endif
};

} // namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_ENABLE_CPLIB
#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H265_WIDEVINE_DECRYPTER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER