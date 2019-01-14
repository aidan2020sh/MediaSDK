// Copyright (c) 1985-2018 Intel Corporation
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

#pragma once

#include "cmrt_cross_platform.h"
//#include "cm_rt.h"
#include "cmdeviceex.h"
#include "cmtaskex.h"
#include "cmkernelex.h"
#include "cmassert.h"
#include "vector"
#include "string"
#include "map"
using namespace std;

class CmQueueEx
{
public:
  CmQueueEx(CmDeviceEx & cmDeviceEx)
    : cmDeviceEx(cmDeviceEx)
  {
    int result = cmDeviceEx->CreateQueue(pCmQueue);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  ~CmQueueEx()
  {
    for (unsigned int i = 0; i < events.size(); ++i) {
      pCmQueue->DestroyEvent(events[i]);
    }
    //cmDeviceEx->DestroyQueue (pCmKernel_);
  }

  int EnqueueAndWait(CmTask * pTask)
  {
    CmEvent * e = NULL;

    int enqueueResult = pCmQueue->Enqueue(pTask, e);
    if (enqueueResult == CM_SUCCESS) {
      events.push_back (e);

      CM_STATUS status;
      e->GetStatus (status);
      while (status != CM_STATUS_FINISHED) {
        e->GetStatus (status);
      }
    }

    return enqueueResult;
  }

  void Enqueue(CmKernelEx & kernelEx, CmThreadSpace * pThreadSpace = NULL)
  {
    CmTaskEx cmTaskEx(cmDeviceEx, kernelEx);
    Enqueue(cmTaskEx, pThreadSpace);

    auto iter = eventsMap.find(kernelEx.KernelName());
    if (iter == eventsMap.end()) {
        eventsMap.insert(make_pair(kernelEx.KernelName(), std::vector<CmEvent *>()));
        eventsMap[kernelEx.KernelName()].push_back(*events.rbegin());
    } else {
        iter->second.push_back(*events.rbegin());
    }    
  }

  void Enqueue(CmTask * pTask, CmThreadSpace * pThreadSpace = NULL)
  {
    CmEvent * pEvent = NULL;

    int result = pCmQueue->Enqueue(pTask, pEvent, pThreadSpace);
    CM_FAIL_IF(result != CM_SUCCESS, result);

    events.push_back(pEvent);
  }

  void WaitForLastKernel()
  {
    CmEvent * pEvent = *events.rbegin();

    CM_STATUS status;
    pEvent->GetStatus(status);
    while (status != CM_STATUS_FINISHED) {
      pEvent->GetStatus(status);
    }

    events.pop_back();
    pCmQueue->DestroyEvent(pEvent);
  }

  void WaitForAllKernels()
  {
    CM_STATUS status;
    for(std::vector<CmEvent*>::iterator it = events.begin(); it != events.end(); ++it) {
        CmEvent * pEvent = *it;
        pEvent->GetStatus(status);
        while (status != CM_STATUS_FINISHED) {
          pEvent->GetStatus(status);
        }
        pCmQueue->DestroyEvent(pEvent);
    }
    events.clear();
  }

  CmEvent * LastEvent()
  {
    return *events.rbegin();
  }

  double LastKernelDuration() const
  {
    UINT64 executionTime;
    (*events.rbegin ())->GetExecutionTime(executionTime);

    return executionTime / 1000000.0;
  }

  double KernelDuration(unsigned int id) const
  {
    if (id >= events.size ()) {
      throw CMUT_EXCEPTION("fail");
    }

    UINT64 executionTime;
    events[id]->GetExecutionTime(executionTime);

    return executionTime / 1000000.0;
  }

  double KernelDuration(const string & kernelName) const
  {
    auto iter = eventsMap.find(kernelName);
    //CMUT_ASSERT(iter != eventsMap.end());
    if (iter == eventsMap.end())
        return 0;

    UINT64 enqueueKernelDuration = 0;
    for (unsigned int i = 0; i < iter->second.size(); ++i) {
      // FIXME assert event status is finished
      UINT64 executionTime;
      iter->second[i]->GetExecutionTime(executionTime);

      enqueueKernelDuration += (executionTime & 0xffffffff);
    }

    return enqueueKernelDuration / 1000000.0;
  }

  //FIXME is this a good name?
  double KernelDurationAvg(const string & kernelName) const
  {
    auto iter = eventsMap.find(kernelName);
    if (iter != eventsMap.end())
        return KernelDuration(kernelName) / iter->second.size();
    else
        return 0;
  }

  double KernelDuration() const
  {
    UINT64 enqueueKernelDuration = 0;
    for (auto e : events) {
      // FIXME assert event status is finished
      UINT64 executionTime;
      e->GetExecutionTime(executionTime);

      enqueueKernelDuration += (executionTime & 0xffffffff);
    }

    return enqueueKernelDuration / 1000000.0;
  }

  CmQueue * operator -> () { return pCmQueue; }
protected:
  CmQueue * pCmQueue;
  CmDeviceEx & cmDeviceEx;
  vector<CmEvent *> events;
  map<string, vector<CmEvent *>> eventsMap;
};