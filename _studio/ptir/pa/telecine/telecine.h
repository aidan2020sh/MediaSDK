// Copyright (c) 2014-2018 Intel Corporation
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

#ifndef PTIR_TELECINE_H
#define PTIR_TELECINE_H

#include "../api.h"
#include "../log_debug.h"
#include "model.h"
#ifdef __cplusplus
extern "C" {
#endif

// Inverse telecine and deinterlacer
#define T1                      1.01
#define T2                      3
#define T3                      1.03
#define T4                      1.08
#define NABS(a)                 ((a<0)?-a:a)
#define TVBLACK                 16
#define BLENDEDOFF              0.375

#define DEATHTOMING             0

// SAD
#define PASAD(a,b)              ((a>b)?a-b:b-a)
#define InterStraightTop        0
#define InterStraightBottom     1
#define InterCrossTop           2
#define InterCrossBottom        3
#define IntraSAD                4


void         Pattern_init(Pattern *ptrn);

void         Rotate_Buffer(Frame *frmBuffer[BUFMINSIZE]);
void         Rotate_Buffer_borders(Frame *frmBuffer[BUFMINSIZE], unsigned int LastInLatch);
void         Rotate_Buffer_deinterlaced(Frame *frmBuffer[BUFMINSIZE]);
double       SSAD8x2(unsigned char *line1, unsigned char *line2, unsigned int offset);
void         Rs_measurement(Frame *pfrmIn);
void         setLinePointers(unsigned char *pLine[10], Frame pfrmIn, int offset, BOOL lastStripe);
void         Line_rearrangement(unsigned char *pFrmDstTop,unsigned char *pFrmDstBottom, unsigned char **pucIn, Plane planeOut,unsigned int *off);
void         Extract_Fields_I420(unsigned char *pucLine, Frame *pfrmOut, BOOL TopFieldFirst);
void         sadCalc_I420_frame(Frame *pfrmCur, Frame *pfrmPrv);
unsigned int Classifier_old(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif, double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture, double dCount, double dRsG, double dRsDif, double dRsB);
unsigned int Classifier(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif, double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture, double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB);
void         Artifacts_Detection_frame(Frame **pFrm, unsigned int frameNum);
unsigned int Artifacts_Detection(Frame **pFrm);
void         Detect_Solve_32RepeatedFramePattern(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void         Detect_Solve_41FramePattern(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void         Detect_Solve_32BlendedPatterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void         Detect_Solve_32Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void         Detect_Solve_3223Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void         Init_drop_frames(Frame **pFrm);
void         Detect_Interlacing_Artifacts_fast(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch, unsigned int *uiisInterlaced);
double       CalcSAD_Avg_NoSC(Frame **pFrm);
void         Detect_32Pattern_rigorous(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch, unsigned int *uiisInterlaced);
void         UndoPatternTypes5and7(Frame *frmBuffer[LASTFRAME], unsigned int firstPos);
void         Undo2Frames(Frame *frmBuffer1, Frame *frmBuffer2, BOOL BFF);
void         Analyze_Buffer_Stats(Frame *frmBuffer[LASTFRAME], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced);
void         Analyze_Buffer_Stats_Automode(Frame *frmBuffer[LASTFRAME], Pattern *ptrn, unsigned int *pdispatch);
unsigned char TelecineParityCheck(Frame frmBuffer);
void         Pattern32Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);
void         Pattern41aRemoval(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);
void         Pattern2332Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch);
void         Pattern41Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch);
void         RemovePattern(Frame **frmBuffer, unsigned int uiPatternNumber, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);

#ifdef __cplusplus
}
#endif 

#endif