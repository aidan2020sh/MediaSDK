//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_FRAME_H__
#define __UMC_AV1_FRAME_H__

#include "umc_av1_dec_defs.h"

#include <memory>

namespace UMC
{
    class FrameData;
    class MediaData;
}

namespace UMC_AV1_DECODER
{
    class AV1DecoderFrame
    {

    public:

        AV1DecoderFrame();
        ~AV1DecoderFrame();

        void Reset();
        void Reset(FrameHeader const*);
        void Allocate(UMC::FrameData const*);
        void AddSource(UMC::MediaData*);

        UMC::MediaData* GetSource()
        { return source.get(); }

        Ipp32s GetError() const
        { return error; }

        void SetError(Ipp32s e)
        { error = e; }

        void AddError(Ipp32s e)
        { error |= e; }

        FrameHeader& GetFrameHeader()
        { return header; }
        FrameHeader const& GetFrameHeader() const
        { return header; }

        bool Empty() const;

        bool Displayed() const
        { return displayed; }
        void Displayed(bool d)
        { displayed = d; }

        UMC::FrameData* GetFrameData()
        { return data.get(); }
        UMC::FrameData const* GetFrameData() const
        { return data.get(); }

        UMC::FrameMemID GetMemID() const;

    public:

        Ipp32s           UID;

    private:

        Ipp16u                            locked;
        bool                              displayed;


        std::unique_ptr<UMC::FrameData>   data;
        std::unique_ptr<UMC::MediaData>   source;

        Ipp32s                            error;

        FrameHeader                       header;

        Ipp32s                            y_dc_delta_q;
        Ipp32s                            uv_dc_delta_q;
        Ipp32s                            uv_ac_delta_q;
    };

} // end namespace UMC_VP9_DECODER

#endif // __UMC_AV1_FRAME_H__
#endif // UMC_ENABLE_AV1_VIDEO_DECODER
