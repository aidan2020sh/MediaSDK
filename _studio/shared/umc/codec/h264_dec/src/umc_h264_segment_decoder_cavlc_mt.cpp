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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_segment_decoder_templates.h"

namespace UMC
{

Status H264SegmentDecoderMultiThreaded::DecodeMacroBlockCAVLC(uint32_t nCurMBNumber,
                                                              uint32_t &nMaxMBNumber)
{
    Status status = m_SD->DecodeSegmentCAVLC(nCurMBNumber, nMaxMBNumber, this);
    return status;
} // Status H264SegmentDecoderMultiThreaded::DecodeMacroBlockCAVLC(uint32_t nCurMBNumber,

Status H264SegmentDecoderMultiThreaded::ReconstructMacroBlockCAVLC(uint32_t nCurMBNumber,
                                                                   uint32_t nMaxMBNumber)
{
    Status status = m_SD->ReconstructSegment(nCurMBNumber, nMaxMBNumber, this);
    return status;
}
// Status H264SegmentDecoderMultiThreaded::ReconstructMacroBlockCAVLC(uint32_t nCurMBNumber,

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER