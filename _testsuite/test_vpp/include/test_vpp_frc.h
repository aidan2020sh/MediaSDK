/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008 - 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __TEST_VPP_FRC_H
#define __TEST_VPP_FRC_H

#include "mfxvideo.h"
#include <stdio.h>

class BaseFRCChecker
{
public:
    //BaseFRCChecker();
    virtual ~BaseFRCChecker(){}

    virtual mfxStatus Init(mfxVideoParam *par, mfxU32 asyncDeep) = 0;

    // notify FRCChecker about one more input frame
    virtual bool  PutInputFrameAndCheck(mfxFrameSurface1* pSurface) = 0;

    // notify FRCChecker about one more output frame and check result
    virtual bool  PutOutputFrameAndCheck(mfxFrameSurface1* pSurface) = 0; 
};


class FRCChecker : public BaseFRCChecker
{
public:
    FRCChecker();
    virtual ~FRCChecker(){};

    virtual mfxStatus Init(mfxVideoParam *par, mfxU32 asyncDeep);

    // notify FRCChecker about one more input frame
    virtual bool  PutInputFrameAndCheck(mfxFrameSurface1* pSurface);

    // notify FRCChecker about one more output frame and check result
    virtual bool  PutOutputFrameAndCheck(mfxFrameSurface1* pSurface); 

protected:
    // calculate greatest common divisor
    mfxU32  CalculateGCD(mfxU32 val1, mfxU32 val2);
    // calculate period 
    void    DefinePeriods();
    void    DefineEdges();

    bool    CheckSurfaceCorrespondance(){return true;}
    void    PrintDumpInfoAboutMomentError();
    void    PrintDumpInfoAboutAverageError();

    mfxU32  m_FRateExtN_In;
    mfxU32  m_FRateExtD_In;
    mfxU32  m_FRateExtN_Out;
    mfxU32  m_FRateExtD_Out;

    // analyzing periods
    mfxU32  m_FramePeriod_In;
    mfxU32  m_FramePeriod_Out;

    // admissible error on period in frames
    mfxU32  m_Error_In;
    mfxU32  m_Error_Out;

    // edges in times between input and output
    mfxU32  m_UpperEdge;
    mfxU32  m_BottomEdge;

    // frame counter in the period for input frames
    mfxU32  m_NumFrame_In;
    // frame counter in the period  for output frames
    mfxU32  m_NumFrame_Out;

    mfxU32  m_MomentError;
    mfxU64  m_AverageError;

    mfxU32  m_asyncDeep;

};

#endif /* __TEST_VPP_PTS_H*/