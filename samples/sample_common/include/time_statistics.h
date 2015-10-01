/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel’s Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2008-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "vm/time_defs.h"
#include "vm/strings_defs.h"

#pragma warning(disable:4100)

class CTimeStatistics
{
public:
    CTimeStatistics()
    {
#ifdef TIME_STATS
        ResetStatistics();
        start=0;
#endif
    }

    static msdk_tick GetFrequency()
    {
        if (!frequency)
        {
            frequency = msdk_time_get_frequency();
        }
        return frequency;
    }

    static mfxF64 ConvertToSeconds(msdk_tick elapsed)
    {
#ifdef TIME_STATS
        return MSDK_GET_TIME(elapsed, 0, GetFrequency());
#else
        return 0;
#endif
    }

    inline void StartTimeMeasurement()
    {
#ifdef TIME_STATS
        start = msdk_time_get_tick();
#endif
    }

    inline void StopTimeMeasurement()
    {
#ifdef TIME_STATS
        totalTime+=GetDeltaTime();
        numMeasurements++;
#endif
    }

    inline void StopTimeMeasurementWithCheck()
    {
#ifdef TIME_STATS
        if(start)
        {
            totalTime+=GetDeltaTime();
            numMeasurements++;
        }
#endif
    }

    inline mfxF64 GetDeltaTime()
    {
#ifdef TIME_STATS
        return MSDK_GET_TIME(msdk_time_get_tick(), start, GetFrequency());
#else
        return 0;
#endif
    }

    inline void PrintStatistics(const msdk_char* prefix)
    {
#ifdef TIME_STATS
        msdk_printf(L"    %s Total: %lf Avg %lf (%lld takes)",prefix,totalTime,GetAvgTime(),numMeasurements);
#endif
    }

    inline mfxU64 GetNumMeasurements()
    {
#ifdef TIME_STATS
        return numMeasurements;
#else
        return 0;
#endif
    }

    inline mfxF64 GetAvgTime()
    {
#ifdef TIME_STATS
        return totalTime/numMeasurements;
#else
        return 0;
#endif
    }

    inline void ResetStatistics()
    {
#ifdef TIME_STATS
        totalTime=0;
        numMeasurements=0;
#endif
    }

protected:
    static msdk_tick frequency;
#ifdef TIME_STATS
    msdk_tick start;
    mfxF64 totalTime;
    mfxU64 numMeasurements;
#endif
};
