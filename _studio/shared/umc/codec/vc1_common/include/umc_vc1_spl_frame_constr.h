//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_SPL_FRAME_CONSTR_H__
#define __UMC_VC1_SPL_FRAME_CONSTR_H__

#include "umc_media_data_ex.h"
#include "umc_structures.h"

namespace UMC
{
    struct VC1FrameConstrInfo
    {
        MediaData* in;
        MediaData* out;
        MediaDataEx::_MediaDataEx *stCodes;
        Ipp32u splMode;
    };
    class vc1_frame_constructor
    {
    public:
            vc1_frame_constructor(){};
            virtual ~vc1_frame_constructor(){};

            virtual Status GetNextFrame(VC1FrameConstrInfo& pInfo) = 0;  //return 1 frame, 1 header....
            virtual Status GetFirstSeqHeader(VC1FrameConstrInfo& pInfo) = 0;
            virtual Status GetData(VC1FrameConstrInfo& pInfo) = 0;
            virtual Status ParseVC1SeqHeader (Ipp8u *data, Ipp32u* bufferSize, VideoStreamInfo* info) = 0;
            virtual void Reset() = 0;
    };

    class vc1_frame_constructor_rcv: public vc1_frame_constructor
    {
        public:
            vc1_frame_constructor_rcv();
            ~vc1_frame_constructor_rcv();

            Status GetNextFrame(VC1FrameConstrInfo& Info);
            Status GetData(VC1FrameConstrInfo& Info);
            Status GetFirstSeqHeader(VC1FrameConstrInfo& Info);
            void Reset();
            Status ParseVC1SeqHeader (Ipp8u *data, Ipp32u* bufferSize, VideoStreamInfo* info);
    };

    class vc1_frame_constructor_vc1: public vc1_frame_constructor
    {
         public:
            vc1_frame_constructor_vc1();
            ~vc1_frame_constructor_vc1();

            Status GetNextFrame(VC1FrameConstrInfo& Info);
            Status GetData(VC1FrameConstrInfo& Info);
            Status GetFirstSeqHeader(VC1FrameConstrInfo& Info);
            void Reset();
            Status ParseVC1SeqHeader (Ipp8u *data, Ipp32u* bufferSize, VideoStreamInfo* info);
    };
}

#endif//__UMC_VC1_SPL_FRAME_CONSTR_H__

#endif //UMC_ENABLE_VC1_SPLITTER || UMC_ENABLE_VC1_VIDEO_DECODER
