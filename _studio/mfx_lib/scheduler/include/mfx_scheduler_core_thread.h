// Copyright (c) 2009-2018 Intel Corporation
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

#ifndef __MFX_SCHEDULER_CORE_THREAD_H
#define __MFX_SCHEDULER_CORE_THREAD_H

#include <mfxdefs.h>

#include <thread>
#include <umc_event.h>

// forward declaration of the owning class
class mfxSchedulerCore;

struct MFX_SCHEDULER_THREAD_CONTEXT
{
    MFX_SCHEDULER_THREAD_CONTEXT()
      : pSchedulerCore(NULL)
      , threadNum(0)
      , threadHandle()
      , workTime(0)
      , sleepTime(0)
    {}

    mfxSchedulerCore *pSchedulerCore;                           // (mfxSchedulerCore *) pointer to the owning core
    mfxU32 threadNum;                                           // (mfxU32) number of the thread
    std::thread threadHandle;                                   // (std::thread) handle to the thread
    UMC::Event taskAdded;                                       // Objects for waiting on in case of nothing to do

    mfxU64 workTime;                                            // (mfxU64) integral working time
    mfxU64 sleepTime;                                           // (mfxU64) integral sleeping time
};

#endif // #ifndef __MFX_SCHEDULER_CORE_THREAD_H
