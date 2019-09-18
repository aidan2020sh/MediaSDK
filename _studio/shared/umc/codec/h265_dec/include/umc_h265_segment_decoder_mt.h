// Copyright (c) 2012-2019 Intel Corporation
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
#ifdef MFX_ENABLE_H265_VIDEO_DECODE
#ifndef MFX_VA

#ifndef __UMC_H265_SEGMENT_DECODER_MT_H
#define __UMC_H265_SEGMENT_DECODER_MT_H

#include "umc_h265_segment_decoder.h"

namespace UMC_HEVC_DECODER
{

class SegmentDecoderRoutines;
class H265Task;

// Slice decoder capable of running in parallel with other decoders
class H265SegmentDecoderMultiThreaded : public H265SegmentDecoder
{
public:
    // Default constructor
    H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoderMultiThreaded(void);

    // Initialize slice decoder
    virtual UMC::Status Init(int32_t iNumber);

    // Initialize decoder and call task processing function
    virtual UMC::Status ProcessSegment(void);

    virtual UMC::Status ProcessTask(H265Task &task);

    // Initialize CABAC context appropriately depending on where starting CTB is
    void InitializeDecoding(H265Task & task);
    // Combined decode and reconstruct task callback
    UMC::Status DecRecSegment(H265Task & task);
    // Decode task callback
    UMC::Status DecodeSegment(H265Task & task);
    // Reconstruct task callback
    UMC::Status ReconstructSegment(H265Task & task);
    // Deblocking filter task callback
    virtual UMC::Status DeblockSegmentTask(H265Task & task);
    // SAO filter task callback
    UMC::Status SAOFrameTask(H265Task & task);

    // Initialize state, decode, reconstruct and filter CTB range
    virtual UMC::Status ProcessSlice(H265Task & task);

    // Recover a region after error
    virtual void RestoreErrorRect(H265Task * task);

protected:

    // Initialize decoder with data of new slice
    virtual void StartProcessingSegment(H265Task &Task);
    // Finish work section
    virtual void EndProcessingSegment(H265Task &Task);

    // Decode one CTB
    bool DecodeCodingUnit_CABAC();

    // Decode CTB range
    UMC::Status DecodeSegment(int32_t curCUAddr, int32_t &nBorder);

    // Reconstruct CTB range
    UMC::Status ReconstructSegment(int32_t curCUAddr, int32_t nBorder);

    // Both decode and reconstruct a CTB range
    UMC::Status DecodeSegmentCABAC_Single_H265(int32_t curCUAddr, int32_t & nBorder);

    // Reconstructor depends on bitdepth_luma || bitdepth_chroma
    void CreateReconstructor();

    void RestoreErrorRect(int32_t startMb, int32_t endMb, H265DecoderFrame *pRefFrame);

private:

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
    {
        return *this;

    } // H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_SEGMENT_DECODER_MT_H */
#endif // #ifndef MFX_VA
#endif // MFX_ENABLE_H265_VIDEO_DECODE