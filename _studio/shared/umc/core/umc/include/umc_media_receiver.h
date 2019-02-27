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

#ifndef __UMC_MEDIA_RECEIVER_H__
#define __UMC_MEDIA_RECEIVER_H__

#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_media_data.h"

namespace UMC
{
// base class for parameters of renderers and buffers
class  MediaReceiverParams
{
    DYNAMIC_CAST_DECL_BASE(MediaReceiverParams)

public:
    // Default constructor
    MediaReceiverParams(void){}
    // Destructor
    virtual ~MediaReceiverParams(void){}
};

// Base class for renderers and buffers
class MediaReceiver
{
    DYNAMIC_CAST_DECL_BASE(MediaReceiver)

public:
    // Default constructor
    MediaReceiver(void){}
    // Destructor
    virtual ~MediaReceiver(void){}

    // Initialize media receiver
    virtual Status Init(MediaReceiverParams *init) = 0;

    // Release all receiver resources
    virtual Status Close(void) {return UMC_OK;}

    // Lock input buffer
    virtual Status LockInputBuffer(MediaData *in) = 0;
    // Unlock input buffer
    virtual Status UnLockInputBuffer(MediaData *in, Status StreamStatus = UMC_OK) = 0;

    // Break waiting(s)
    virtual Status Stop(void) = 0;

    // Reset media receiver
    virtual Status Reset(void) {return UMC_OK;}
};

} // end namespace UMC

#endif // __UMC_MEDIA_RECEIVER_H__