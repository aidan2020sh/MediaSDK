//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw_brc.h"
#include "math.h"
#include <algorithm>
namespace MfxHwH265Encode
{

#define CLIPVAL(MINVAL, MAXVAL, VAL) (IPP_MAX(MINVAL, IPP_MIN(MAXVAL, VAL)))
mfxF64 const INIT_RATE_COEFF_VME[] = {
        1.109, 1.196, 1.225, 1.309, 1.369, 1.428, 1.490, 1.588, 1.627, 1.723, 1.800, 1.851, 1.916,
        2.043, 2.052, 2.140, 2.097, 2.096, 2.134, 2.221, 2.084, 2.153, 2.117, 2.014, 1.984, 2.006,
        1.801, 1.796, 1.682, 1.549, 1.485, 1.439, 1.248, 1.221, 1.133, 1.045, 0.990, 0.987, 0.895,
        0.921, 0.891, 0.887, 0.896, 0.925, 0.917, 0.942, 0.964, 0.997, 1.035, 1.098, 1.170, 1.275
    };
mfxF64 const QSTEP_VME[52] = {
        0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
        2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
    12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
    57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070
};
/*mfxF64 const QSTEP[52] = {
    0.82,    0.93,    1.11,    1.24,     1.47,    1.65,    1.93,    2.12,    2.49,    2.80,
    3.15,    3.53,    4.10,    4.55,     5.24,    5.88,    6.44,    7.20,    8.36,    8.92,
    10.26,  11.55,    12.93,  14.81,    17.65,   19.30,   22.30,   25.46,   28.97,   33.22,
    38.50,  43.07,    50.52,  55.70,    64.34,   72.55,   82.25,   93.12,   108.95, 122.40,
    130.39,148.69,   185.05, 194.89,   243.18,  281.61,  290.58,   372.38,  378.27, 406.62,
    468.34,525.69 
};*/


mfxF64 const INTRA_QSTEP_COEFF  = 2.0;
mfxF64 const INTRA_MODE_BITCOST = 0.0;
mfxF64 const INTER_MODE_BITCOST = 0.0;
mfxI32 const MAX_QP_CHANGE      = 2;
mfxF64 const LOG2_64            = 3;
mfxF64 const MIN_EST_RATE       = 0.3;
mfxF64 const NORM_EST_RATE      = 100.0;

mfxF64 const MIN_RATE_COEFF_CHANGE = 0.5;
mfxF64 const MAX_RATE_COEFF_CHANGE = 2.0;



mfxStatus VMEBrc::Init(MfxVideoParam &video, mfxI32 )
{
    m_qpUpdateRange = 20;
    mfxU32 RegressionWindow = 20;

    mfxF64 fr = mfxF64(video.mfx.FrameInfo.FrameRateExtN) / video.mfx.FrameInfo.FrameRateExtD;
    m_totNumMb = video.mfx.FrameInfo.Width * video.mfx.FrameInfo.Height / 256;
    m_initTargetRate = 1000 * video.mfx.TargetKbps / fr / m_totNumMb;
    m_targetRateMin = m_initTargetRate;
    m_targetRateMax = m_initTargetRate;

    m_laData.resize(0);

    for (mfxU32 qp = 0; qp < 52; qp++)
            m_rateCoeffHistory[qp].Reset(RegressionWindow, 100.0, 100.0 * INIT_RATE_COEFF_VME[qp]);

    m_framesBehind = 0;
    m_bitsBehind = 0.0;
    m_curQp = -1;
    m_curBaseQp = -1;
    m_lookAheadDep = 0;
    

    return MFX_ERR_NONE;
}

mfxStatus VMEBrc::SetFrameVMEData(const mfxExtLAFrameStatistics *pLaOut, mfxU32 width, mfxU32 height)
{
    mfxU32 resNum = 0;
    mfxU32 numLaFrames = pLaOut->NumFrame;
    mfxU32 k = height*width >> 7;

    UMC::AutomaticUMCMutex guard(m_mutex);

    while(resNum < pLaOut->NumStream) 
    {
        if (pLaOut->FrameStat[resNum*numLaFrames].Height == height &&
            pLaOut->FrameStat[resNum*numLaFrames].Width  == width) 
            break;
        resNum ++;
    }
    MFX_CHECK(resNum <  pLaOut->NumStream, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pLaOut->NumFrame > 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxLAFrameInfo * pFrameData = pLaOut->FrameStat + numLaFrames*resNum;
    std::list<LaFrameData>::iterator it = m_laData.begin();
    while (m_laData.size()>0)
    {
        it = m_laData.begin();
        if (!((*it).bNotUsed))
            break;
        m_laData.pop_front();
    }

    // some frames can be stored already
    // start of stored sequence
    it = m_laData.begin();
     while (it != m_laData.end())
    {
        if ((*it).encOrder == pFrameData[0].FrameEncodeOrder)
            break;
        ++it;
    }
    mfxU32 ind  = 0;
    
    // check stored sequence
    while ((it != m_laData.end()) && (ind < numLaFrames))
    {
        MFX_CHECK((*it).encOrder == pFrameData[ind].FrameEncodeOrder, MFX_ERR_UNDEFINED_BEHAVIOR);
        ++ind;
        ++it;
    }
    MFX_CHECK(it == m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    // store a new data
    for (; ind < numLaFrames; ind++)
    {
        LaFrameData data = {};

        data.encOrder  = pFrameData[ind].FrameEncodeOrder;
        data.dispOrder = pFrameData[ind].FrameDisplayOrder;
        data.interCost = pFrameData[ind].InterCost;
        data.intraCost = pFrameData[ind].IntraCost;
        data.propCost  = pFrameData[ind].DependencyCost;
        data.bframe    = (pFrameData[ind].FrameType & MFX_FRAMETYPE_B) != 0;
        data.layer     = pFrameData[ind].Layer;

        MFX_CHECK(data.intraCost, MFX_ERR_UNDEFINED_BEHAVIOR);

        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            data.estRate[qp] = ((mfxF64)pFrameData[ind].EstimatedRate[qp])/(QSTEP_VME[qp]*k);
            //printf("data.estRate[%d] %f\n", qp, data.estRate[qp]);
        }
        //printf("EncOrder %d, dispOrder %d, interCost %d,  intraCost %d, data.propCost %d\n", 
        //    data.encOrder,data.dispOrder, data.interCost, data.intraCost, data.propCost );
        m_laData.push_back(data);
    }
    if (m_lookAheadDep == 0)
        m_lookAheadDep = numLaFrames;
      
    //printf("VMEBrc::SetFrameVMEData m_laData[0].encOrder %d\n", pFrameData[0].FrameEncodeOrder);

    return MFX_ERR_NONE;
}

mfxU8 SelectQp(mfxF64 erate[52], mfxF64 budget, mfxU8 minQP)
{
    minQP = (minQP !=0) ? minQP : 1; //KW
    for (mfxU8 qp = minQP; qp < 52; qp++)
        if (erate[qp] < budget)
            return (erate[qp - 1] + erate[qp] < 2 * budget) ? qp - 1 : qp;
    return 51;
}


mfxF64 GetTotalRate(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxI32 baseQp)
{
    mfxF64 totalRate = 0.0;
    std::list<VMEBrc::LaFrameData>::iterator it = start;
    for (; it!=end; ++it)
        totalRate += (*it).estRateTotal[CLIPVAL(0, 51, baseQp + (*it).deltaQp)];
    return totalRate;
}



 mfxI32 SelectQp(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxF64 budget, mfxU8 minQP)
{
    mfxF64 prevTotalRate = GetTotalRate(start,end, minQP);
    for (mfxU8 qp = minQP+1; qp < 52; qp++)
    {
        mfxF64 totalRate = GetTotalRate(start,end, qp);
        if (totalRate < budget)
            return (prevTotalRate + totalRate < 2 * budget) ? qp - 1 : qp;
        else
            prevTotalRate = totalRate;
    }
    return 51;
}

mfxU8 GetFrameTypeLetter(mfxU32 frameType)
{
    mfxU32 ref = (frameType & MFX_FRAMETYPE_REF) ? 0 : 'a' - 'A';
    if (frameType & MFX_FRAMETYPE_I)
        return mfxU8('I' + ref);
    if (frameType & MFX_FRAMETYPE_P)
        return mfxU8('P' + ref);
    if (frameType & MFX_FRAMETYPE_B)
        return mfxU8('B' + ref);
    return 'x';
}




void VMEBrc::PreEnc(mfxU32 /*frameType*/, std::vector<VmeData *> const & /*vmeData*/, mfxU32 /*curEncOrder*/)
{
}


mfxU32 VMEBrc::Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 /*userDataLength*/, mfxU32 /*repack*/, mfxU32  picOrder, mfxU32 /* maxFrameSize */, mfxU32 /* qp */)
{
    frameType; // unused

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::Report");
    //printf("VMEBrc::Report order %d\n", picOrder);
    mfxF64 realRatePerMb = 8 * dataLength / mfxF64(m_totNumMb);

    m_framesBehind++;
    m_bitsBehind += realRatePerMb;
   
    // start of sequence
    mfxU32 numFrames = 0;
    std::list<LaFrameData>::iterator start = m_laData.begin();
    for(;start != m_laData.end(); ++start)
    {
        if ((*start).encOrder == picOrder)
            break;
    }
    //printf("Report: data len %d, realRatePerMb %f, framesBehin %d, bitsBehind %f, init target rate %f, qp %d\n", dataLength, realRatePerMb, m_framesBehind, m_bitsBehind, m_initTargetRate, (*start).qp);

    for (std::list<LaFrameData>::iterator it = start; it != m_laData.end(); ++it) numFrames++;
    numFrames = IPP_MIN(numFrames, m_lookAheadDep);

     mfxF64 framesBeyond = (mfxF64)(IPP_MAX(2, numFrames) - 1); 
     m_targetRateMax = (m_initTargetRate * (m_framesBehind + m_lookAheadDep - 1) - m_bitsBehind) / framesBeyond;
     m_targetRateMin = (m_initTargetRate * (m_framesBehind + framesBeyond  ) - m_bitsBehind) / framesBeyond;


    if (start != m_laData.end())
    {
        //mfxU32 level = (*start).layer < 8 ? (*start).layer : 7;
        mfxI32 curQp = (*start).qp;
        mfxF64 oldCoeff = m_rateCoeffHistory[curQp].GetCoeff();
        mfxF64 y = IPP_MAX(0.0, realRatePerMb);
        mfxF64 x = (*start).estRate[curQp];
        mfxF64 minY = NORM_EST_RATE * INIT_RATE_COEFF_VME[curQp] * MIN_RATE_COEFF_CHANGE;
        mfxF64 maxY = NORM_EST_RATE * INIT_RATE_COEFF_VME[curQp] * MAX_RATE_COEFF_CHANGE;
        y = CLIPVAL(minY, maxY, y / x * NORM_EST_RATE); 
        m_rateCoeffHistory[curQp].Add(NORM_EST_RATE, y);
        //mfxF64 ratio = m_rateCoeffHistory[curQp].GetCoeff() / oldCoeff;
        mfxF64 ratio = y / (oldCoeff*NORM_EST_RATE);
        //printf("oldCoeff %f, new %f, ratio %f\n",oldCoeff, y, ratio);
        for (mfxI32 i = -m_qpUpdateRange; i <= m_qpUpdateRange; i++)
            if (i != 0 && curQp + i >= 0 && curQp + i < 52)
            {
                mfxF64 r = ((ratio - 1.0) * (1.0 - ((mfxF64)abs(i))/((mfxF64)m_qpUpdateRange + 1.0)) + 1.0);
                m_rateCoeffHistory[curQp + i].Add(NORM_EST_RATE,
                    NORM_EST_RATE * m_rateCoeffHistory[curQp + i].GetCoeff() * r);
            }
        (*start).bNotUsed = 1;
    }
    return 0;
}

mfxI32 VMEBrc::GetQP(MfxVideoParam &video, Task &task )
{
    video;

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VMEBrc::GetQp");

    if (!m_laData.size())
        return 26;

    mfxF64 totalEstRate[52] = {}; 

     // start of sequence
    std::list<LaFrameData>::iterator start = m_laData.begin();
    while (start != m_laData.end())
    {
        if ((*start).encOrder == task.m_eo)
            break;
        ++start;
    }
    MFX_CHECK(start != m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);
    
    std::list<LaFrameData>::iterator it = start;
    mfxU32 numberOfFrames = 0;
    for(it = start;it != m_laData.end(); ++it) 
        numberOfFrames++;

    numberOfFrames = IPP_MIN(numberOfFrames, m_lookAheadDep);

   // fill totalEstRate
   it = start;
   for(mfxU32 i=0; i < numberOfFrames ; i++)
   {
        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            
            (*it).estRateTotal[qp] = IPP_MAX(MIN_EST_RATE, m_rateCoeffHistory[qp].GetCoeff() * (*it).estRate[qp]);
            totalEstRate[qp] += (*it).estRateTotal[qp];        
        }
        ++it;
   }
   


   // calculate Qp
    mfxI32 maxDeltaQp = -100000;
    mfxI32 curQp = m_curBaseQp < 0 ? SelectQp(totalEstRate, m_targetRateMin * numberOfFrames, 1) : m_curBaseQp;
    mfxF64 strength = 0.03 * curQp + .75;

    // fill deltaQp
    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        mfxU32 intraCost    = (*it).intraCost;
        mfxU32 interCost    = (*it).interCost;
        mfxU32 propCost     = (*it).propCost;
        mfxF64 ratio        = 1.0;//mfxF64(interCost) / intraCost;
        mfxF64 deltaQp      = log((intraCost + propCost * ratio) / intraCost) / log(2.0);
        (*it).deltaQp = (interCost >= intraCost * 0.9)
            ? -mfxI32(deltaQp * 2.0 * strength + 0.5)
            : -mfxI32(deltaQp * 1.0 * strength + 0.5);
        maxDeltaQp = IPP_MAX(maxDeltaQp, (*it).deltaQp);
        ++it;
    }
    
 
    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        (*it).deltaQp -= maxDeltaQp;
        ++it;
    }

    
    {
        mfxU8 minQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMax * numberOfFrames, 1);
        mfxU8 maxQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMin * numberOfFrames, 1);

        if (m_curBaseQp < 0)
            m_curBaseQp = minQp; // first frame
        else if (m_curBaseQp < minQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, minQp);
        else if (m_curQp > maxQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, maxQp);
        else
            {}; // do not change qp if last qp guarantees target rate interval
    }
    m_curQp = CLIPVAL(1, 51, m_curBaseQp + (*start).deltaQp); // driver doesn't support qp=0
    //printf("%d) intra %d, inter %d, prop %d, delta %d, maxdelta %d, baseQP %d, qp %d \n",(*start).encOrder,(*start).intraCost, (*start).interCost, (*start).propCost, (*start).deltaQp, maxDeltaQp, m_curBaseQp,m_curQp );

    //printf("%d\t base QP %d\tdelta QP %d\tcurQp %d, rate (%f, %f), total rate %f (%f, %f), number of frames %d\n", 
    //    (*start).dispOrder, m_curBaseQp, (*start).deltaQp, m_curQp, (mfxF32)m_targetRateMax*numberOfFrames,(mfxF32)m_targetRateMin*numberOfFrames,  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp),  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp + 1), (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp -1),numberOfFrames);
    //if ((*start).dispOrder > 1700 && (*start).dispOrder < 1800 )
    {
        //for (mfxU32 qp = 0; qp < 52; qp++)
        //{
        //    printf("....qp %d coeff hist %f\n", qp, (mfxF32)m_rateCoeffHistory[qp].GetCoeff());
       // }        
    }
    (*start).qp = m_curQp;

    return mfxU8(m_curQp);
}
BrcIface * CreateBrc(MfxVideoParam &video)

{
    if (video.isSWBRC())
    {
        if (video.mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
            return new VMEBrc;
#ifdef NEW_BRC
        
        else if (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR || video.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
            return new H265BRCNew;
#else
        else if (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR || video.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
            return new H265BRC;
#endif
    }
    return 0;
}

#ifndef NEW_BRC
#define MFX_H265_BITRATE_SCALE 0
#define MFX_H265_CPBSIZE_SCALE 2



//--------------------------------- SW BRC -----------------------------------------------------------------------
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX_DOUBLE                  1.7e+308    ///< max. value of double-type value
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

#define BRC_CLIP(val, minval, maxval) val = Saturate(minval, maxval, val)

#define SetQuantB() \
    mQuantB = ((mQuantP + mQuantPrev + 1) >> 1) + 1; \
    BRC_CLIP(mQuantB, 1, mQuantMax)

   mfxF64 const QSTEP1[88] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
        256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
        1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
        4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
    };


mfxI32 QStep2QpFloor(mfxF64 qstep, mfxI32 qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
{
    Ipp8u qp = Ipp8u(std::upper_bound(QSTEP1, QSTEP1 + 52 + qpoffset, qstep) - QSTEP1);
    return qp > 0 ? qp - 1 : 0;
}

mfxI32 Qstep2QP(mfxF64 qstep, mfxI32 qpoffset = 0) // return 0<=qp<=51+mQuantOffset
{
    mfxI32 qp = QStep2QpFloor(qstep, qpoffset);
    return (qp == 51 + qpoffset || qstep < (QSTEP1[qp] + QSTEP1[qp + 1]) / 2) ? qp : qp + 1;
}

mfxF64 QP2Qstep(mfxI32 qp, mfxI32 qpoffset = 0)
{
    return QSTEP1[IPP_MIN(51 + qpoffset, qp)];
}

#define BRC_QSTEP_SCALE_EXPONENT 0.7
#define BRC_RATE_CMPLX_EXPONENT 0.5

mfxStatus H265BRC::Close()
{
    mfxStatus status = MFX_ERR_NONE;
    if (!m_IsInit)
        return MFX_ERR_NOT_INITIALIZED;    
    
    memset(&m_par, 0, sizeof(m_par));
    memset(&m_hrdState, 0, sizeof(m_hrdState));
    ResetParams();
    m_IsInit = false;
    return status;
}

mfxStatus H265BRC::InitHRD()
{
    m_hrdState.bufFullness = m_hrdState.prevBufFullness= m_par.initialDelayInBytes << 3;
    m_hrdState.underflowQuant = 0;
    m_hrdState.overflowQuant = 999;
    m_hrdState.frameNum = 0;

    m_hrdState.minFrameSize = 0;
    m_hrdState.maxFrameSize = 0;
    
    return MFX_ERR_NONE;
}

mfxU32 H265BRC::GetInitQP()
{
    mfxI32 fs, fsLuma;

  fsLuma = m_par.width * m_par.height;
  fs = fsLuma;

  if (m_par.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (m_par.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (m_par.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * m_par.bitDepthLuma / 8;
  
 
  mfxF64 qstep = pow(1.5 * fs * m_par.frameRate / (m_par.targetbps), 0.8);
 
  mfxI32 q = Qstep2QP(qstep, mQuantOffset) + mQuantOffset;

  BRC_CLIP(q, 1, mQuantMax);

  return q;
}

mfxStatus H265BRC::SetParams( MfxVideoParam &par)
{
    MFX_CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR, MFX_ERR_UNDEFINED_BEHAVIOR );

    m_par.rateControlMethod = par.mfx.RateControlMethod;
    m_par.bufferSizeInBytes  = ((par.BufferSizeInKB*1000) >> (1 + MFX_H265_CPBSIZE_SCALE)) << (1 + MFX_H265_CPBSIZE_SCALE);
    m_par.initialDelayInBytes =((par.InitialDelayInKB*1000) >> (1 + MFX_H265_CPBSIZE_SCALE)) << (1 + MFX_H265_CPBSIZE_SCALE);

    m_par.targetbps = (((par.TargetKbps*1000) >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));
    m_par.maxbps =    (((par.MaxKbps*1000) >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));

    MFX_CHECK (par.mfx.FrameInfo.FrameRateExtD != 0 && par.mfx.FrameInfo.FrameRateExtN != 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_par.frameRate = (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD;
    m_par.width = par.mfx.FrameInfo.Width;
    m_par.height =par.mfx.FrameInfo.Height;
    m_par.chromaFormat = par.mfx.FrameInfo.ChromaFormat;
    m_par.bitDepthLuma = par.mfx.FrameInfo.BitDepthLuma;

    m_par.inputBitsPerFrame = m_par.maxbps / m_par.frameRate;
    m_par.gopPicSize = par.mfx.GopPicSize;
    m_par.gopRefDist = par.mfx.GopRefDist;
 
    return MFX_ERR_NONE;
}

mfxStatus H265BRC::Init( MfxVideoParam &par, mfxI32 enableRecode)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = SetParams(par);
    MFX_CHECK_STS(sts);

    //mLowres = video.LowresFactor ? 1 : 0;

    mRecode = enableRecode;

    if (mRecode)
        InitHRD();

    
    mBitsDesiredFrame = (mfxI32)(m_par.targetbps / m_par.frameRate);
    
    MFX_CHECK(mBitsDesiredFrame > 10, MFX_ERR_INVALID_VIDEO_PARAM);

    mQuantOffset = 6 * (m_par.bitDepthLuma - 8);
    mQuantMax = 51 + mQuantOffset;
    mQuantMin = 1;
    mMinQp = mQuantMin;


    mBitsDesiredTotal = 0;
    mBitsEncodedTotal = 0;

    mQuantUpdated = 1;
    mRecodedFrame_encOrder = 0;
    m_bRecodeFrame = false;

    mfxI32 q = GetInitQP();

    if (!mRecode) {
        if (q - 6 > 10)
            mQuantMin = IPP_MAX(10, q - 10);
        else
            mQuantMin = IPP_MAX(q - 6, 2);

        if (q < mQuantMin)
            q = mQuantMin;
    }

    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;

    mRCbap = 100;
    mRCqap = 100;
    mRCfap = 100;

    mRCq = q;
    mRCqa = mRCqa0 = 1. / (mfxF64)mRCq;
    mRCfa = mBitsDesiredFrame;
    mRCfa_short = mBitsDesiredFrame;

    mSChPoc = 0;
    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;
    mPicType = MFX_FRAMETYPE_I;

    mMaxQp = 999;
    mMinQp = -1;

    m_IsInit = true;



    return sts;
}




mfxStatus H265BRC::Reset(MfxVideoParam &par, mfxI32 )
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK (mRecode == 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = SetParams(par);
    MFX_CHECK_STS(sts);

    mBitsDesiredFrame = (mfxI32)(m_par.targetbps / m_par.frameRate);    
    MFX_CHECK(mBitsDesiredFrame > 10, MFX_ERR_INVALID_VIDEO_PARAM);

    mRCq = (mfxI32)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5); 
    BRC_CLIP(mRCq, 1, mQuantMax); 
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq; 
    mRCqa = mRCqa0 = 1./mRCq; 
    mRCfa = mBitsDesiredFrame; 
    mRCfa_short = mBitsDesiredFrame; 


    return sts;
}

#define BRC_QSTEP_COMPL_EXPONENT 0.4
#define BRC_QSTEP_COMPL_EXPONENT_AVBR 0.2
#define BRC_CMPLX_DECAY 0.1
#define BRC_CMPLX_DECAY_AVBR 0.2
#define BRC_MIN_CMPLX 0.01
#define BRC_MIN_CMPLX_LAYER 0.05
#define BRC_MIN_CMPLX_LAYER_I 0.01

static const mfxF64 brc_qstep_factor[8] = {pow(2., -1./6.), 1., pow(2., 1./6.),   pow(2., 2./6.), pow(2., 3./6.), pow(2., 4./6.), pow(2., 5./6.), 2.};
static const mfxF64 minQstep = QSTEP1[0];
static const mfxF64 maxQstepChange = pow(2, 0.5);
static const mfxF64 predCoef = 1000000. / (1920 * 1080);

mfxU16 GetFrameType(Task &task)
{
    if (task.m_frameType & MFX_FRAMETYPE_I)
        return MFX_FRAMETYPE_I;
    if ((task.m_frameType & MFX_FRAMETYPE_B) && !task.m_ldb)
        return MFX_FRAMETYPE_B;

    return MFX_FRAMETYPE_P;
}

mfxI32 H265BRC::GetQP(MfxVideoParam &video, Task &task)
{
    //printf("mQuant I %d, P %d B %d, m_bRecodeFrame %d, mQuantOffset %d, max - min %d, %d\n ",mQuantI,mQuantP, mQuantB, m_bRecodeFrame, mQuantOffset, mQuantMax, mQuantMin ); 
    mfxI32 qp = mQuantB;

    if (task.m_eo == mRecodedFrame_encOrder && m_bRecodeFrame)
        qp = mQuantRecoded;
    else {
        mfxU16 type = GetFrameType(task);

        if (type == MFX_FRAMETYPE_I) 
            qp = mQuantI;
        else if (type == MFX_FRAMETYPE_P) 
            qp = mQuantP;
        else { 
            if (video.isBPyramid()) {
                qp = mQuantB + task.m_level - 1;
                BRC_CLIP(qp, 1, mQuantMax);
            } else
                qp = mQuantB;
        }
    }

    return qp - mQuantOffset;
}

mfxStatus H265BRC::SetQP(mfxI32 qp, mfxU16 frameType, bool bLowDelay)
{
    if (MFX_FRAMETYPE_B == frameType && !bLowDelay) {
        mQuantB = qp + mQuantOffset;
        BRC_CLIP(mQuantB, 1, mQuantMax);
    } else {
        mRCq = qp + mQuantOffset;
        BRC_CLIP(mRCq, 1, mQuantMax);
        mQuantI = mQuantP = mRCq;
    }
    return MFX_ERR_NONE;
}


mfxBRCStatus H265BRC::UpdateAndCheckHRD(mfxI32 frameBits, mfxF64 inputBitsPerFrame, mfxI32 recode)
{
    mfxBRCStatus ret = MFX_ERR_NONE;
    mfxF64 buffSizeInBits = m_par.bufferSizeInBytes <<3;

    if (!(recode & (MFX_BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        m_hrdState.prevBufFullness = m_hrdState.bufFullness;
        m_hrdState.underflowQuant = 0;
        m_hrdState.overflowQuant = 999;
    } else { // frame is being recoded - restore buffer state
        m_hrdState.bufFullness = m_hrdState.prevBufFullness;
    }

    m_hrdState.maxFrameSize = (mfxI32)(m_hrdState.bufFullness - 1);
    m_hrdState.minFrameSize = (m_par.rateControlMethod != MFX_RATECONTROL_CBR ? 0 : (mfxI32)(m_hrdState.bufFullness + 1 + 1 + inputBitsPerFrame - buffSizeInBits));
    if (m_hrdState.minFrameSize < 0)
        m_hrdState.minFrameSize = 0;

   mfxF64  bufFullness = m_hrdState.bufFullness - frameBits;

    if (bufFullness < 2) {
        bufFullness = inputBitsPerFrame;
        ret = MFX_BRC_ERR_BIG_FRAME;
        if (bufFullness > buffSizeInBits)
            bufFullness = buffSizeInBits;
    } else {
        bufFullness += inputBitsPerFrame;
        if (bufFullness > buffSizeInBits - 1) {
            bufFullness = buffSizeInBits - 1;
            if (m_par.rateControlMethod == MFX_RATECONTROL_CBR)
                ret = MFX_BRC_ERR_SMALL_FRAME;
        }
    }
    if (MFX_ERR_NONE == ret)
        m_hrdState.frameNum++;
    else if ((recode & MFX_BRC_EXT_FRAMESKIP) || MFX_BRC_RECODE_EXT_PANIC == recode || MFX_BRC_RECODE_PANIC == recode) // no use in changing QP
        ret |= MFX_BRC_NOT_ENOUGH_BUFFER;

    m_hrdState.bufFullness = bufFullness;

    return ret;
}

#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 10.0
#define BRC_RCFAP_SHORT 5

#define I_WEIGHT 1.2
#define P_WEIGHT 0.25
#define B_WEIGHT 0.2

#define BRC_MAX_LOAN_LENGTH 75
#define BRC_LOAN_RATIO 0.075

#define BRC_BIT_LOAN \
{ \
    if (picType == MFX_FRAMETYPE_I) { \
        if (mLoanLength) \
            bitsEncoded += mLoanLength * mLoanBitsPerFrame; \
        mLoanLength = video->GopPicSize; \
        if (mLoanLength > BRC_MAX_LOAN_LENGTH || mLoanLength == 0) \
            mLoanLength = BRC_MAX_LOAN_LENGTH; \
        mfxI32 bitsEncodedI = (mfxI32)((mfxF64)bitsEncoded  / (mLoanLength * BRC_LOAN_RATIO + 1)); \
        mLoanBitsPerFrame = (bitsEncoded - bitsEncodedI) / mLoanLength; \
        bitsEncoded = bitsEncodedI; \
    } else if (mLoanLength) { \
        bitsEncoded += mLoanBitsPerFrame; \
        mLoanLength--; \
    } \
}





void H265BRC::SetParamsForRecoding (mfxI32 encOrder)
{
    mQuantUpdated = 0;
    m_bRecodeFrame = true;
    mRecodedFrame_encOrder = encOrder;
}
 bool  H265BRC::isFrameBeforeIntra (mfxU32 order)
 {
     mfxI32 distance0 = m_par.gopPicSize*3/4;
     mfxI32 distance1 = m_par.gopPicSize - m_par.gopRefDist*3;
     return (order - mEOLastIntra) > (mfxU32)(IPP_MAX(distance0, distance1));
 }


mfxBRCStatus H265BRC::PostPackFrame(MfxVideoParam & /* par */, Task &task, mfxI32 totalFrameBits, mfxI32 overheadBits, mfxI32 repack)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;

    mfxI32 bitsEncoded = totalFrameBits - overheadBits;
    mfxF64 e2pe;
    mfxI32 qp, qpprev;
    mfxU32 prevFrameType = mPicType;
    mfxU32 picType = GetFrameType(task);
    mfxI32 qpY = task.m_qpY;

    mPoc = task.m_poc;
    qpY += mQuantOffset;

    mfxI32 layer = ((picType == MFX_FRAMETYPE_I) ? 0 : ((picType == MFX_FRAMETYPE_P) ?  1 : 1 + IPP_MAX(1, task.m_level))); // should be 0 for I, 1 for P, etc. !!!
    mfxF64 qstep = QP2Qstep(qpY, mQuantOffset);

    if (picType == MFX_FRAMETYPE_I)
        mEOLastIntra = task.m_eo;

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mRecode |= 2;
        mQp = mRCq;
        UpdateQuant(mBitsEncoded, totalFrameBits, layer);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;

    mBitsEncoded = bitsEncoded;

    if (mSceneChange)
        if (mQuantUpdated == 1 && mPoc > mSChPoc + 1)
            mSceneChange &= ~16;


    mfxF64 buffullness = 1.e12; // a big number
    
    if (mRecode)
    {
        buffullness = repack ? m_hrdState.prevBufFullness : m_hrdState.bufFullness;
        Sts = UpdateAndCheckHRD(totalFrameBits, m_par.inputBitsPerFrame, repack);
    }

    qpprev = qp = mQp = qpY;

    mfxF64 fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

    {
        qstep = QP2Qstep(qp, mQuantOffset);
        mfxF64 qstep_prev = QP2Qstep(mQPprev, mQuantOffset);
        mfxF64 frameFactor = 1.0;
        mfxF64 targetFrameSize = IPP_MAX((mfxF64)mBitsDesiredFrame, mRCfa);
        if (picType & MFX_FRAMETYPE_I)
            frameFactor = 3.0;

        e2pe = bitsEncoded * sqrt(qstep) / (mBitsEncodedPrev * sqrt(qstep_prev));

        mfxF64 maxFrameSize;
        maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
        BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor);

        mfxF64 famax = 1./9. * buffullness + 8./9. * mRCfa;

        mfxI32 maxqp = mQuantMax;
        if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && mRecode ) {
            maxqp = IPP_MIN(maxqp, m_hrdState.overflowQuant - 1);
        }

        if (bitsEncoded >  maxFrameSize && qp < maxqp ) {
            mfxF64 targetSizeScaled = maxFrameSize * 0.8;
            mfxF64 qstepnew = qstep * pow(bitsEncoded / targetSizeScaled, 0.9);
            mfxI32 qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
              qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            if (qpnew > qp) {
                mQp = mRCq = mQuantI = mQuantP = qpnew;
                if (picType & MFX_FRAMETYPE_B)
                    mQuantB = qpnew;
                else {
                    SetQuantB();
                }

                mRCfa_short = fa_short0;

                if (e2pe > BRC_SCENE_CHANGE_RATIO1) { // scene change, resetting BRC statistics
                  mRCfa = mBitsDesiredFrame;
                  mRCqa = 1./qpnew;
                  mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;
                  mSceneChange |= 1;
                  if (picType != MFX_FRAMETYPE_B) {
                      mSceneChange |= 16;
                      mSChPoc = mPoc;
                  }
                  mRCfa_short = mBitsDesiredFrame;
                }
                if (mRecode) {
                    SetParamsForRecoding (task.m_eo);
                    m_hrdState.frameNum--;
                    mMinQp = qp;
                    mQuantRecoded = qpnew;
                    //printf("recode1 %d %d %d %d \n", task.m_eo, qpnew, bitsEncoded ,  (int)maxFrameSize);
                    return MFX_BRC_ERR_BIG_FRAME;
                }
            }
        }

        if (bitsEncoded >  maxFrameSize && qp == maxqp && picType != MFX_FRAMETYPE_I && mRecode && (!task.m_bSkipped) && isFrameBeforeIntra(task.m_eo)) //skip frames before intra
        {
            SetParamsForRecoding(task.m_eo);
            m_hrdState.frameNum--;
            return MFX_BRC_ERR_BIG_FRAME|MFX_BRC_NOT_ENOUGH_BUFFER;
        }

        if (mRCfa_short > famax && (!repack) && qp < maxqp) {

            mfxF64 qstepnew = qstep * mRCfa_short / (famax * 0.8);
            mfxI32 qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
                qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            mRCfa = mBitsDesiredFrame;
            mRCqa = 1./qpnew;
            mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;

            mRCfa_short = mBitsDesiredFrame;

            if (mRecode) {
                SetParamsForRecoding (task.m_eo);
                m_hrdState.frameNum--;
                mMinQp = qp;
                mQuantRecoded = qpnew;

            }
        }
    }

    mPicType = picType;

    mfxF64 fa = mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (mfxF64)mBitsEncodedP) && (mfxF64)mBitsEncoded < 1.5*fa)
        oldScene = true;

    if (Sts != MFX_BRC_OK && mRecode) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, overheadBits, layer, task.m_eo == mRecodedFrame_encOrder && m_bRecodeFrame);
        SetParamsForRecoding(task.m_eo);
        mPicType = prevFrameType;
        mRCfa_short = fa_short0;
    } else {
        if (mQuantUpdated == 0 && 1./qp < mRCqa)
            mRCqa += (1. / qp - mRCqa) / 16;
        else if (mQuantUpdated == 0)
            mRCqa += (1. / qp - mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
        else
            mRCqa += (1. / qp - mRCqa) / mRCqap;

        BRC_CLIP(mRCqa, 1./mQuantMax , 1./1.);

        if (repack != MFX_BRC_RECODE_PANIC && repack != MFX_BRC_RECODE_EXT_PANIC && !oldScene) {
            mQPprev = qp;
            mBitsEncodedPrev = mBitsEncoded;

            Sts = UpdateQuant(bitsEncoded, totalFrameBits,  layer, task.m_eo == mRecodedFrame_encOrder && m_bRecodeFrame);

            if (mPicType != MFX_FRAMETYPE_B) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }

            mQuantP = mQuantI = mRCq;
        }
        mQuantUpdated = 1;
        //    mMaxBitsPerPic = mMaxBitsPerPicNot0;

        if (mRecode)
        {
            m_hrdState.underflowQuant = 0;
            m_hrdState.overflowQuant = 999;
        }

        mMinQp = -1;
        mMaxQp = 999;
    }

#ifdef PRINT_BRC_STATS
    if (Sts & 1)
        brc_fprintf("underflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
    else if (Sts & 4)
        brc_fprintf("overflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
#endif

    return Sts;
};
void H265BRC::ResetParams()
{
    mQuantI = mQuantP= mQuantB= mQuantMax= mQuantMin= mQuantPrev= mQuantOffset= mQPprev=0;
    mMinQp=0;
    mMaxQp=0;
    mBitsDesiredFrame=0;
    mQuantUpdated=0;
    mRecodedFrame_encOrder=0;
    m_bRecodeFrame=false;
    mPicType=0;
    mRecode=0;
    mBitsEncodedTotal= mBitsDesiredTotal=0;
    mQp=0;
    mRCfap= mRCqap= mRCbap= mRCq=0;
    mRCqa= mRCfa= mRCqa0=0;
    mRCfa_short=0;
    mQuantRecoded=0;
    mQuantIprev= mQuantPprev= mQuantBprev=0;
    mBitsEncoded=0;
    mSceneChange=0;
    mBitsEncodedP= mBitsEncodedPrev=0;
    mPoc= mSChPoc=0;
    mEOLastIntra = 0;
}
mfxBRCStatus H265BRC::UpdateQuant(mfxI32 bitEncoded, mfxI32 totalPicBits, mfxI32 , mfxI32 recode)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    mfxF64  bo = 0, qs = 0, dq = 0;
    mfxI32  quant = (recode) ? mQuantRecoded : mQp;
    mfxU32 bitsPerPic = (mfxU32)mBitsDesiredFrame;
    mfxI64 totalBitsDeviation = 0;
    mfxF64 buffSizeInBits = m_par.bufferSizeInBytes <<3;

    if (mRecode & 2) {
        mRCfa = bitsPerPic;
        mRCqa = mRCqa0;
        mRecode &= ~2;
    }

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    if (mRecode) 
    {
        if (m_par.rateControlMethod == MFX_RATECONTROL_VBR) {
            mfxI64 targetFullness = IPP_MIN(m_par.initialDelayInBytes << 3, (mfxU32)buffSizeInBits / 2);
            mfxI64 minTargetFullness = IPP_MIN(mfxU32(buffSizeInBits / 2), m_par.targetbps * 2); // half bufsize or 2 sec
            if (targetFullness < minTargetFullness)
                targetFullness = minTargetFullness;
                mfxI64 bufferDeviation = targetFullness - (mfxI64)m_hrdState.bufFullness;
                if (bufferDeviation > totalBitsDeviation)
                    totalBitsDeviation = bufferDeviation;
        }
    }

    if (mPicType != MFX_FRAMETYPE_I || m_par.rateControlMethod == MFX_RATECONTROL_CBR || mQuantUpdated == 0)
        mRCfa += (bitEncoded - mRCfa) / mRCfap;
    SetQuantB();
    if (mQuantUpdated == 0)
        if (mQuantB < quant)
            mQuantB = quant;
    qs = pow(bitsPerPic / mRCfa, 2.0);
    dq = mRCqa * qs;

    mfxI32 bap = mRCbap;
    if (mRecode)
    {
        mfxF64 bfRatio = m_hrdState.bufFullness / mBitsDesiredFrame;
        if (totalBitsDeviation > 0) {
            bap = (mfxI32)bfRatio*3;
            bap = IPP_MAX(bap, 10);
            BRC_CLIP(bap, mRCbap/10, mRCbap);
        }
    }

    bo = (mfxF64)totalBitsDeviation / bap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./mQuantMin);
    quant = (mfxI32) (1. / dq + 0.5);

    if (quant >= mRCq + 5)
        quant = mRCq + 3;
    else if (quant >= mRCq + 3)
        quant = mRCq + 2;
    else if (quant > mRCq + 1)
        quant = mRCq + 1;
    else if (quant <= mRCq - 5)
        quant = mRCq - 3;
    else if (quant <= mRCq - 3)
        quant = mRCq - 2;
    else if (quant < mRCq - 1)
        quant = mRCq - 1;

    mRCq = quant;

    if (mRecode)
    {
        mfxF64 qstep = QP2Qstep(quant, mQuantOffset);
        mfxF64 fullnessThreshold = MIN(bitsPerPic * 12, buffSizeInBits*3/16);
        qs = 1.0;
        if (bitEncoded > m_hrdState.bufFullness && mPicType != MFX_FRAMETYPE_I)
            qs = (mfxF64)bitEncoded / (m_hrdState.bufFullness);

        if (m_hrdState.bufFullness < fullnessThreshold && (mfxU32)totalPicBits > bitsPerPic)
            qs *= sqrt((mfxF64)fullnessThreshold * 1.3 / m_hrdState.bufFullness); // ??? is often useless (quant == quant_old)

        if (qs > 1.0) {
            qstep *= qs;
            quant = Qstep2QP(qstep, mQuantOffset);
            if (mRCq == quant)
                quant++;

            BRC_CLIP(quant, 1, mQuantMax);

            mQuantB = ((quant + quant) * 563 >> 10) + 1;
            BRC_CLIP(mQuantB, 1, mQuantMax);
            mRCq = quant;
        }
    }
    return Sts;
}

mfxBRCStatus H265BRC::UpdateQuantHRD(mfxI32 totalFrameBits, mfxBRCStatus sts, mfxI32 overheadBits, mfxI32 , mfxI32 recode)
{
    mfxI32 quant, quant_prev;
    mfxI32 wantedBits = (sts == MFX_BRC_ERR_BIG_FRAME ? m_hrdState.maxFrameSize * 3 / 4 : m_hrdState.minFrameSize * 5 / 4);
    mfxI32 bEncoded = totalFrameBits - overheadBits;
    mfxF64 qstep, qstep_new;

    wantedBits -= overheadBits;
    if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    if (recode)
        quant_prev = quant = mQuantRecoded;
    else
        //quant_prev = quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : ((mPicType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));
        quant_prev = quant = mQp;

    if (sts & MFX_BRC_ERR_BIG_FRAME)
        m_hrdState.underflowQuant = quant;
    else if (sts & MFX_BRC_ERR_SMALL_FRAME)
        m_hrdState.overflowQuant = quant;

    qstep = QP2Qstep(quant, mQuantOffset);
    qstep_new = qstep * sqrt((mfxF64)bEncoded / wantedBits);
//    qstep_new = qstep * sqrt(sqrt((mfxF64)bEncoded / wantedBits));
    quant = Qstep2QP(qstep_new, mQuantOffset);
    BRC_CLIP(quant, 1, mQuantMax);

    if (sts & MFX_BRC_ERR_SMALL_FRAME) // overflow
    {
        mfxI32 qpMin = IPP_MAX(m_hrdState.underflowQuant, mMinQp);
        if (qpMin > 0) {
            if (quant < (qpMin + quant_prev + 1) >> 1)
                quant = (qpMin + quant_prev + 1) >> 1;
        }
        if (quant > quant_prev - 1)
            quant = quant_prev - 1;
        if (quant < m_hrdState.underflowQuant + 1)
            quant = m_hrdState.underflowQuant + 1;
        if (quant < mMinQp + 1 && quant_prev > mMinQp + 1)
            quant = mMinQp + 1;
    } 
    else // underflow
    {
        if (m_hrdState.overflowQuant <= mQuantMax) {
            if (quant > (quant_prev + m_hrdState.overflowQuant + 1) >> 1)
                quant = (quant_prev + m_hrdState.overflowQuant + 1) >> 1;
        }
        if (quant < quant_prev + 1)
            quant = quant_prev + 1;
        if (quant > m_hrdState.overflowQuant - 1)
            quant = m_hrdState.overflowQuant - 1;
    }

   if (quant == quant_prev)
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    mQuantRecoded = quant;

    return sts;
}

/*mfxStatus H265BRC::GetInitialCPBRemovalDelay(mfxU32 *initial_cpb_removal_delay, mfxI32 recode)
{
    mfxU32 cpb_rem_del_u32;
    mfxU64 cpb_rem_del_u64, temp1_u64, temp2_u64;

    if (MFX_RATECONTROL_VBR == m_par.rateControlMethod) {
        if (recode)
            mBF = mBFsaved;
        else
            mBFsaved = mBF;
    }

    temp1_u64 = (mfxU64)mBF * 90000;
    temp2_u64 = (mfxU64)m_par.maxbps/8 ;
    cpb_rem_del_u64 = temp1_u64 / temp2_u64;
    cpb_rem_del_u32 = (mfxU32)cpb_rem_del_u64;

    if (MFX_RATECONTROL_VBR == m_par.rateControlMethod) {
        mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
        temp1_u64 = (mfxU64)cpb_rem_del_u32 * m_par.maxbps/8;
        mfxU32 dec_buf_ful = (mfxU32)(temp1_u64 / (90000/8));
        if (recode)
            m_hrdState.prevBufFullness = (mfxF64)dec_buf_ful;
        else
            m_hrdState.bufFullness = (mfxF64)dec_buf_ful;
    }

    *initial_cpb_removal_delay = cpb_rem_del_u32;
    return MFX_ERR_NONE;
}*/

void  H265BRC::GetMinMaxFrameSize(mfxI32 *minFrameSizeInBits, mfxI32 *maxFrameSizeInBits)
{
    if (minFrameSizeInBits)
      *minFrameSizeInBits = m_hrdState.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = m_hrdState.maxFrameSize;
}
#endif

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace MfxHwH265EncodeBRC
{
#ifdef NEW_BRC
#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))
#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 5.0


mfxExtBuffer* Hevc_GetExtBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
        {
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
        }
    }

    return 0;
}

mfxStatus cBRCParams::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(par->mfx.RateControlMethod == MFX_RATECONTROL_CBR || 
              par->mfx.RateControlMethod == MFX_RATECONTROL_VBR, 
              MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 k = par->mfx.BRCParamMultiplier == 0 ?  1: par->mfx.BRCParamMultiplier;

    rateControlMethod  = par->mfx.RateControlMethod;
    targetbps = (((k*par->mfx.TargetKbps*1000) >> 6) << 6 );
    maxbps =    (((k*par->mfx.MaxKbps*1000) >> 6) << 6 );

    mfxExtCodingOption * pExtCO = (mfxExtCodingOption*)Hevc_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION);

    bHRDConformance =  (pExtCO && pExtCO->NalHrdConformance == MFX_CODINGOPTION_OFF) ? 0 : 1;

    if (bHRDConformance)
    {
        bufferSizeInBytes  = ((k*par->mfx.BufferSizeInKB*1000) >> 3) << 3;
        initialDelayInBytes =((k*par->mfx.InitialDelayInKB*1000) >> 3) << 3;
        bRec = 1;
        bPanic = 1;
    }
    MFX_CHECK (par->mfx.FrameInfo.FrameRateExtD != 0 && 
               par->mfx.FrameInfo.FrameRateExtN != 0, 
               MFX_ERR_UNDEFINED_BEHAVIOR);

    frameRate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / (mfxF64)par->mfx.FrameInfo.FrameRateExtD;
    
    width = par->mfx.FrameInfo.Width;
    height =par->mfx.FrameInfo.Height;

    chromaFormat = par->mfx.FrameInfo.ChromaFormat;
    bitDepthLuma = par->mfx.FrameInfo.BitDepthLuma;
    quantOffset   = 6 * (bitDepthLuma - 8);

    inputBitsPerFrame    = (mfxU32)(targetbps / frameRate);
    maxInputBitsPerFrame = (mfxU32)(maxbps / frameRate);
    gopPicSize = par->mfx.GopPicSize;
    gopRefDist = par->mfx.GopRefDist;

    mfxExtCodingOption2 * pExtCO2 = (mfxExtCodingOption2*)Hevc_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
    bPyr = (pExtCO2 && pExtCO2->BRefType == MFX_B_REF_PYRAMID);
    fAbPeriodLong  = 100;
    fAbPeriodShort = 5;
    dqAbPeriod = 100;
    bAbPeriod = 100;


    quantMax = 51 + quantOffset;
    quantMin = 1;

 
    return MFX_ERR_NONE;
}



enum 
{
    MFX_BRC_RECODE_NONE           = 0,
    MFX_BRC_RECODE_QP             = 1,
    MFX_BRC_RECODE_PANIC          = 2,
};

   mfxF64 const QSTEP[88] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
        256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
        1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
        4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
    };


mfxI32 QStep2QpFloor(mfxF64 qstep, mfxI32 qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
{
    Ipp8u qp = Ipp8u(std::upper_bound(QSTEP, QSTEP + 51 + qpoffset, qstep) - QSTEP);
    return qp > 0 ? qp - 1 : 0;
}

mfxI32 Qstep2QP(mfxF64 qstep, mfxI32 qpoffset = 0) // return 0<=qp<=51+mQuantOffset
{
    mfxI32 qp = QStep2QpFloor(qstep, qpoffset);
    return (qp == 51 + qpoffset || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
}
mfxF64 QP2Qstep(mfxI32 qp, mfxI32 qpoffset = 0)
{
    return QSTEP[IPP_MIN(51 + qpoffset, qp)];
}
#define BRC_CLIP(val, minval, maxval) val = Saturate(minval, maxval, val)



mfxF64  cHRD::GetBufferDiviation(mfxU32 targetBitrate)
{
    mfxI64 targetFullness = IPP_MIN(m_delayInBits, m_buffSizeInBits / 2);
    mfxI64 minTargetFullness = IPP_MIN(mfxU32(m_buffSizeInBits / 2),targetBitrate * 2); // half bufsize or 2 sec
    targetFullness = IPP_MAX(targetFullness , minTargetFullness);
     return targetFullness - m_bufFullness;
}

mfxU16 cHRD::UpdateAndCheckHRD(mfxI32 frameBits, mfxI32 recode, mfxI32 minQuant, mfxI32 maxQuant)
{
    mfxU16 brcStatus = MFX_BRC_OK ;

    if (recode  == 0) 
    { 
        m_prevBufFullness = m_bufFullness;
        m_underflowQuant = minQuant - 1;
        m_overflowQuant  = maxQuant + 1;
    } 
    else 
    { // frame is being recoded - restore buffer state
        m_bufFullness = m_prevBufFullness;
        m_frameNum--;
    }

    m_maxFrameSize = (mfxI32)(m_bufFullness - 1);
    m_minFrameSize = (!m_bCBR)? 0 : (mfxI32)(m_bufFullness + 1 + 1 + m_inputBitsPerFrame - m_buffSizeInBits);
    if (m_minFrameSize < 0)
        m_minFrameSize = 0;

   mfxF64  bufFullness = m_bufFullness - frameBits;

    if (bufFullness < 2) 
    {
        bufFullness = m_inputBitsPerFrame;
        brcStatus = MFX_BRC_BIG_FRAME;
        if (bufFullness > m_buffSizeInBits)
            bufFullness = m_buffSizeInBits;
    } 
    else 
    {
        bufFullness += m_inputBitsPerFrame;
        if (bufFullness > m_buffSizeInBits - 1) 
        {
            bufFullness = m_buffSizeInBits - 1;
            if (m_bCBR)
                brcStatus = MFX_BRC_SMALL_FRAME;
        }
    }
    m_frameNum++;
    if ( MFX_BRC_RECODE_PANIC == recode) // no use in changing QP
    {
        if (brcStatus == MFX_BRC_SMALL_FRAME)
            brcStatus =  MFX_BRC_PANIC_SMALL_FRAME ;
        if (brcStatus == MFX_BRC_BIG_FRAME)
            brcStatus =  MFX_BRC_PANIC_BIG_FRAME ;    }

    m_bufFullness = bufFullness;
    return brcStatus;
}

mfxStatus cHRD::UpdateMinMaxQPForRec( mfxU32 brcSts, mfxI32 qp)
{
    MFX_CHECK(brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME, MFX_ERR_UNDEFINED_BEHAVIOR);
    if (brcSts == MFX_BRC_BIG_FRAME)
        m_underflowQuant = qp;
    else 
        m_overflowQuant = qp;
    return MFX_ERR_NONE;
}
mfxI32 cHRD::GetTargetSize(mfxU32 brcSts)
{
     if (brcSts != MFX_BRC_BIG_FRAME && brcSts != MFX_BRC_SMALL_FRAME) return 0;
     return (brcSts == MFX_BRC_BIG_FRAME) ? m_maxFrameSize * 3 / 4 : m_minFrameSize * 5 / 4;
}

mfxI32 GetNewQP(mfxF64 totalFrameBits, mfxF64 targetFrameSizeInBits, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp , mfxI32 qp_offset, mfxF64 f_pow, bool bStrict = false)
{    
    mfxF64 qstep = 0, qstep_new = 0;
    mfxI32 qp_new = qp;

    qstep = QP2Qstep(qp, qp_offset);
    qstep_new = qstep * pow(totalFrameBits / targetFrameSizeInBits, f_pow);
    qp_new = Qstep2QP(qstep_new, qp_offset);

    if (totalFrameBits < targetFrameSizeInBits) // overflow
    {
        if (qp <= minQP)
        {
            return qp; // QP change is impossible
        }        
        qp_new  = IPP_MAX (qp_new , (minQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = IPP_MIN (qp_new, qp - 1);
    } 
    else // underflow
    {
        if (qp >= maxQP)
        {
            return qp; // QP change is impossible
        }        
        qp_new  = IPP_MIN (qp_new , (maxQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = IPP_MAX (qp_new, qp + 1);
    }
    return qp_new;
}




void cHRD::Init(mfxU32 buffSizeInBytes, mfxU32 delayInBytes, mfxU32 inputBitsPerFrame, bool bCBR)
{
    m_bufFullness = m_prevBufFullness= delayInBytes << 3;
    m_delayInBits = delayInBytes << 3;
    m_buffSizeInBits = buffSizeInBytes << 3;
    m_inputBitsPerFrame =inputBitsPerFrame;
    m_bCBR = bCBR;

    m_underflowQuant = 0;
    m_overflowQuant = 999;
    m_frameNum = 0;
    m_minFrameSize = 0;
    m_maxFrameSize = 0;

}

    
void UpdateQPParams(mfxI32 qp, mfxU32 type , BRC_Ctx  &ctx, mfxU32 /* rec_num */, mfxI32 minQuant, mfxI32 maxQuant, mfxU32 level)
{
    ctx.Quant = qp;
    if (type == MFX_FRAMETYPE_I)
    {
        ctx.QuantI = qp;
        ctx.QuantP = qp + 1;
        ctx.QuantB = qp + 2;
    }
    else if (type == MFX_FRAMETYPE_P)
    {
        qp -= level;
        ctx.QuantI = (3*ctx.QuantI + qp ) >> 2;
        ctx.QuantI = IPP_MIN(ctx.QuantI, qp - 1);
        ctx.QuantP = qp;
        ctx.QuantB = qp + 1;
    }
    else if (type == MFX_FRAMETYPE_B)
    {
        level = level > 0 ? level - 1: 0;
        qp -= level;
        ctx.QuantI = (3*ctx.QuantI + qp ) >> 2;
        ctx.QuantI = IPP_MIN(ctx.QuantI, qp - 2);
        ctx.QuantP = qp - 1;
        ctx.QuantB = qp;
    }
    BRC_CLIP(ctx.QuantI, minQuant, maxQuant);
    BRC_CLIP(ctx.QuantP, minQuant, maxQuant);
    BRC_CLIP(ctx.QuantB, minQuant, maxQuant);
}

mfxI32 GetRawFrameSize(mfxU32 lumaSize, mfxU16 chromaFormat, mfxU16 bitDepthLuma)
{
    mfxI32 frameSize = lumaSize;

  if (chromaFormat == MFX_CHROMAFORMAT_YUV420)
    frameSize += lumaSize / 2;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV422)
   frameSize += lumaSize;
  else if (chromaFormat == MFX_CHROMAFORMAT_YUV444)
    frameSize += lumaSize * 2;

  frameSize = frameSize * bitDepthLuma / 8;
  return frameSize*8; //frame size in bits
}
  
 

mfxStatus ExtBRC::Init (mfxVideoParam* par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = m_par.Init(par);
    MFX_CHECK_STS(sts);

    if (m_par.bHRDConformance)
    {
        m_hrd.Init(m_par.bufferSizeInBytes, m_par.initialDelayInBytes, m_par.maxInputBitsPerFrame, m_par.rateControlMethod == MFX_RATECONTROL_CBR);
    }
    memset(&m_ctx, 0, sizeof(m_ctx));

    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
    m_ctx.fAbShort = m_par.inputBitsPerFrame;

    mfxI32 rawSize = GetRawFrameSize(m_par.width * m_par.height ,m_par.chromaFormat, m_par.bitDepthLuma);
    mfxI32 qp = GetNewQP(rawSize, m_par.inputBitsPerFrame, m_par.quantMin, m_par.quantMax, 1 , m_par.quantOffset, 0.5);

    UpdateQPParams(qp,MFX_FRAMETYPE_I , m_ctx, 0, m_par.quantMin, m_par.quantMax, 0);

    m_ctx.dQuantAb = 1./qp;

    m_bInit = true;
    return sts;
}

mfxU16 GetFrameType(mfxU16 m_frameType, mfxU16 level, mfxU16 gopRegDist)
{
    if (m_frameType & MFX_FRAMETYPE_IDR)
        return MFX_FRAMETYPE_I;
    else if (m_frameType & MFX_FRAMETYPE_I)
        return MFX_FRAMETYPE_I;
    else if (m_frameType & MFX_FRAMETYPE_P)
        return MFX_FRAMETYPE_P;
    else if ((m_frameType & MFX_FRAMETYPE_REF) && (level == 0 || gopRegDist == 1))
        return MFX_FRAMETYPE_P; //low delay B
    else
        return MFX_FRAMETYPE_B;
}


bool  isFrameBeforeIntra (mfxU32 order, mfxU32 intraOrder, mfxU32 gopPicSize, mfxU32 gopRefDist)
 {
     mfxI32 distance0 = gopPicSize*3/4;
     mfxI32 distance1 = gopPicSize - gopRefDist*3;
     return (order - intraOrder) > (mfxU32)(IPP_MAX(distance0, distance1));
 }
mfxStatus SetRecodeParams(mfxU16 brcStatus, mfxI32 qp, mfxI32 qp_new, mfxI32 minQP, mfxI32 maxQP, BRC_Ctx &ctx, mfxBRCFrameStatus* status)
{
    ctx.bToRecode = 1;
  
    if (brcStatus == MFX_BRC_BIG_FRAME || brcStatus == MFX_BRC_PANIC_BIG_FRAME )
    {
         MFX_CHECK(qp_new >= qp, MFX_ERR_UNDEFINED_BEHAVIOR);
         ctx.Quant = qp_new;
         ctx.QuantMax = maxQP;
         if (brcStatus == MFX_BRC_BIG_FRAME && qp_new > qp)
         {
            ctx.QuantMin = IPP_MAX(qp + 1, minQP); //limit QP range for recoding
            status->BRCStatus = MFX_BRC_BIG_FRAME;

         }
         else
         {
             ctx.QuantMin = minQP;
             ctx.bPanic = 1;
             status->BRCStatus = MFX_BRC_PANIC_BIG_FRAME;
         }
         
    }
    else if (brcStatus == MFX_BRC_SMALL_FRAME || brcStatus == MFX_BRC_PANIC_SMALL_FRAME)
    {
         MFX_CHECK(qp_new <= qp, MFX_ERR_UNDEFINED_BEHAVIOR);

         ctx.Quant = qp_new;
         ctx.QuantMin = minQP; //limit QP range for recoding

         if (brcStatus == MFX_BRC_SMALL_FRAME && qp_new < qp)
         {
            ctx.QuantMax = IPP_MIN (qp - 1, maxQP);
            status->BRCStatus = MFX_BRC_SMALL_FRAME;
         }
         else
         {
            ctx.QuantMax = maxQP;
            status->BRCStatus = MFX_BRC_PANIC_SMALL_FRAME;
            ctx.bPanic = 1;
         }
    }
    //printf("recode %d , qp %d new %d, status %d\n", ctx.encOrder, qp, qp_new, status->BRCStatus);
    return MFX_ERR_NONE;
}
mfxI32 GetNewQPTotal(mfxF64 bo, mfxF64 dQP, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp, bool bPyr)
{
    BRC_CLIP(bo, -1.0, 1.0);
    BRC_CLIP(dQP, 1./maxQP, 1./minQP);
    dQP = dQP + (1./maxQP - dQP) * bo;
    BRC_CLIP(dQP, 1./maxQP, 1./minQP);
    mfxI32 quant_new = (mfxI32) (1. / dQP + 0.5);

    if (bPyr)
    {
        if (quant_new >= qp + 5)
            quant_new = qp + 2;
        else if (quant_new > qp + 3)
            quant_new = qp + 1;
        else if (quant_new <= qp - 5)
            quant_new = qp - 2;
        else if (quant_new < qp - 2)
            quant_new = qp - 1;
    }
    else
    {
        if (quant_new >= qp + 5)
            quant_new = qp + 3;
        else if (quant_new > qp + 3)
            quant_new = qp + 2;
        else if (quant_new <= qp - 5)
            quant_new = qp - 3;
        else if (quant_new < qp - 2)
            quant_new = qp - 2;
   
    }

    return quant_new;
}


mfxStatus ExtBRC::Update(mfxBRCFrameParam* frame_par, mfxBRCFrameCtrl* frame_ctrl, mfxBRCFrameStatus* status)
{
    mfxStatus sts       = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(frame_par, frame_ctrl, status);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxU16 &brcSts       = status->BRCStatus;
    status->MinFrameSize  = 0;


    mfxI32 bitsEncoded  = frame_par->CodedFrameSize*8;
    mfxU32 picType      = GetFrameType(frame_par->FrameType, frame_par->PyramidLayer, m_par.gopRefDist);    
    mfxI32 qpY          = frame_ctrl->QpY + m_par.quantOffset;
    mfxI32 layer        = frame_par->PyramidLayer;
    mfxF64 qstep        = QP2Qstep(qpY, m_par.quantOffset);

    mfxF64 fAbLong  = m_ctx.fAbLong   + (bitsEncoded - m_ctx.fAbLong)  / m_par.fAbPeriodLong;
    mfxF64 fAbShort = m_ctx.fAbShort  + (bitsEncoded - m_ctx.fAbShort) / m_par.fAbPeriodShort;
    mfxF64 eRate    = bitsEncoded * sqrt(qstep);
    mfxF64 e2pe     = (m_ctx.eRate == 0) ? (BRC_SCENE_CHANGE_RATIO2 + 1) : eRate / m_ctx.eRate;

    brcSts = MFX_BRC_OK;
    
    if (m_par.bRec && m_ctx.bToRecode &&  (m_ctx.encOrder != frame_par->EncodedOrder || frame_par->NumRecode == 0))
    {
        // Frame must be recoded, but encoder calls BR for another frame
        return MFX_ERR_UNDEFINED_BEHAVIOR;    
    }
    if (frame_par->NumRecode == 0)
    {
        // Set context for new frame
        if (picType == MFX_FRAMETYPE_I)
            m_ctx.LastIEncOrder = frame_par->EncodedOrder;
        m_ctx.encOrder = frame_par->EncodedOrder;
        m_ctx.poc = frame_par->DisplayOrder;
        m_ctx.bToRecode = 0;
        m_ctx.bPanic = 0;
        m_ctx.QuantMin = m_par.quantMin; 
        m_ctx.QuantMax = m_par.quantMax; 
        m_ctx.Quant = qpY;


        if (m_ctx.SceneChange && ( m_ctx.poc > m_ctx.SChPoc + 1 || m_ctx.poc == 0))
            m_ctx.SceneChange &= ~16;

        //printf("m_ctx.SceneChange %d, m_ctx.poc %d, m_ctx.SChPoc, m_ctx.poc %d \n", m_ctx.SceneChange, m_ctx.poc, m_ctx.SChPoc, m_ctx.poc);
    }
    if (e2pe > BRC_SCENE_CHANGE_RATIO2) 
    { 
      // scene change, resetting BRC statistics
        m_ctx.fAbLong  = m_par.inputBitsPerFrame;
        m_ctx.fAbShort = m_par.inputBitsPerFrame;
        m_ctx.SceneChange |= 1;
        if (picType != MFX_FRAMETYPE_B) 
        {
            m_ctx.dQuantAb  = 1./m_ctx.Quant;
            m_ctx.SceneChange |= 16;
            m_ctx.SChPoc = frame_par->DisplayOrder;
            //printf("   m_ctx.SceneChange %d, order %d\n", m_ctx.SceneChange, frame_par->DisplayOrder);
        }

    }
    if (m_par.bHRDConformance)
    {
       //check hrd
        brcSts = m_hrd.UpdateAndCheckHRD(bitsEncoded,frame_par->NumRecode, m_par.quantMin, m_par.quantMax);
        MFX_CHECK(brcSts ==  MFX_BRC_OK || (!m_ctx.bPanic), MFX_ERR_NOT_ENOUGH_BUFFER);
        if (brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME)
            m_hrd.UpdateMinMaxQPForRec(brcSts, qpY);
        status->MinFrameSize = m_hrd.GetMinFrameSize() + 7;
        //printf("%d: poc %d, size %d QP %d (%d %d), HRD sts %d, maxFrameSize %d, type %d \n",frame_par->EncodedOrder, frame_par->DisplayOrder, bitsEncoded, m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax, brcSts,  m_hrd.GetMaxFrameSize(), frame_par->FrameType);
    }

    if (frame_par->NumRecode < 2) 
    // Check other condions for recoding (update qp is it is needed)
    {
        mfxF64 targetFrameSize = IPP_MAX((mfxF64)m_par.inputBitsPerFrame, fAbLong);
        mfxF64 frameFactor = (picType == MFX_FRAMETYPE_I) ? 1.5 : 1.0;
        mfxF64 maxFrameSize = (m_ctx.encOrder == 0 ? BRC_SCENE_CHANGE_RATIO2 : BRC_SCENE_CHANGE_RATIO1) * targetFrameSize * frameFactor;
        mfxI32 quantMax = m_ctx.QuantMax;
        mfxI32 quantMin = m_ctx.QuantMin;
        mfxI32 quant = qpY;


        if (m_par.bHRDConformance)
        {
            maxFrameSize = IPP_MIN(maxFrameSize, 2.5/9. * m_hrd.GetMaxFrameSize() + 6.5/9. * targetFrameSize); 
            quantMax     = IPP_MIN(m_hrd.GetMaxQuant(), quantMax);
            quantMin     = IPP_MAX(m_hrd.GetMinQuant(), quantMin);
        }
        maxFrameSize = IPP_MAX(maxFrameSize, targetFrameSize);

        if (bitsEncoded >  maxFrameSize && quant < quantMax) 
        {
                
            mfxI32 quant_new = GetNewQP(bitsEncoded, (mfxU32)maxFrameSize, quantMin , quantMax, quant ,m_par.quantOffset, 1);
            if (quant_new > quant) 
            {
                //printf("    recode 1-0: %d:  k %5f bitsEncoded %d maxFrameSize %d, targetSize %d, fAbLong %f, inputBitsPerFrame %d, qp %d new %d\n",frame_par->EncodedOrder, bitsEncoded/maxFrameSize, (int)bitsEncoded, (int)maxFrameSize,(int)targetFrameSize, fAbLong, m_par.inputBitsPerFrame, quant, quant_new);

                UpdateQPParams(quant_new ,picType, m_ctx, 0, quantMin , quantMax, layer);

                if (m_par.bRec) 
                {
                    SetRecodeParams(MFX_BRC_BIG_FRAME,quant,quant_new, quantMin, quantMax, m_ctx, status);
                    return sts;
                }
            } //(quant_new > quant) 
        } //bitsEncoded >  maxFrameSize

        if (bitsEncoded >  maxFrameSize && quant == quantMax && 
            picType != MFX_FRAMETYPE_I && m_par.bPanic && 
            (!m_ctx.bPanic) && isFrameBeforeIntra(m_ctx.encOrder, m_ctx.LastIEncOrder, m_par.gopPicSize, m_par.gopRefDist)) 
        {
            //skip frames before intra
            SetRecodeParams(MFX_BRC_PANIC_BIG_FRAME,quant,quant, quantMin ,quantMax, m_ctx, status);
            return sts;
        }
        if (m_par.bHRDConformance && frame_par->NumRecode == 0 && (quant < quantMax))
        {
            mfxF64 FAMax = 1./9. * m_hrd.GetMaxFrameSize() + 8./9. * fAbLong;

            if (fAbShort > FAMax) 
            {
                mfxI32 quant_new = GetNewQP(fAbShort, FAMax, quantMin , quantMax, quant ,m_par.quantOffset, 0.5);
                //printf("    recode 2-0: %d:  FAMax %f, fAbShort %f, quant_new %d\n",frame_par->EncodedOrder, FAMax, fAbShort, quant_new);

                if (quant_new > quant) 
                {
                    UpdateQPParams(quant_new ,picType, m_ctx, 0, quantMin , quantMax, layer);
                    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
                    m_ctx.fAbShort = m_par.inputBitsPerFrame;
                    m_ctx.dQuantAb = 1./quant_new;
                    if (m_par.bRec) 
                    {
                        SetRecodeParams(MFX_BRC_BIG_FRAME,quant,quant_new, quantMin, quantMax, m_ctx, status);
                        return sts;
                    }
                }//quant_new > quant
            }
        }//m_par.bHRDConformance
    }
    if (m_par.bHRDConformance && brcSts != MFX_BRC_OK && m_par.bRec)
    {
        mfxI32 quant = m_ctx.Quant;
        mfxI32 quant_new = quant;
        if (brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME)
        {
            quant_new = GetNewQP(bitsEncoded, m_hrd.GetTargetSize(brcSts), m_ctx.QuantMin , m_ctx.QuantMax,quant,m_par.quantOffset, 1, true);
        }
        if (quant_new != quant)
        {
           //printf("    recode hrd: %d, quant %d, quant_new %d, qpY %d, m_ctx.Quant %d, min %d max%d \n",frame_par->EncodedOrder, quant,quant_new, qpY, m_ctx.Quant,m_ctx.QuantMin , m_ctx.QuantMax);

           UpdateQPParams(quant_new ,picType, m_ctx, 0, m_ctx.QuantMin , m_ctx.QuantMax, layer);
        }
        SetRecodeParams(brcSts,quant,quant_new, m_ctx.QuantMin , m_ctx.QuantMax, m_ctx, status);    
    }
    else 
    {
        // no recoding are needed. Save context params

        mfxF64 k = 1./m_ctx.Quant;
        mfxF64 dqAbPeriod = m_par.dqAbPeriod;
        if (m_ctx.bToRecode)
            dqAbPeriod = (k < m_ctx.dQuantAb)? 16:25;
        m_ctx.dQuantAb += (k - m_ctx.dQuantAb)/dqAbPeriod;
        BRC_CLIP(m_ctx.dQuantAb, 1./m_par.quantMax , 1./m_par.quantMin);

        m_ctx.fAbLong  = fAbLong;
        m_ctx.fAbShort = fAbShort;
        if (picType != MFX_FRAMETYPE_B)
            m_ctx.eRate = eRate;

        bool oldScene = false;
        if ((m_ctx.SceneChange & 16) && (m_ctx.poc < m_ctx.SChPoc) && (bitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (mfxF64)m_ctx.LastNonBFrameSize) && (mfxF64)bitsEncoded < 1.5*fAbLong)
            oldScene = true;

        if (picType != MFX_FRAMETYPE_B)
            m_ctx.LastNonBFrameSize = bitsEncoded;

        if (!m_ctx.bPanic&& !oldScene) 
        {
            //Update QP
            m_ctx.totalDiviation += (bitsEncoded - m_par.inputBitsPerFrame);

            mfxF64 totDiv = m_ctx.totalDiviation;
            mfxF64 dequant_new = m_ctx.dQuantAb*pow(m_par.inputBitsPerFrame/m_ctx.fAbLong, 1.2);
            mfxF64 bAbPreriod = m_par.bAbPeriod;

            if (m_par.bHRDConformance && totDiv > 0 )
            {
                if (m_par.rateControlMethod == MFX_RATECONTROL_VBR)
                {
                    totDiv = IPP_MAX(totDiv, m_hrd.GetBufferDiviation(m_par.targetbps));
                }                    
                bAbPreriod = (mfxF64)m_hrd.GetMaxFrameSize() / m_par.inputBitsPerFrame*3;
                BRC_CLIP(bAbPreriod , m_par.bAbPeriod/10, m_par.bAbPeriod);                
            }
            mfxI32 quant_new = GetNewQPTotal(totDiv /bAbPreriod /(mfxF64)m_par.inputBitsPerFrame, dequant_new, m_ctx.QuantMin , m_ctx.QuantMax, m_ctx.Quant, m_par.bPyr);
            //printf("   Update QP %d: totalDiviation %f, bAbPreriod %f, QP %d (%d %d), qp_new %d\n",frame_par->EncodedOrder,totDiv , bAbPreriod, m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax,quant_new);
            if (quant_new != m_ctx.Quant && !m_ctx.bToRecode)
            {
                UpdateQPParams(quant_new ,picType, m_ctx, 0, m_ctx.QuantMin , m_ctx.QuantMax, layer);
            }
            //printf("QP: I %d, P %d, B %d layer %d\n", m_ctx.QuantI, m_ctx.QuantP, m_ctx.QuantB, layer);
        } 
        m_ctx.bToRecode = 0;
    }
    return sts;

}

mfxStatus ExtBRC::GetFrameCtrl (mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
{
    MFX_CHECK_NULL_PTR2(par, ctrl);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxI32 qp = 0;
    if (par->EncodedOrder == m_ctx.encOrder)
    {
        qp = m_ctx.Quant;
    }
    else 
    {
        mfxU16 type = GetFrameType(par->FrameType,par->PyramidLayer, m_par.gopRefDist);

        if (type == MFX_FRAMETYPE_I) 
            qp = m_ctx.QuantI;
        else if (type == MFX_FRAMETYPE_P) 
            qp =  m_ctx.QuantP + par->PyramidLayer;
        else 
             qp =  m_ctx.QuantB + (par->PyramidLayer > 0 ? par->PyramidLayer - 1 : 0);
    }
    BRC_CLIP(qp, m_par.quantMin, m_par.quantMax);

    ctrl->QpY = qp - m_par.quantOffset;
    return MFX_ERR_NONE;
}

mfxStatus ExtBRC::Reset(mfxVideoParam *par )
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK (m_par.bHRDConformance == 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK (m_par.rateControlMethod == MFX_RATECONTROL_VBR, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    sts = m_par.Init(par);
    MFX_CHECK_STS(sts);

    MFX_CHECK (m_par.bHRDConformance == 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK (m_par.rateControlMethod == MFX_RATECONTROL_VBR, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    m_ctx.Quant = (mfxI32)(1./m_ctx.dQuantAb * pow(m_ctx.fAbLong/m_par.inputBitsPerFrame, 0.32) + 0.5); 
    BRC_CLIP(m_ctx.Quant, m_par.quantMin, m_par.quantMax); 
    
    UpdateQPParams(m_ctx.Quant, MFX_FRAMETYPE_I , m_ctx, 0, m_par.quantMin, m_par.quantMax, 0);

    m_ctx.dQuantAb = 1./m_ctx.Quant;
    m_ctx.fAbLong  = m_par.inputBitsPerFrame;
    m_ctx.fAbShort = m_par.inputBitsPerFrame;

    return sts;
}
#endif
}


