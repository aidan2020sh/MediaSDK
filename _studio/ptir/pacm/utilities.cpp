/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: utilities.cpp

\* ****************************************************************************** */
#include "utilities.h"

extern DeinterlaceFilter *pdeinterlaceFilter;

static void CopyFrame(Frame *pSrc, Frame* pDst)
{
    unsigned int i, val = 0;
    const unsigned char* pSrcLine;
    unsigned char* pDstLine;

    // copying Y-component
    for (i = 0; i < pSrc->plaY.uiHeight; i++)
    {
        pSrcLine = pSrc->plaY.ucCorner + i * pSrc->plaY.uiStride;
        pDstLine = pDst->plaY.ucCorner + i * pDst->plaY.uiStride;
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaY.uiWidth);
    }

    // copying U-component
    for (i = 0; i < pSrc->plaU.uiHeight; i++)
    {
        pSrcLine = pSrc->plaU.ucCorner + i * pSrc->plaU.uiStride;
        pDstLine = pDst->plaU.ucCorner + i * pDst->plaU.uiStride;
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaU.uiWidth);
    }
    // copying V-component
    for (i = 0; i < pSrc->plaV.uiHeight; i++)
    {
        pSrcLine = pSrc->plaV.ucCorner + i * pSrc->plaV.uiStride;
        pDstLine = pDst->plaV.ucCorner + i * pDst->plaV.uiStride;
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaV.uiWidth);
    }
}

#if TEST_UNDO2FRAMES_CM
void Undo2Frames_CMTest(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF)
{    
    Frame frmBackup;
    Frame *frmBuffer1_c = &frmBackup;
    Frame_Create(frmBuffer1_c, frmBuffer1->plaY.uiWidth, frmBuffer1->plaY.uiHeight, frmBuffer1->plaY.uiWidth/ 2, frmBuffer1->plaY.uiHeight/ 2, 0);
    CopyFrame(frmBuffer1, frmBuffer1_c);
    int comp = 0;

    pdeinterlaceFilter->Undo2FramesYUVCM(frmBuffer1, frmBuffer2, BFF);

    unsigned int 
        i,
        start;

    start = BFF;

    for(i = start; i < frmBuffer1_c->plaY.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1_c->plaY.ucData + (i * frmBuffer1_c->plaY.uiStride), frmBuffer2->plaY.ucData + (i * frmBuffer2->plaY.uiStride),frmBuffer2->plaY.uiStride);
    for(i = start; i < frmBuffer1_c->plaU.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1_c->plaU.ucData + (i * frmBuffer1_c->plaU.uiStride), frmBuffer2->plaU.ucData + (i * frmBuffer2->plaU.uiStride),frmBuffer2->plaU.uiStride);
    for(i = start; i < frmBuffer1_c->plaV.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1_c->plaV.ucData + (i * frmBuffer1_c->plaV.uiStride), frmBuffer2->plaV.ucData + (i * frmBuffer2->plaV.uiStride),frmBuffer2->plaV.uiStride);

    unsigned char * line0 = frmBuffer1->plaY.ucData;
    unsigned char * line1 = frmBuffer1->plaY.ucData + frmBuffer1->plaY.uiStride;

    comp = memcmp(frmBuffer1_c->ucMem, frmBuffer1->ucMem, frmBuffer1->uiSize);
    printf("Undo2Frames_CM test result: %s", comp?"Failed!\n":"Successful!\n");
}
#endif

static void Undo2Frames_CM(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF)
{    
 //memset(frmBuffer1->ucMem, 0, frmBuffer1->uiSize);
 //return;
#ifdef GPUPATH
    pdeinterlaceFilter->Undo2FramesYUVCM(frmBuffer1, frmBuffer2, BFF);
#endif

#ifdef CPUPATH
    Undo2Frames(frmBuffer1, frmBuffer2, BFF);
#endif
}

static void Detect_Solve_32Patterns_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    bool condition[10];


    //Case: First and last frames have pattern, need to move buffer two frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));


    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4] && condition[5])
    {
        pFrm[0]->frmProperties.drop = false;//true;
        pFrm[0]->frmProperties.drop24fps = true;
        pFrm[0]->frmProperties.candidate = true;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[4]->plaY.ucStats.ucSAD[2] > pFrm[4]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 2;
        return;
    }

    //Case: First and second frames have pattern, need to move buffer three frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4] && condition[5])
    {
        pFrm[0]->frmProperties.drop = false;
        pFrm[1]->frmProperties.drop = false;//true;
        pFrm[1]->frmProperties.drop24fps = true;
        pFrm[1]->frmProperties.candidate = true;
        pFrm[2]->frmProperties.drop = false;
        ptrn->ucLatch.ucFullLatch = true;

        if(pFrm[1]->plaY.ucStats.ucSAD[2] > pFrm[1]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 3;
        Undo2Frames_CM(pFrm[0],pFrm[1],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: second and third frames have pattern, need to move buffer four frames up
    //Texture Analysis
    condition[0] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[1] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[0]->frmProperties.drop = false;
        pFrm[1]->frmProperties.drop = false;
        pFrm[2]->frmProperties.drop = false;/*true*/
        pFrm[2]->frmProperties.drop24fps = true;
        pFrm[2]->frmProperties.candidate = true;
        pFrm[3]->frmProperties.drop = false;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 4;
        Undo2Frames_CM(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: third and fourth frames have pattern, pattern is synchronized
    //Texture Analysis
    condition[0] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    condition[1] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[3]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[3]->frmProperties.drop = true;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 5;
        Undo2Frames_CM(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: fourth and fifth frames have pattern, one frame needs to be moved out
    //Texture Analysis
    condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[3]->plaY.ucStats.ucSAD[4] /** T1*/ > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    if((condition[0] && condition[1] && condition[2]) || (condition[0] && condition[1] && condition[3]))
    {
        pFrm[0]->frmProperties.drop = false;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        pFrm[0]->frmProperties.candidate = true;

        *dispatch = 1;
        return;
    }
}

static void Detect_32Pattern_rigorous_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    //Detect_32Pattern_rigorous(pFrm, ptrn, dispatch);
    //return;
    bool condition[10];
    int previousPattern = ptrn->ucPatternType;
    double previousAvgSAD = ptrn->ucAvgSAD;

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    Init_drop_frames(pFrm);

    condition[8] = ptrn->ucAvgSAD < previousAvgSAD * T2;

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || previousPattern == 5 || previousPattern == 6 || previousPattern == 7))
        Detect_Solve_32BlendedPatternsCM(pFrm, ptrn, dispatch);

    if((!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum >= 3)) || ((previousPattern == 1 || previousPattern == 2) && ptrn->ucLatch.ucSHalfLatch))
    {
        ptrn->ucLatch.ucSHalfLatch = false;
        ptrn->ucPatternType = 0;
        //Texture Analysis  - To override previous pattern if needed
        condition[0] = pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[1]) ||
                       pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[2]) ||
                       pFrm[2]->frmProperties.interlaced;
        condition[1] = pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[1]) ||
                       pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[2]) ||
                       pFrm[3]->frmProperties.interlaced;


        if(previousPattern == 1 || (condition[0] && condition[1]))
        {
            //Assuming pattern is synchronized
            //Texture Analysis 2
            condition[2] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7])) || pFrm[2]->frmProperties.interlaced;
            condition[3] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) || pFrm[3]->frmProperties.interlaced;

            condition[9] = (pFrm[0]->plaY.ucStats.ucRs[5] == 0) && (pFrm[1]->plaY.ucStats.ucRs[5] == 0) && (pFrm[4]->plaY.ucStats.ucRs[5] == 0);

            //SAD analysis
            condition[4] = pFrm[2]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[5] = pFrm[3]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));

            condition[6] = pFrm[2]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[7] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));


            if((condition[0] && condition[1] && ptrn->uiInterlacedFramesNum > 0) || (condition[2] && condition[3]) || (condition[4] && condition[5]) || (condition[6] && condition[7]))
            {
                pFrm[3]->frmProperties.drop = true;
                ptrn->ucLatch.ucFullLatch = true;
                if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[0] > pFrm[2]->plaY.ucStats.ucSAD[1] && pFrm[4]->plaY.ucStats.ucSAD[1] > pFrm[4]->plaY.ucStats.ucSAD[0])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                ptrn->ucPatternType = 1;
                *dispatch = 5;
                Undo2Frames_CM(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
                return;
            }
            if((condition[0] || condition[1] || condition[2] || condition[3] || (condition[4] || condition[5]) || condition[6] || condition[7] || condition[9]) && (ptrn->ucAvgSAD < 2.3))
            {
                pFrm[3]->frmProperties.drop = true;
                ptrn->ucLatch.ucFullLatch = true;
                if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                if(condition[8] || condition[9])
                    ptrn->ucPatternType = 1;
                else
                    ptrn->ucPatternType = 0;
                *dispatch = 5;
                Undo2Frames_CM(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
                return;
            }
        }
        else if(previousPattern == 2)
        {
            //Assuming pattern is synchronized
            //Texture Analysis
            condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                           pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                           pFrm[3]->frmProperties.interlaced;
            //SAD analysis
            condition[1] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
            if(condition[0] || condition[1])
            {
                pFrm[2]->frmProperties.candidate = true;
                pFrm[3]->frmProperties.drop = true;
                pFrm[4]->frmProperties.candidate = true;
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 2;
                *dispatch = 5;
                return;
            }
            else if(pFrm[3]->plaY.ucStats.ucRs[7] > 0)
            {
                pFrm[2]->frmProperties.candidate = true;
                pFrm[3]->frmProperties.drop = true;
                pFrm[4]->frmProperties.candidate = true;
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 0;
                *dispatch = 5;
                return;
            }
        }
    }

    if(!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum > 2) && (ptrn->uiInterlacedFramesNum > 0))
    {
        Detect_Solve_32Patterns_CM(pFrm, ptrn, dispatch);
        if(!ptrn->ucLatch.ucFullLatch)
            Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);
    }
    if(!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum == 4))
    {
        ptrn->ucLatch.ucFullLatch = true;
        ptrn->ucPatternType = 3;
        *dispatch = 5;
    }
}

static void Detect_Interlacing_Artifacts_fast_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i;

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucPatternType = 0;

    Init_drop_frames(pFrm);

    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || ptrn->ucPatternType == 5 || ptrn->ucPatternType == 6))
        Detect_Solve_32BlendedPatternsCM(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_32Patterns_CM(pFrm, ptrn, dispatch);
    
    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);

    for(i = 0; i < BUFMINSIZE; i++)
        pFrm[i]->frmProperties.processed = true;

}
void Analyze_Buffer_Stats_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced)
{
    unsigned int uiDropCount = 0,
        i = 0,
        uiPrevInterlacedFramesNum = (*pdispatch == 1 ? 0:ptrn->uiInterlacedFramesNum);

    ptrn->uiInterlacedFramesNum = Artifacts_Detection(frmBuffer) + frmBuffer[0]->frmProperties.interlaced;

    if ((*uiisInterlaced != 1) && !(ptrn->uiInterlacedFramesNum > 4 && ptrn->ucPatternType < 1))
    {
        if(!frmBuffer[0]->frmProperties.processed)
            Detect_Interlacing_Artifacts_fast_CM(frmBuffer, ptrn, pdispatch);
        else
            Detect_32Pattern_rigorous_CM(frmBuffer, ptrn, pdispatch);

        if(!ptrn->ucLatch.ucFullLatch)
        {
            if(ptrn->uiInterlacedFramesNum < 2 && !ptrn->ucLatch.ucThalfLatch)
            {
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 0;
                *pdispatch = 5;
            }
            else
            {
                *pdispatch = 1;
                ptrn->uiOverrideHalfFrameRate = false;
            }
        }
        if (*uiisInterlaced == 2)
        {
            if (*pdispatch >= BUFMINSIZE - 1)
            {
                for (i = 0; i < *pdispatch; i++)
                    uiDropCount += frmBuffer[i]->frmProperties.drop;

                if (!uiDropCount)
                    frmBuffer[BUFMINSIZE > 1]->frmProperties.drop = 1;
            }
        }
    }
    else
    {
        for (i = 0; i < BUFMINSIZE - 1; i++)
        {
            frmBuffer[i]->frmProperties.drop = 0;
            frmBuffer[i]->frmProperties.interlaced = 1;
        }

        *pdispatch = (BUFMINSIZE - 1);

        if(uiPrevInterlacedFramesNum == BUFMINSIZE - 1)
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
            {
                ptrn->uiOverrideHalfFrameRate = true;
                ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            }
        }
        else
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
                ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            else
            {
                if(ptrn->uiCountFullyInterlacedBuffers > 0)
                    ptrn->uiCountFullyInterlacedBuffers--;
                else
                    ptrn->uiOverrideHalfFrameRate = false;
            }
        }
    }

    ptrn->ucPatternFound = ptrn->ucLatch.ucFullLatch;
}

void STDCALL Analyze_Buffer_Stats_Automode_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch)
{
    unsigned int uiAutomode = AUTOMODE;

    Analyze_Buffer_Stats_CM(frmBuffer, ptrn, pdispatch, &uiAutomode);

}

void Detect_Solve_32BlendedPatternsCM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i, count, start, previousPattern;
 bool condition[10];

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucLatch.ucThalfLatch = false;
    previousPattern = ptrn->ucPatternType;
    ptrn->ucPatternType = 0;
 for (i = 0; i < 10; i++)
  condition[i] = 0;

    if (previousPattern != 5 && previousPattern != 7)
    {
        condition[0] = (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
            (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
            (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
            (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[4]->plaY.ucStats.ucRs[4]) &&
            (pFrm[0]->plaY.ucStats.ucRs[4] < 100);//(pFrm[0]->plaY.ucStats.ucRs[7] < 0.22);//

        condition[1] = (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[0]->plaY.ucStats.ucRs[4]) &&
            (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
            (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
            (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
            (pFrm[4]->plaY.ucStats.ucRs[4] < 100);//(pFrm[4]->plaY.ucStats.ucRs[7] < 0.22);//

        if (condition[0] && !condition[1])
            start = 1;
        else if (!condition[0] && condition[1])
            start = 0;
        else
        {
            if (!pFrm[5]->frmProperties.interlaced)
                ptrn->ucLatch.ucSHalfLatch = true;
            return;
        }

        for (i = 0; i < 4; i++)
            condition[i + start] = (pFrm[i + start]->plaY.ucStats.ucRs[6] > 100) && (pFrm[i + start]->plaY.ucStats.ucRs[7] > 0.1);

        count = 1;
        for (i = 1; i < 4; i++)
            count += condition[i];
        if (count > 3)
        {
            if (condition[0])
            {
                start = 1;
                pFrm[4]->frmProperties.drop = true;
                *dispatch = 5;
            }
            else
            {
                start = 0;
                pFrm[3]->frmProperties.drop = true;
                *dispatch = 4;
            }
            UndoPatternTypes5and7CM(pFrm, start);
            ptrn->ucLatch.ucFullLatch = true;
            if (previousPattern != 6)
                ptrn->ucPatternType = 5;
            else
                ptrn->ucPatternType = 7;
        }
    }
    else
    {
        count = 0;
        for (i = 0; i < 3; i++)
            count += pFrm[i]->frmProperties.interlaced;
        if (count < 2)
        {
            if (previousPattern != 7)
            {
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->blendedCount += BLENDEDOFF;
                if (ptrn->blendedCount > 1)
                {
                    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(pFrm, 1, 0);
                    ptrn->blendedCount -= 1;
                }
                else
                {
                    pFrm[1]->frmProperties.drop = true;
                }
                ptrn->ucPatternType = 6;
                *dispatch = 2;
            }
            else
            {
                count = 0;
                for (i = 3; i < 5; i++)
                    count += pFrm[i]->frmProperties.interlaced;
                if (count > 1 && pFrm[5]->frmProperties.interlaced)
                {
                    ptrn->ucLatch.ucFullLatch = true;
                    ptrn->ucPatternType = 8;
                    *dispatch = 2;
                }
                else
                {
                    ptrn->ucLatch.ucFullLatch = true;
                    ptrn->ucPatternType = 1;
                    *dispatch = 1;
                }
            }
        }
    }
}

void UndoPatternTypes5and7CM(Frame *frmBuffer[BUFMINSIZE + BUFEXTRASIZE], unsigned int firstPos)
{
    unsigned int
        start = firstPos;
    //Frame 1
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));

    //Frame 2
    start++;
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));
 

    //Frame 3
    start++;
    frmBuffer[start]->frmProperties.drop = true;


    //Frame 4
    start++;
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));

}

void CheckGenFrameCM(Frame **pfrmIn, unsigned int frameNum, /*unsigned int patternType, */unsigned int uiOPMode)
{
    bool stop = false;
    if ((pfrmIn[frameNum]->frmProperties.candidate/* || patternType == 0*/) && (uiOPMode == 0 || uiOPMode == 3))
    {
        pdeinterlaceFilter->MeasureRs(pfrmIn[frameNum]);
        pfrmIn[frameNum]->frmProperties.interlaced = false;
        Artifacts_Detection_frame(pfrmIn,frameNum, false);
        if(pfrmIn[frameNum]->frmProperties.interlaced)
        {
            //FillBaseLinesIYUV(pfrmIn[frameNum], pfrmIn[BUFMINSIZE], false, false);
            pdeinterlaceFilter->DeinterlaceMedianFilterCM(pfrmIn, frameNum);
        }
    }
}

void Prepare_frame_for_queueCM(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight, frameSupplier* frmSupply, bool bCreate)
{
    assert(0 != pfrmOut);
    assert(pfrmIn != NULL && pfrmIn->inSurf != NULL && pfrmIn->outSurf != NULL);
    if(!pfrmOut || !pfrmIn)
        return;
    *pfrmOut = (Frame *)malloc(sizeof(Frame));
    if(!*pfrmOut)
        return;
    memset(*pfrmOut, 0, sizeof(Frame));
    assert(*pfrmOut != NULL);
    Frame_CreateCM(*pfrmOut, uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 64, bCreate);
    if(frmSupply && !bCreate)
        static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
#ifdef CPUPATH
    ReSample(*pfrmOut, pfrmIn);
#endif
 std::swap((*pfrmOut)->outSurf, pfrmIn->outSurf);
 std::swap((*pfrmOut)->inSurf, pfrmIn->inSurf);
 std::swap((*pfrmOut)->outState, pfrmIn->outState);
}

void Frame_CreateCM(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder, bool bcreate)
{
#ifdef CPUPATH
    Frame_Create(pfrmIn, uiYWidth, uiYHeight, uiUVWidth, uiUVHeight, uiBorder);
#else
 unsigned int uiOffset;
 pfrmIn->uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder) + (2 * (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder));
 //pfrmIn->ucMem = (unsigned char *)malloc(pfrmIn->uiSize);
    assert(pfrmIn->inSurf == 0);
    assert(pfrmIn->outSurf == 0);
 pfrmIn->ucMem = NULL;
 pfrmIn->inSurf = NULL;
 pfrmIn->outSurf = NULL;

    //if (pfrmIn->ucMem)
    {
        pfrmIn->frmProperties.drop = 0;
        pfrmIn->frmProperties.candidate = 0;
        pfrmIn->plaY.uiWidth = uiYWidth;
        pfrmIn->plaY.uiHeight = uiYHeight;
        pfrmIn->plaY.uiBorder = uiBorder;
        pfrmIn->plaY.uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder);
        pfrmIn->plaY.uiStride = uiYWidth + 2 * uiBorder;
        pfrmIn->plaY.ucData = pfrmIn->ucMem;
        pfrmIn->plaY.ucCorner = pfrmIn->plaY.ucData + (pfrmIn->plaY.uiStride + 1) * pfrmIn->plaY.uiBorder;

        uiOffset = pfrmIn->plaY.uiStride * (uiYHeight + 2 * uiBorder);

        pfrmIn->plaU.uiWidth = uiUVWidth;
        pfrmIn->plaU.uiHeight = uiUVHeight;
        pfrmIn->plaU.uiBorder = uiBorder;
        pfrmIn->plaU.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaU.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaU.ucData = pfrmIn->ucMem + uiOffset;
        pfrmIn->plaU.ucCorner = pfrmIn->plaU.ucData + (pfrmIn->plaU.uiStride + 1) * pfrmIn->plaU.uiBorder;

        uiOffset += pfrmIn->plaU.uiStride * (uiUVHeight + 2 * uiBorder);

        pfrmIn->plaV.uiWidth = uiUVWidth;
        pfrmIn->plaV.uiHeight = uiUVHeight;
        pfrmIn->plaV.uiBorder = uiBorder;
        pfrmIn->plaV.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaV.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaV.ucData = pfrmIn->ucMem + uiOffset;
        pfrmIn->plaV.ucCorner = pfrmIn->plaV.ucData + (pfrmIn->plaV.uiStride + 1) * pfrmIn->plaV.uiBorder;
    }
    //else
    //    pfrmIn->uiSize = 0;
#endif
    pdeinterlaceFilter->FrameCreateSurface(pfrmIn, bcreate);
}

void Frame_ReleaseCM(Frame *pfrmIn)
{
    pdeinterlaceFilter->FrameReleaseSurface(pfrmIn);
    Frame_Release(pfrmIn);
}

// FIXME error handling
//void LoadNextFrameInI420(HANDLE hIn, unsigned char *pucIn,  const char * pcFormat, Frame * pFrame, unsigned int uiSize, double dFrameRate)
//{
//    DWORD uiBytesRead = 0;
//    pFrame->frmProperties.fr = dFrameRate;
//    if (strcmp(pcFormat, "I420") == 0) {
//#ifdef CPUPATH
//        void* ucMem = pFrame->ucMem;
//        bool ferror = ReadFile(hIn, pFrame->ucMem, uiSize, &uiBytesRead, NULL);
//        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
//#else
//        void* ucMem = malloc(uiSize);
//        bool ferror = ReadFile(hIn, ucMem, uiSize, &uiBytesRead, NULL);
//        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
//        free(ucMem);
//#endif
//
//    } else if (strcmp(pcFormat, "UYVY") == 0) {
//        ;
//    } 
//}

unsigned int Convert_to_I420CM(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double dFrameRate)
{
    pfrmOut->frmProperties.fr = dFrameRate;
    if (strcmp(pcFormat, "I420") == 0)
    {
        ptir_memcpy(pfrmOut->ucMem, pucIn, pfrmOut->uiSize);
        return 1;
    }
    else if (strcmp(pcFormat, "UYVY") == 0)
    {
       // Convert_UYVY_to_I420(pucIn, pfrmOut);
        return 0;
    }
    else
        return 0;
}

void Frame_Prep_and_AnalysisCM(Frame **frmBuffer, char *pcFormat, double dFrameRate, unsigned int uiframeBufferIndexCur, unsigned int uiframeBufferIndexNext, unsigned int uiTemporalIndex)
{
#if PRINTDEBUG
    double    *dSAD,
            *dRs;
    unsigned int dPicSize;

    dPicSize = frmBuffer[uiframeBufferIndexCur]->plaY.uiWidth * frmBuffer[uiframeBufferIndexCur]->plaY.uiHeight;
#endif

    //pdeinterlaceFilter->WriteRAWI420ToGPUNV12(frmBuffer[uiframeBufferIndexCur], frmBuffer[BUFMINSIZE]->ucMem);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.tindex = uiTemporalIndex + 1;
    pdeinterlaceFilter->CalculateSADRs(frmBuffer[uiframeBufferIndexCur], frmBuffer[uiframeBufferIndexNext]);
    Artifacts_Detection_frame(frmBuffer, uiframeBufferIndexCur, true);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.processed = true;

    if(uiframeBufferIndexCur == uiframeBufferIndexNext)
    {
        frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD[1] = 99.999;
    }

#if PRINTDEBUG
    dSAD = frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD;
    dRs = frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucRs;
    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", uiTemporalIndex + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
#endif
}

void PTIRCM_Frame_Prep_and_Analysis(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur == 0)
    {
        SysBuffer->control.uiNext = 0;
        SysBuffer->control.uiFrame = 0;
    }
    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    SysBuffer->control.uiCur++;
}

void Update_Frame_Buffer_CM(Frame** frmBuffer, unsigned int frameIndex, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn)
{
    if (frmBuffer[frameIndex]->frmProperties.interlaced)
    {
        if(uiInterlaceParity) //BFF
        {
            if(bFullFrameRate) //Generate second frame
            {
                pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, frameIndex, BUFMINSIZE);
            }
            pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, -1, 0); //bootom field
            pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, 0, -1); //top field
        }
        else //TFF, use old scheme
        {
            pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, frameIndex, BUFMINSIZE);
            //deinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, -1, 1);
            //deinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, 0, -1);
        }
     }
     else
         CheckGenFrameCM(frmBuffer, frameIndex, uiisInterlaced);

    Prepare_frame_for_queueCM(&frmIn,frmBuffer[frameIndex], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight);
    ptir_memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[frameIndex]->plaY.ucStats.ucRs,sizeof(double) * 10);
           
    //Timestamp
    if (frmBuffer[frameIndex]->frmProperties.interlaced && bFullFrameRate == 1 && uiisInterlaced == 1)
    {
        frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[frameIndex]->frmProperties.tindex;
        frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp + dTimePerFrame;
        frmBuffer[frameIndex + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dTimePerFrame;
    }
    else
        frmBuffer[frameIndex + 1]->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp + dTimePerFrame;

    frmIn->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp;

    FrameQueue_Add(fqIn, frmIn);

    if (bFullFrameRate && uiisInterlaced == 1)
    {
        Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight); // Go to double frame rate
        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

        //Timestamp
        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

        FrameQueue_Add(fqIn, frmIn);
    }
    
    frmBuffer[frameIndex]->frmProperties.drop = false;
    frmBuffer[frameIndex]->frmProperties.candidate = false;
}

int PTIRCM_AutoMode_FF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType);
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            uiDeinterlace = SysBuffer->control.mainPattern.uiOverrideHalfFrameRate;
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &SysBuffer->control.uiDispatch, SysBuffer->control.mainPattern.ucPatternType);
            SysBuffer->control.dBaseTimeSw = SysBuffer->control.dTimePerFrame / (1 + uiDeinterlace);
            for(i = 0; i < SysBuffer->control.uiDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop)
                {
                    if (!(SysBuffer->control.mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1)))
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTimeSw, AUTOMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTimeSw, uiDeinterlace, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                    }
                }
                else
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_AutoMode_HF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType);
            if(!SysBuffer->control.uiEndOfFrames)
                uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
            else
                uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &SysBuffer->control.uiDispatch, SysBuffer->control.mainPattern.ucPatternType);
            SysBuffer->control.dBaseTimeSw = SysBuffer->control.dTimePerFrame;
            for(i = 0; i < SysBuffer->control.uiDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop)
                {
                    if (!(SysBuffer->control.mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1)))
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTimeSw, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTimeSw, uiDeinterlace, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                    }
                }
                else
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_DeinterlaceMode_FF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        SysBuffer->control.dTimePerFrame = SysBuffer->control.dBaseTime / 2;
        //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dTimePerFrame, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIRCM_DeinterlaceMode_HF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIRCM_BaseFrameMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            for (i = 0; i < uiNumFramesToDispatch; i++)
                Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
            Rotate_Buffer_borders(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            for(i = 0; i < SysBuffer->control.uiDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_Auto24fpsMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiTelecineMode = TELECINE24FPSMODE,
                 uiNumFramesToDispatch, uiLetGo = 0;
    const double filmfps = 1 / 23.976 * 1000;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch, &uiTelecineMode);
        /*if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex > SysBuffer->control.uiFrameCount)
            SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.drop = true;*/
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        for(i = 0; i < uiNumFramesToDispatch; i++)
        {
            if(SysBuffer->frmBuffer[i]->frmProperties.drop || SysBuffer->frmBuffer[i]->frmProperties.drop24fps)
                uiLetGo++;
        }

        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop && (!(SysBuffer->control.uiBufferCount + 1 + i == 5) || uiLetGo))
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
                    if((SysBuffer->control.uiBufferCount + 1 + i == 5) && !uiLetGo)
                        SysBuffer->control.uiBufferCount = 0;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
            if((uiNumFramesToDispatch < (BUFMINSIZE - 1)) && !uiLetGo)
                SysBuffer->control.uiBufferCount += uiNumFramesToDispatch;
        }
        else
        {
            for(i = 0; i < SysBuffer->control.uiDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    SysBuffer->control.uiBufferCount++;

                    if ((SysBuffer->control.uiBufferCount < (BUFMINSIZE - 1)))
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        SysBuffer->control.uiBufferCount = 0;
                        SysBuffer->frmBuffer[1]->frmProperties.timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
                    }
                }
                else
                {
                    SysBuffer->frmBuffer[0]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
                    SysBuffer->control.uiBufferCount = 0;
                }
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_FixedTelecinePatternMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiTelecineMode = TELECINE24FPSMODE,
                 uiNumFramesToDispatch, uiLetGo = 0;
    const double filmfps = 1 / 23.976 * 1000;

    printf("Fixed Telecine pattern removal mode or Mode 5 is not available right now\n");
    exit(MODE_NOT_AVAILABLE);

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch, &uiTelecineMode);
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex > SysBuffer->control.uiFrameCount)
            SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.drop = true;
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        for(i = 0; i < uiNumFramesToDispatch; i++)
        {
            if(SysBuffer->frmBuffer[i]->frmProperties.drop || SysBuffer->frmBuffer[i]->frmProperties.drop24fps)
                uiLetGo++;
        }

        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop && (!(SysBuffer->control.uiBufferCount + 1 + i == 5) || uiLetGo))
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
                    if((SysBuffer->control.uiBufferCount + 1 + i == 5) && !uiLetGo)
                        SysBuffer->control.uiBufferCount = 0;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
            if((uiNumFramesToDispatch < (BUFMINSIZE - 1)) && !uiLetGo)
                SysBuffer->control.uiBufferCount += uiNumFramesToDispatch;
        }
        else
        {
            for(i = 0; i < SysBuffer->control.uiDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    SysBuffer->control.uiBufferCount++;

                    if ((SysBuffer->control.uiBufferCount < (BUFMINSIZE - 1)))
                    {
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        SysBuffer->control.uiBufferCount = 0;
                        SysBuffer->frmBuffer[1]->frmProperties.timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
                    }
                }
                else
                {
                    SysBuffer->frmBuffer[0]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
                    SysBuffer->control.uiBufferCount = 0;
                }
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_MultipleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode)
{
    if(uiOpMode == 0)
        return(PTIRCM_AutoMode_FF(SysBuffer));
    else if(uiOpMode == 1)
        return(PTIRCM_AutoMode_HF(SysBuffer));
    else if(uiOpMode == 2)
        return(PTIRCM_DeinterlaceMode_FF(SysBuffer));
    else if(uiOpMode == 3)
        return(PTIRCM_DeinterlaceMode_HF(SysBuffer));
    else if(uiOpMode == 4)
        return(PTIRCM_Auto24fpsMode(SysBuffer));
    else if(uiOpMode == 5)
        return(PTIRCM_FixedTelecinePatternMode(SysBuffer));
    else
        return(PTIRCM_BaseFrameMode(SysBuffer));
}

void PTIRCM_Init(PTIRSystemBuffer *SysBuffer, unsigned int _uiInWidth, unsigned int _uiInHeight, double _dFrameRate, unsigned int _uiInterlaceParity)
{
    unsigned int i;

    SysBuffer->control.uiInWidth = _uiInWidth;
    SysBuffer->control.uiInHeight = _uiInHeight;
    SysBuffer->control.dFrameRate = _dFrameRate;
    SysBuffer->control.uiInterlaceParity = _uiInterlaceParity;
    SysBuffer->control.uiWidth = _uiInWidth;
    SysBuffer->control.uiHeight = _uiInHeight;

    SysBuffer->control.uiCur = 0;
    SysBuffer->control.uiNext = 0;
    SysBuffer->control.uiFrame = 0;
    SysBuffer->control.uiBufferCount = 0;
    SysBuffer->control.uiFrameCount = 0;
    SysBuffer->control.uiDispatch = 0;
    SysBuffer->control.mainPattern.blendedCount = 0.0;
    SysBuffer->control.mainPattern.uiIFlush = 0;
    SysBuffer->control.mainPattern.uiPFlush = 0;
    SysBuffer->control.mainPattern.uiOverrideHalfFrameRate = false;
    SysBuffer->control.mainPattern.uiCountFullyInterlacedBuffers = 0;
    SysBuffer->control.mainPattern.uiInterlacedFramesNum = 0;
    SysBuffer->control.dBaseTime = (1 / SysBuffer->control.dFrameRate) * 1000;
    SysBuffer->control.dDeIntTime = SysBuffer->control.dBaseTime / 2;
    SysBuffer->control.dBaseTimeSw = SysBuffer->control.dBaseTime;
    SysBuffer->control.dTimePerFrame = 0.0;
    SysBuffer->control.dTimeStamp = 0.0;
    SysBuffer->control.uiSize = SysBuffer->control.uiInWidth * SysBuffer->control.uiInHeight * 3 / 2;
    SysBuffer->control.uiEndOfFrames = 0;

    for(i = 0; i < LASTFRAME; i++)
    {
        SysBuffer->frmIO[i] = (Frame*)malloc(sizeof(Frame));
        Frame_CreateCM(SysBuffer->frmIO[i], SysBuffer->control.uiInWidth, SysBuffer->control.uiInHeight, SysBuffer->control.uiInWidth / 2, SysBuffer->control.uiInHeight / 2, 0);
        SysBuffer->frmBuffer[i] = SysBuffer->frmIO[i];
    }
    SysBuffer->frmIO[LASTFRAME] = (Frame*)malloc(sizeof(Frame));
    Frame_CreateCM(SysBuffer->frmIO[LASTFRAME], SysBuffer->control.uiWidth, SysBuffer->control.uiHeight, SysBuffer->control.uiWidth / 2, SysBuffer->control.uiHeight / 2, 0);
    FrameQueue_Initialize(&SysBuffer->fqIn);
    Pattern_init(&SysBuffer->control.mainPattern);
    SysBuffer->frmIn = NULL;
}

int OutputFrameToDiskCM(HANDLE hOut, Frame* frmIn, Frame* frmOut, unsigned int * uiLastFrameNumber, DWORD *uiBytesRead)
{
    int ferror;
    *uiLastFrameNumber = frmIn->frmProperties.tindex;
    ferror = false;
    void* ucMem = malloc(frmIn->uiSize);
    //pdeinterlaceFilter->ReadRAWI420FromGPUNV12(frmIn, ucMem);
    ferror = WriteFile(hOut, ucMem, frmOut->uiSize, uiBytesRead, NULL);
    free(ucMem);

    return ferror;

}

void PTIRCM_PutFrame(unsigned char *pucIO, PTIRSystemBuffer *SysBuffer)
{
    unsigned int status = 0;
    status = Convert_to_I420CM(pucIO, SysBuffer->frmBuffer[BUFMINSIZE], "I420", SysBuffer->control.dFrameRate);
    if(status)
        SysBuffer->control.uiFrameCount += status;
    else
        exit(-1000);
}

void PTIRCM_Clean(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i;

    for (i = 0; i <= LASTFRAME; ++i)
        Frame_ReleaseCM(SysBuffer->frmIO[i]);
}