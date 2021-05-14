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
/********************************************************************************
 * 
 * File: SkinDetBuff.c
 *
 * Buffers and buffer-related routines for skin detection
 * 
 ********************************************************************************/

#include "Def.h"
#include "SkinDetBuff.h"

//Get buffer sizes
void GetBufferSizes(Sizes *sz, int w, int h,  int bs) 
{
    sz->sz3     = (3*w*h);
    sz->szb1_5  = (3*(w/bs)*(h/bs))/2;
    sz->szb3    = (3*(w/bs)*(h/bs));
}

//Allocates memory buffers
int AllocateBuffers(SkinDetBuff *sdb, Sizes *sz) 
{
    INIT_MEMORY_C(sdb->pInYUV, 0, sz->szb3, BYTE);
    if (sdb->pInYUV == NULL) return 1;
    
    INIT_MEMORY_C(sdb->pYCbCr, 0, sz->szb3, BYTE);
    if (sdb->pYCbCr == NULL) return 1;
    
    return 0;
}

//De-allocates memory buffers
void FreeBuffers(SkinDetBuff *sdb) 
{
    DEINIT_MEMORY_C(sdb->pInYUV);
    DEINIT_MEMORY_C(sdb->pYCbCr);
}
