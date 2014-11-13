/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: deinterlacer.h

\* ****************************************************************************** */
#ifndef PTIR_DEINTERLACER_H
#define PTIR_DEINTERLACER_H

#pragma once

#include "../api.h"
#include <assert.h> 

#ifdef __cplusplus
extern "C" {
#endif

#define CLIPCROP(x, _min_, _max_, _min_v_, _max_v_, _def_v_) ((x) < (_min_) ? (_min_v_) : (x) > (_max_) ? (_max_v_) : (_def_v_))

__inline unsigned int EDIError(BYTE* PrvLinePixel, BYTE* NxtLinePixel, int dir);
void FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, unsigned int ybegin, unsigned int yend);
void FillBaseLinesIYUV(Frame *pSrc, Frame* pDst, int BottomLinesBaseY, int BottomLinesBaseUV);
void BilinearDeint(Frame *This, int BotBase);
void MedianDeinterlace(Frame *This, int BotBase);
void BuildLowEdgeMask_Main(Frame **frmBuffer, unsigned int frame, unsigned int BotBase);
void CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase);
void EdgeDirectionalIYUV_Main(Frame **frmBuffer, unsigned int curFrame, int BotBase);
void DeinterlaceBorders(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);
void DeinterlaceBilinearFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);
void DeinterlaceMedianFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);

#ifdef __cplusplus
}
#endif 
#endif