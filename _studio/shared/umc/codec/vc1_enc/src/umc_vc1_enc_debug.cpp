// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_debug.h"
#include "vm_debug.h"
#include "umc_vc1_enc_mb.h"
#include "umc_vc1_common_defs.h"


namespace UMC_VC1_ENCODER
{
#ifdef VC1_ENC_DEBUG_ON

VC1EncDebug *pDebug;

const uint32_t  VC1_ENC_POSITION     = 0x00000001;  //Frame num, frame type, frame size,
                                                  //MB, Block positions, skip info
const uint32_t  VC1_ENC_COEFFS       = 0x00000080;  // DC, AC coefficiens
const uint32_t  VC1_ENC_AC_PRED      = 0x00004000;  // AC prediction
const uint32_t  VC1_ENC_QUANT        = 0x00000008;  // quant info
const uint32_t  VC1_ENC_CBP          = 0x00000002;  // coded block patern info
const uint32_t  VC1_ENC_MV           = 0x00000020;  // motion vectors info
const uint32_t  VC1_ENC_PRED         = 0x00000040;  //interpolation info
const uint32_t  VC1_ENC_DEBLK_EDGE   = 0x00008000;  //deblocking edge
const uint32_t VC1_ENC_FIELD_MV      = 0x00002000;  //field motion vectors info
const uint32_t VC1_ENC_SMOOTHING     = 0x00000200;  //print restored frame
const uint32_t  VC1_ENC_DEBLOCKING   = 0x1;         //deblocking info

const uint32_t  VC1_ENC_FRAME_DEBUG  = 1;           //on/off frame debug
const uint32_t  VC1_ENC_FRAME_MIN    = 0;           //first frame to debug
const uint32_t  VC1_ENC_FRAME_MAX    = 3;           //last frame to debug

const uint32_t  VC1_ENC_DEBUG     = 0x000060e3;  //current debug output

VC1EncDebug::VC1EncDebug()
{
    LogFile     = NULL;
    FrameCount  = 0;
    FrameType   = 0;
    FrameSize   = 0;
    MBWidth   = 0;
    MBHeight  = 0;
    XPos      = 0;
    YPos      = 0;
    MBs          = NULL;
    pCurrMB      = NULL;
    InterpType = 0;
    RoundControl = 0;
    HalfCoef   = 1;
    DeblkFlag    = false;
    VTSFlag      = false;
    FieldPic     = 0;
    NumSlices    = 0;
    NV12         = 0;
};

VC1EncDebug::~VC1EncDebug()
{
    Close();
};

void VC1EncDebug::Init(int32_t Width, int32_t Height, uint32_t NV12flag)
{
    VM_ASSERT(Width > 0);
    VM_ASSERT(Height > 0);

    MBHeight = Height;
    MBWidth  = Width;
    RefNum   = 0;

    NumSlices = 0;

    //open log file
    LogFile = vm_file_open(VM_STRING("enc_log.txt"),VM_STRING("wb"));
    VM_ASSERT(LogFile != NULL);

    MBs = (VC1MBDebugInfo*)ippsMalloc_8u(sizeof(VC1MBDebugInfo) * MBWidth * MBHeight);

    VM_ASSERT(MBs != NULL);
    memset(MBs, 0, sizeof(VC1MBDebugInfo) * MBWidth * MBHeight);

    pCurrMB = MBs;

    NV12 = NV12flag;
};

void VC1EncDebug::Close()
{
    //close log file
    if(LogFile)
    {
        vm_file_close(LogFile);
        LogFile = NULL;
    }

    FrameCount  = 0;
    FrameType   = 0;
    FieldPic    = 0;
    FrameSize   = 0;
    MBWidth   = 0;
    MBHeight  = 0;
    XPos      = 0;
    YPos      = 0;
    RefNum    = 0;
    NumSlices = 0;
    
    if(MBs != NULL)
    {
        ippsFree(MBs);
        MBs = NULL;
    }
    pCurrMB      = NULL;
};

void VC1EncDebug::SetSliceInfo(uint32_t startRow, uint32_t endRow)
{
    Slices[NumSlices][0] = startRow;
    Slices[NumSlices][1] = endRow;
    NumSlices++;
}

void VC1EncDebug::SetPicType(int32_t picType)
{
    FrameType = picType;
    FieldPic  = 0;

    if(FrameType == VC1_ENC_I_I_FIELD || FrameType == VC1_ENC_I_P_FIELD
        || FrameType == VC1_ENC_P_I_FIELD ||  FrameType == VC1_ENC_P_P_FIELD
        || FrameType == VC1_ENC_B_BI_FIELD || FrameType == VC1_ENC_BI_B_FIELD
        || FrameType == VC1_ENC_B_B_FIELD)
        FieldPic = 1;
};

void VC1EncDebug::SetRefNum(uint32_t num)
{
    RefNum = num;
}

void VC1EncDebug::SetFrameSize(int32_t frameSize)
{
    FrameSize = frameSize;
};

void VC1EncDebug::WriteParams(int32_t /*_cur_frame*/, int32_t level, vm_char *format,...)
{
    vm_char line[1024];
    va_list args;

    if (!(level & VC1_ENC_DEBUG))
       return;

   va_start(args, format);
   vm_string_vsprintf(line, format, args);
   vm_string_fprintf(LogFile, line);
   //vm_string_printf(line);
};

void VC1EncDebug::SetRunLevelCoefs(uint8_t* run, int16_t* level, uint8_t* pairs, int32_t blk_num)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t pairNum = 0;
    int32_t coefNum = 0;

    pCurrMB->Block[blk_num].Pairs[0] = pairs[0];
    pCurrMB->Block[blk_num].Pairs[1] = pairs[1];
    pCurrMB->Block[blk_num].Pairs[2] = pairs[2];
    pCurrMB->Block[blk_num].Pairs[3] = pairs[3];

    for(i = 0; i < 4; i++)
    {
        pairNum =  pairs[i];

        for(j = 0; j < pairNum; j++)
        {
            pCurrMB->Block[blk_num].Run[coefNum]   = run[coefNum];
            pCurrMB->Block[blk_num].Level[coefNum] = level[coefNum];
            coefNum++;
        }
    }

     VM_ASSERT(coefNum <= 64);
};

void VC1EncDebug::NextMB()
{
    pCurrMB++;

    XPos++;

    XFieldPos++;
    if(XPos >= MBWidth)
    {
        XPos = 0;
        YPos++;
        XFieldPos = 0;
        YFieldPos++;
    }
};

void VC1EncDebug::SetCurrMBFirst(bool bSecondField)
{
    if(!bSecondField)
    {
        pCurrMB = MBs;
        XPos = 0;
        YPos = 0;
        XFieldPos = 0;
        YFieldPos = 0;
    }
    else
    {
        pCurrMB = MBs + MBHeight/2*MBWidth;
        XPos = 0;
        YPos = MBHeight/2;
        XFieldPos = 0;
        YFieldPos = 0;
    }
}

void VC1EncDebug::SetCurrMB(bool bSecondField, uint32_t NumMBX, uint32_t NumMBY)
{
    if(!bSecondField)
    {
        pCurrMB = MBs + NumMBY * MBWidth + NumMBX;
        YPos = NumMBX;
        XPos = NumMBY;
        XFieldPos = NumMBX;
        YFieldPos = NumMBY;
    }
    else
    {
        pCurrMB = MBs + MBHeight/2*MBWidth + NumMBY * MBWidth + NumMBX;
        XPos = NumMBX;
        YPos = MBHeight/2 + NumMBY;
        XFieldPos = NumMBX;
        YFieldPos = NumMBY;
    }
}

void VC1EncDebug::SetRLMode(uint8_t mode, int32_t blk_num, int32_t coef_num)
{
    assert(mode < 65);
    pCurrMB->Block[blk_num].Mode[coef_num] = mode;
};


void VC1EncDebug::SetBlockAsIntra(int32_t blk_num)
{
    pCurrMB->Block[blk_num].intra = 1;
};

void VC1EncDebug::SetBlockDifference(int16_t** pBlock, uint32_t* step)
{
    int32_t blk_num = 0;
    int32_t i = 0;
    int32_t j = 0;

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        for(i = 0; i < 8; i++)
        {
            for (j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BlockDiff[i*8 + j] = pBlock[blk_num][i * step[blk_num]/2 + j];
            }
        }
    }

};

void VC1EncDebug::SetMBAsSkip()
{
    pCurrMB->skip = 1;
};

void VC1EncDebug::SetDCACInfo(int32_t ACPred, int16_t* pBlock, int32_t BlockStep,
                              int16_t* pPredBlock, int32_t PredBlockStep,
                              int32_t direction, int32_t blk_num)
{
    int32_t i = 0;

    pCurrMB->Block[blk_num].DC = pBlock[0];
    pCurrMB->Block[blk_num].DCDiff = pPredBlock[0];
    pCurrMB->Block[blk_num].DCDirection = direction;

    pCurrMB->Block[blk_num].ACPred = ACPred;

    if(ACPred >= 0)
    {
        if(direction == VC1_ENC_LEFT)
        {
            //AC left prediction
            for(i = 1; i < 8; i++)
            {
                pCurrMB->Block[blk_num].AC[i - 1] = pBlock[i * BlockStep/2] - pPredBlock[i * PredBlockStep/2];
            }
        }
        else
        {
            //AC top prediction
            for(i = 1; i < 8; i++)
            {
                pCurrMB->Block[blk_num].AC[i - 1] = pBlock[i] - pPredBlock[i];
            }
        }
    }
};

void VC1EncDebug::SetCPB(int32_t predcbp, int32_t cbp)
{
    pCurrMB->PredCBP = predcbp;
    pCurrMB->CBP = cbp;
};

void VC1EncDebug::SetQuant(int32_t MQuant, uint8_t  HalfQP)
{
    pCurrMB->MQuant = MQuant;
    pCurrMB->HalfQP = HalfQP;

};

void VC1EncDebug::SetMVInfo(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 4; i++)
    {
        pCurrMB->Block[i].MV[0][forward]    = MV->x;
        pCurrMB->Block[i].MV[1][forward]    = MV->y;
        pCurrMB->Block[i].PredMV[0][forward] = predX;
        pCurrMB->Block[i].PredMV[1][forward] = predY;
    }
};

void VC1EncDebug::SetMVInfo(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward, int32_t blk_num)
{
    pCurrMB->Block[blk_num].MV[0][forward]    = MV->x;
    pCurrMB->Block[blk_num].MV[1][forward]    = MV->y;
    pCurrMB->Block[blk_num].PredMV[0][forward] = predX;
    pCurrMB->Block[blk_num].PredMV[1][forward] = predY;
};

void VC1EncDebug::SetMVInfoField(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 4; i++)
    {
        pCurrMB->Block[i].MV[0][forward]    = MV->x;
        pCurrMB->Block[i].MV[1][forward]    = MV->y;
        pCurrMB->Block[i].MVField[forward]  = MV->bSecond;
        pCurrMB->Block[i].PredMV[0][forward] = predX;
        pCurrMB->Block[i].PredMV[1][forward] = predY;
    }
};

void VC1EncDebug::SetMVInfoField(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward, int32_t blk_num)
{
    pCurrMB->Block[blk_num].MV[0][forward]    = MV->x;
    pCurrMB->Block[blk_num].MV[1][forward]    = MV->y;
    pCurrMB->Block[blk_num].MVField[forward]  = MV->bSecond;
    pCurrMB->Block[blk_num].PredMV[0][forward] = predX;
    pCurrMB->Block[blk_num].PredMV[1][forward] = predY;
};

void VC1EncDebug::SetIntrpMV(int16_t X, int16_t Y, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 4; i++)
    {
        pCurrMB->Block[i].IntrpMV[0][forward] = X;
        pCurrMB->Block[i].IntrpMV[1][forward] = Y;
    }
}

void VC1EncDebug::SetIntrpMV(int16_t X, int16_t Y, int32_t forward, int32_t blk_num)
{
    pCurrMB->Block[blk_num].IntrpMV[0][forward] = X;
    pCurrMB->Block[blk_num].IntrpMV[1][forward] = Y;
}

void VC1EncDebug::SetMVDiff(int16_t DiffX, int16_t DiffY, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 6; i++)
    {
        pCurrMB->Block[i].difMV[0][forward]    = DiffX;
        pCurrMB->Block[i].difMV[1][forward]    = DiffY;
    }
};

void VC1EncDebug::SetMVDiff(int16_t DiffX, int16_t DiffY, int32_t forward, int32_t blk_num)
{
    pCurrMB->Block[blk_num].difMV[0][forward]    = DiffX;
    pCurrMB->Block[blk_num].difMV[1][forward]    = DiffY;
};

void VC1EncDebug::SetFieldMVPred2Ref(sCoordinate *A, sCoordinate *C, int32_t forward)
{
    uint32_t i = 0;

    if(A)
    {
        for(i = 0; i < 6; i++)
        {
            pCurrMB->Block[i].FieldMV.pred[0][forward][0] = A->x;
            pCurrMB->Block[i].FieldMV.pred[0][forward][1] = A->y;
        }
    }

    if(C)
    {
        for(i = 0; i < 6; i++)
        {
            pCurrMB->Block[i].FieldMV.pred[2][forward][0] = C->x;
            pCurrMB->Block[i].FieldMV.pred[2][forward][1] = C->y;
        }
    }
}

void VC1EncDebug::SetFieldMVPred2Ref(sCoordinate *A, sCoordinate *C,     int32_t forward, int32_t blk)
{
    if(A)
    {
        pCurrMB->Block[blk].FieldMV.pred[0][forward][0] = A->x;
        pCurrMB->Block[blk].FieldMV.pred[0][forward][1] = A->y;
    }

    if(C)
    {
        pCurrMB->Block[blk].FieldMV.pred[2][forward][0] = C->x;
        pCurrMB->Block[blk].FieldMV.pred[2][forward][1] = C->y;
    }
}

void VC1EncDebug::SetFieldMVPred1Ref(sCoordinate *A, sCoordinate *C, int32_t forward)
{
    uint32_t i = 0;

    if(A)
    {
        for(i = 0; i < 6; i++)
        {
            pCurrMB->Block[i].FieldMV.pred[0][forward][0] = A->x;
            pCurrMB->Block[i].FieldMV.pred[0][forward][1] = A->y;
        }
    }

    if(C)
    {
        for(i = 0; i < 6; i++)
        {
            pCurrMB->Block[i].FieldMV.pred[2][forward][0] = C->x;
            pCurrMB->Block[i].FieldMV.pred[2][forward][1] = C->y;
        }
    }
}

void VC1EncDebug::SetFieldMVPred1Ref(sCoordinate *A, sCoordinate *C,  int32_t forward, int32_t blk)
{
    if(A)
    {
        pCurrMB->Block[blk].FieldMV.pred[0][forward][0] = A->x;
        pCurrMB->Block[blk].FieldMV.pred[0][forward][1] = A->y;
    }

    if(C)
    {
        pCurrMB->Block[blk].FieldMV.pred[2][forward][0] = C->x;
        pCurrMB->Block[blk].FieldMV.pred[2][forward][1] = C->y;
    }
}

void VC1EncDebug::SetHybrid(uint8_t hybrid, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 6; i++)
       pCurrMB->Block[i].FieldMV.hybrid[forward] = hybrid;
}

void VC1EncDebug::SetHybrid(uint8_t hybrid, int32_t forward, int32_t blk)
{
   pCurrMB->Block[blk].FieldMV.hybrid[forward] = hybrid;
}

void VC1EncDebug::SetScaleType(uint8_t type, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 6; i++)
        pCurrMB->Block[i].FieldMV.scaleType[forward] = type;
}

void VC1EncDebug::SetScaleType(uint8_t type, int32_t forward, int32_t blk)
{
    pCurrMB->Block[blk].FieldMV.scaleType[forward] = type;
}

void VC1EncDebug::SetPredFlag(uint8_t pred, int32_t forward)
{
    uint32_t i = 0;

    for(i = 0; i < 6; i++)
        pCurrMB->Block[i].FieldMV.predFlag[forward] = pred;
}

void VC1EncDebug::SetPredFlag(uint8_t pred, int32_t forward, int32_t blk)
{
    pCurrMB->Block[blk].FieldMV.predFlag[forward] = pred;
}

void VC1EncDebug::SetMBType(int32_t type)
{
    pCurrMB->MBType = type;
};
void VC1EncDebug::SetInterpInfoChroma(uint8_t* pBlockU, int32_t stepU,
                                      uint8_t* pBlockV, int32_t stepV,
                                      int32_t forward)
{
    int32_t i = 0;
    int32_t j = 0;


    if(!forward)
    {
        if(!NV12)
        {
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[4].FDst[i*8 + j] = pBlockU[j + i * stepU];
                }
            }
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[5].FDst[i*8 + j] = pBlockV[j + i * stepV];
                }
            }
        }
        else
        {
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[4].FDst[i*8 + j] = pBlockU[j*2 + i * stepU];
                }
            }

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[5].FDst[i*8 + j] = pBlockU[j*2 + 1 + i * stepV];
                }
            }
        }
    }
    else
    {
        if(!NV12)
        {
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[4].BDst[i*8 + j] = pBlockU[j + i * stepU];
                }
            }

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[5].BDst[i*8 + j] = pBlockV[j + i * stepV];
                }
            }
        }
        else
        {
            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[4].BDst[i*8 + j] = pBlockU[j*2 + i * stepU];
                }
            }

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[5].BDst[i*8 + j] = pBlockV[j*2 + i * stepV];
                }
            }

        }
    }

};


void VC1EncDebug::SetInterpInfoLuma(uint8_t* pBlock, int32_t step, int32_t blk_num, int32_t forward)
{
    int32_t i = 0;
    int32_t j = 0;


    if(!forward)
    {
        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
            }
        }
    }
    else
    {
        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
            }
        }
    }
 };

void VC1EncDebug::SetInterpInfo(IppVCInterpolate_8u* YInfo, IppVCInterpolate_8u* UInfo, IppVCInterpolate_8u* VInfo,
                                int32_t forward, bool padded)
{
    int32_t i = 0;
    int32_t j = 0;

    uint8_t* pBlock;
    int32_t step;
    int32_t blk_num;
    const uint8_t* pSrc;
    int32_t srcStep;

    if(!forward)
    {
        //block 0
        pBlock = YInfo->pDst;
        step = YInfo->dstStep;
        blk_num = 0;
        pSrc = YInfo->pSrc;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
            }
        }
        if(padded)
        {
            i = 0; j = 0;
        }
        else
        {
            i = 1; j = 1;
        }

        //block 1
        pBlock = YInfo->pDst + 8;
        step = YInfo->dstStep;
        blk_num = 1;
        pSrc = YInfo->pSrc + 8;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        if(padded)
        {
            i = 0; j = 0;
        }
        else
        {
            i = 1; j = 1;
        }

         //block 2
        pBlock = YInfo->pDst + 8 * YInfo->dstStep;
        step = YInfo->dstStep;
        blk_num = 2;
        pSrc = YInfo->pSrc + 8 * YInfo->srcStep;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        //block 3
        pBlock = YInfo->pDst + 8 * YInfo->dstStep + 8;
        step = YInfo->dstStep;
        blk_num = 3;
        pSrc = YInfo->pSrc + 8 * YInfo->srcStep + 8;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        if(!NV12)
        {
            //block 4
            pBlock = UInfo->pDst;
            step = UInfo->dstStep;
            blk_num = 4;
            pSrc = UInfo->pSrc;
            srcStep = UInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
                }
            }

            //block 5
            pBlock = VInfo->pDst;
            step = VInfo->dstStep;
            blk_num = 5;
            pSrc = VInfo->pSrc;
            srcStep = VInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[j + i * step];
                }
            }
        }
        else
        {
            //block 4
            pBlock = UInfo->pDst;
            step = UInfo->dstStep;
            blk_num = 4;
            pSrc = UInfo->pSrc;
            srcStep = UInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[2*j + i * step];
                }
            }

            //block 5
            pBlock = UInfo->pDst+1;
            step = UInfo->dstStep;
            blk_num = 5;
            pSrc = VInfo->pSrc;
            srcStep = VInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].FDst[i*8 + j] = pBlock[2*j + i * step];
                }
            }
        }
    }
    else
    {
        //block 0
        pBlock = YInfo->pDst;
        step = YInfo->dstStep;
        blk_num = 0;
        pSrc = YInfo->pSrc;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        if(padded)
        {
            i = 0; j = 0;
        }
        else
        {
            i = 1; j = 1;
        }

        //block 1
        pBlock = YInfo->pDst + 8;
        step = YInfo->dstStep;
        blk_num = 1;
        pSrc = YInfo->pSrc + 8;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        if(padded)
        {
            i = 0; j = 0;
        }
        else
        {
            i = 1; j = 1;
        }

        //block 2
        pBlock = YInfo->pDst + 8 * YInfo->dstStep;
        step = YInfo->dstStep;
        blk_num = 2;
        pSrc = YInfo->pSrc + 8 * YInfo->srcStep;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        //block 3
        pBlock = YInfo->pDst + 8 * YInfo->dstStep + 8;
        step = YInfo->dstStep;
        blk_num = 3;
        pSrc = YInfo->pSrc + 8 * YInfo->srcStep + 8;
        srcStep = YInfo->srcStep;

        for(i = 0; i < 8; i++)
        {
            for(j = 0; j < 8; j++)
            {
                pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
            }
        }

        if(!NV12)
        {
            //block 4
            pBlock = UInfo->pDst;
            step = UInfo->dstStep;
            blk_num = 4;
            pSrc = UInfo->pSrc;
            srcStep = UInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
                }
            }

            //block 5
            pBlock = VInfo->pDst;
            step = VInfo->dstStep;
            blk_num = 5;
            pSrc = VInfo->pSrc;
            srcStep = VInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[j + i * step];
                }
            }
        }
        else
        {
            //block 4
            pBlock = UInfo->pDst;
            step = UInfo->dstStep;
            blk_num = 4;
            pSrc = UInfo->pSrc;
            srcStep = UInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[2*j + i * step];
                }
            }

            //block 5
            pBlock = UInfo->pDst+1;
            step = UInfo->dstStep;
            blk_num = 5;
            pSrc = VInfo->pSrc;
            srcStep = VInfo->srcStep;

            for(i = 0; i < 8; i++)
            {
                for(j = 0; j < 8; j++)
                {
                    pCurrMB->Block[blk_num].BDst[i*8 + j] = pBlock[2*j + i * step];
                }
            }
        }
    }
}

void VC1EncDebug::SetInterpolType(int32_t type)
{
    InterpType = type;
}

void VC1EncDebug::SetRounControl(int32_t rc)
{
    RoundControl = rc;
}

void VC1EncDebug::SetACDirection(int32_t direction, int32_t blk_num)
{
    pCurrMB->Block[blk_num].ACPred = direction;
};

void VC1EncDebug::SetHalfCoef(bool half)
{
    HalfCoef = 1 + (uint32_t)(half);
};

void VC1EncDebug::SetDeblkFlag(bool flag)
{
    DeblkFlag = flag;
};

void VC1EncDebug::SetVTSFlag(bool flag)
{
    VTSFlag = flag;
};

void VC1EncDebug::SetDblkHorEdgeLuma(uint32_t ExHorEdge,     uint32_t InHorEdge,
                                     uint32_t InUpHorEdge,   uint32_t InBotHorEdge)
{
    pCurrMB->ExHorEdgeLuma = ExHorEdge;
    pCurrMB->InHorEdgeLuma = InHorEdge;
    pCurrMB->InUpHorEdgeLuma = InUpHorEdge;
    pCurrMB->InBotHorEdgeLuma = InBotHorEdge;

    //static FILE* f;
   //if (!f)
    //    f=fopen("set.txt","wb");

    //fprintf(f, "pCurrMB->ExHorEdgeLuma = %d\n",pCurrMB->ExHorEdgeLuma);
    //fprintf(f, "pCurrMB->InHorEdgeLuma = %d\n",pCurrMB->InHorEdgeLuma);
    //fprintf(f, "pCurrMB->InUpHorEdgeLuma = %d\n",pCurrMB->InUpHorEdgeLuma);
    //fprintf(f, "pCurrMB->InBotHorEdgeLuma = %d\n\n",pCurrMB->InBotHorEdgeLuma);

};
void VC1EncDebug::SetDblkVerEdgeLuma(uint32_t ExVerEdge,     uint32_t InVerEdge,
                        uint32_t InLeftVerEdge, uint32_t InRightVerEdge)
{
    pCurrMB->ExVerEdgeLuma = ExVerEdge;
    pCurrMB->InVerEdgeLuma = InVerEdge;
    pCurrMB->InLeftVerEdgeLuma = InLeftVerEdge;
    pCurrMB->InRightVerEdgeLuma = InRightVerEdge;

    //static FILE* f;

    //if (!f)
    //    f=fopen("set.txt","wb");

    //fprintf(f, "pCurrMB->ExVerEdgeLuma = %d\n",pCurrMB->ExVerEdgeLuma);
    //fprintf(f, "pCurrMB->InVerEdgeLuma = %d\n",pCurrMB->InVerEdgeLuma);
    //fprintf(f, "pCurrMB->InLeftVerEdgeLuma = %d\n",pCurrMB->InLeftVerEdgeLuma);
    //fprintf(f, "pCurrMB->InRightVerEdgeLuma = %d\n\n",pCurrMB->InRightVerEdgeLuma);
};

void VC1EncDebug::SetDblkHorEdgeU(uint32_t ExHorEdge, uint32_t InHorEdge)
{
    pCurrMB->ExHorEdgeU = ExHorEdge;
    pCurrMB->InHorEdgeU = InHorEdge;
};

void VC1EncDebug::SetDblkVerEdgeU(uint32_t ExVerEdge, uint32_t InVerEdge)
{
    pCurrMB->ExVerEdgeU = ExVerEdge;
    pCurrMB->InVerEdgeU = InVerEdge;
};

void VC1EncDebug::SetDblkHorEdgeV(uint32_t ExHorEdge, uint32_t InHorEdge)
{
    pCurrMB->ExHorEdgeV = ExHorEdge;
    pCurrMB->InHorEdgeV = InHorEdge;
};

void VC1EncDebug::SetDblkVerEdgeV(uint32_t ExVerEdge, uint32_t InVerEdge)
{
    pCurrMB->ExVerEdgeV = ExVerEdge;
    pCurrMB->InVerEdgeV = InVerEdge;
};

void VC1EncDebug::SetVTSType(eTransformType type[6])
{
    int32_t blk_num = 0;
    for(blk_num = 0; blk_num < 6; blk_num ++)
    {
        pCurrMB->Block[blk_num].VTSType = type[blk_num];
    }
}

void VC1EncDebug::PrintBlockDifference(int32_t blk_num)
{
    int32_t i = 0;
    int32_t j = 0;

    WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("Block %d\n"), blk_num);
    for(i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("%d  "), pCurrMB->Block[blk_num].BlockDiff[i * 8 + j]);
        }
        WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("\n"));
    }
};

void VC1EncDebug::PrintInterpolationInfo()
{
    int32_t blk_num = 0;

    switch(pCurrMB->MBType)
    {
        case (UMC_VC1_ENCODER::VC1_ENC_I_MB):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA):
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_P();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }
                }
            }
            PrintChroma_B_4MV(0);
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT):
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_B_F();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                   if((0 == (pCurrMB->Block[blk_num].MV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].MV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].MV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].MV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }
                }
            }

            //PrintChroma_B_4MV(0);

            //WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
            //    pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
            //WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
            //    pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_B_B();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                   if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 1);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 1);
                    }
               }
                PrintChroma_B_4MV(1);
            }


            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_FB):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_B_FB();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                   if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }

                    if((0 == (pCurrMB->Block[blk_num].MV[0][1] & 3)) && (0 == (pCurrMB->Block[blk_num].MV[1][1] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].MV[0][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].MV[1][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 1);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 1);
                    }
               }
                PrintChroma();
            }


            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

            break;
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_P();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if(!pCurrMB->Block[blk_num].intra)
                    {
                        if((0 == (pCurrMB->Block[blk_num].MV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].MV[1][0] & 3)))
                        {
                            PrintCopyPatchInterpolation(blk_num, 0);
                        }
                        else if(0 == (pCurrMB->Block[blk_num].MV[0][0] & 3))
                        {
                            PrintInterpQuarterPelBicubicVert(blk_num, 0);
                        }
                        else if(0 == (pCurrMB->Block[blk_num].MV[1][0] & 3))
                        {
                            PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                        }
                        else
                        {
                            PrintInterpQuarterPelBicubic(blk_num, 0);
                        }
                    }
                }
            }

            if(!pCurrMB->Block[4].intra)
                      PrintChroma_B_4MV(0);
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT):
            Print1MVHalfPelBilinear_B_FB();
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F_4MV):
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B_4MV):
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F_4MV):
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B_4MV):
            break;
    };
};

void VC1EncDebug::PrintInterpolationInfoField()
{
    int32_t blk_num = 0;

    switch(pCurrMB->MBType)
    {
        case (UMC_VC1_ENCODER::VC1_ENC_I_MB):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA):
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_PField();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }
                }
            }

            PrintChroma_P_4MVField();

            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F):
            Print1MVHalfPelBilinear_B_F_Field();
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B):
            Print1MVHalfPelBilinear_B_B_Field();
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_FB):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_B_FB_Field();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }

                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 1);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 1);
                    }
                }

                PrintChroma_B_4MVField();

                WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                    pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
                WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                    pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);
            }
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV):
        case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV):
            if(InterpType)
            {
                Print1MVHalfPelBilinear_P();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if(!pCurrMB->Block[blk_num].intra)
                    {
                        if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                        {
                            PrintCopyPatchInterpolation(blk_num, 0);
                        }
                        else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                        {
                            PrintInterpQuarterPelBicubicVert(blk_num, 0);
                        }
                        else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                        {
                            PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                        }
                        else
                        {
                            PrintInterpQuarterPelBicubic(blk_num, 0);
                        }
                    }
                }
            }

            if(!pCurrMB->Block[4].intra)
                      PrintChroma_B_4MV(0);
            break;

        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT):
            Print1MVHalfPelBilinear_B_FB_Field();
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F_4MV):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F_4MV):
           if(InterpType)
            {
                Print1MVHalfPelBilinear_B_FB_Field();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 0);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][0] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 0);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 0);
                    }
                }

                PrintChroma_P_4MVField();

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0], pCurrMB->Block[1].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[1].MV[0][1], pCurrMB->Block[1].MV[1][1], pCurrMB->Block[1].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0], pCurrMB->Block[2].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[2].MV[0][1], pCurrMB->Block[2].MV[1][1], pCurrMB->Block[2].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0], pCurrMB->Block[3].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[3].MV[0][1], pCurrMB->Block[3].MV[1][1], pCurrMB->Block[3].MVField[1]);

            }
            break;
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B_4MV):
        case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B_4MV):
           if(InterpType)
            {
                Print1MVHalfPelBilinear_B_FB_Field();
            }
            else
            {
                for(blk_num = 0; blk_num < 4; blk_num++)
                {
                    if((0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3)) && (0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3)))
                    {
                        PrintCopyPatchInterpolation(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[0][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicVert(blk_num, 1);
                    }
                    else if(0 == (pCurrMB->Block[blk_num].IntrpMV[1][1] & 3))
                    {
                        PrintInterpQuarterPelBicubicHoriz(blk_num, 1);
                    }
                    else
                    {
                        PrintInterpQuarterPelBicubic(blk_num, 1);
                    }
                }

                PrintChroma_B_4MV(1);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0], pCurrMB->Block[1].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[1].MV[0][1], pCurrMB->Block[1].MV[1][1], pCurrMB->Block[1].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0], pCurrMB->Block[2].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[2].MV[0][1], pCurrMB->Block[2].MV[1][1], pCurrMB->Block[2].MVField[1]);

            WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0], pCurrMB->Block[3].MVField[0]);
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
                pCurrMB->Block[3].MV[0][1], pCurrMB->Block[3].MV[1][1], pCurrMB->Block[3].MVField[1]);
           }
            break;
    };
};
void VC1EncDebug::PrintInterpQuarterPelBicubicVert(int32_t blk_num, uint8_t back)
{
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , back);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING(" vc1_MVMode1MV\n"));
    WriteParams(-1,VC1_ENC_PRED,VM_STRING("PelBicubicVert\n"));

   for (i = 0; i < 8; i ++)
   {
      for (j = 0; j < 8; j++)
      {
          if(!back)
          WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
          else
          WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
      }
       WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
    }
}

void VC1EncDebug::PrintInterpQuarterPelBicubicHoriz(int32_t blk_num, uint8_t back)
{
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , back);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING(" vc1_MVMode1MV\n"));
    WriteParams(-1,VC1_ENC_PRED,VM_STRING("PelBicubicHoriz\n"));

   for (i = 0; i < 8; i ++)
   {
      for (j = 0; j < 8; j++)
      {
          if(!back)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
          else
              WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
      }
       WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
    }
}
void VC1EncDebug::PrintInterpQuarterPelBicubic(int32_t blk_num, uint8_t back)
{
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , back);

    WriteParams(-1,VC1_ENC_PRED,VM_STRING(" vc1_MVMode1MV\n"));
    WriteParams(-1,VC1_ENC_PRED,VM_STRING("PelBicubicDiag\n"));

    if(!back)
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\n"),/*pCurrMB->Block[blk_num].FSrc,*/
            (pCurrMB->Block[blk_num].IntrpMV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
            (pCurrMB->Block[blk_num].IntrpMV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);
    else
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\n"),/*pCurrMB->Block[blk_num].BSrc,*/
        (pCurrMB->Block[blk_num].MV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        (pCurrMB->Block[blk_num].MV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);


   // WriteParams(-1,VC1_ENC_PRED,VM_STRING("Source\n"));


   //for (i = 0; i < 11; i ++)
   //{
   //   for (j = 0; j < 11; j++)
   //   {
   //       if(!back)
   //       WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FSource[j + i*11]);
   //       else
   //           WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BSource[j + i*11]);
   //   }
   //    WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
   // }

   WriteParams(-1,VC1_ENC_PRED,VM_STRING("rcontrol=%d\n"),RoundControl);

   WriteParams(-1,VC1_ENC_PRED,VM_STRING("after interpolate\n\n"));

   for (i = 0; i < 8; i ++)
   {
      for (j = 0; j < 8; j++)
      {
          if(!back)
          WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
          else
          WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
      }
       WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
    }
}

void VC1EncDebug::Print1MVHalfPelBilinear_B_B()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][1] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][1] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].BSrc,
        //    (pCurrMB->Block[blk_num].MV[0][1] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].MV[1][1] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }

        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

}

void VC1EncDebug::Print1MVHalfPelBilinear_B_F()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);
        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));
        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //    (pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

}
void VC1EncDebug::Print1MVHalfPelBilinear_B_FB()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        //forward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //    (pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

        //backward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][1] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][1] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);


        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].BSrc,
        //    (pCurrMB->Block[blk_num].MV[0][1] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].MV[1][1] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

    }
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

}
void VC1EncDebug::Print1MVHalfPelBilinear_B_B_Field()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][1] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][1] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].BSrc,
        //    (pCurrMB->Block[blk_num].IntrpMV[0][1] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].IntrpMV[1][1] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }

        WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);
}

void VC1EncDebug::Print1MVHalfPelBilinear_B_F_Field()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);
        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));
        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //    (pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);
}
void VC1EncDebug::Print1MVHalfPelBilinear_B_FB_Field()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        //forward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

        //backward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][1] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][1] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);


        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].BSrc,
        //    (pCurrMB->Block[blk_num].IntrpMV[0][1] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].IntrpMV[1][1] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

    }

    WriteParams(-1,VC1_ENC_MV,  VM_STRING("FORWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0], pCurrMB->Block[0].MVField[0]);
    WriteParams(-1,VC1_ENC_MV,  VM_STRING("BACKWARD: MV_X  = %d, MV_Y  = %d, bSecond = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1], pCurrMB->Block[0].MVField[1]);

}
void VC1EncDebug::Print1MVHalfPelBilinear_P()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 4; blk_num++)
    {
        //forward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
                pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
                pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //    (pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
}

void VC1EncDebug::Print1MVHalfPelBilinear_PField()
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t blk_num = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 0; blk_num < 4; blk_num++)
    {
        //forward
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
                pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
                pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" MVMode1MVHalfPelBilinear\n"));

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
}

void VC1EncDebug::PrintCopyPatchInterpolation(int32_t blk_num, uint8_t back)
{
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , back);

        if(blk_num < 4)
            WriteParams(-1,VC1_ENC_PRED,VM_STRING(" vc1_MVMode1MV\n"));

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("CopyPatch\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                if(!back)
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
                else
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
}

void VC1EncDebug::PrintChroma_B_4MV(uint8_t back)
{
    int32_t blk_num = 0;
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 4; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , back);

        //if(!back)
        //    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //        (pCurrMB->Block[blk_num].MV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //        (pCurrMB->Block[blk_num].MV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);
        //else
        //    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].BSrc,
        //        (pCurrMB->Block[blk_num].MV[0][back] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //        (pCurrMB->Block[blk_num].MV[1][back] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);


        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                if(!back)
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
                else
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
}

void VC1EncDebug::PrintChroma()
{
    int32_t blk_num = 0;
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 4; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][0] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][0] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].MV[0][1] + (XPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].MV[1][1] + (YPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);

 
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                    WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
}

void VC1EncDebug::PrintChroma_P_4MVField()
{
    int32_t blk_num = 0;
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 4; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        //WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d\t %d\t %d\n"),pCurrMB->Block[blk_num].FSrc,
        //    (pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4)>>2,
        //    (pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4)>>2);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }
    }
}

void VC1EncDebug::PrintChroma_B_4MVField()
{
    int32_t blk_num = 0;
    int32_t i = 0;
    int32_t j = 0;

    uint8_t Xshift[6] = {0, 8, 0, 8, 0, 0};
    uint8_t Yshift[6] = {0, 0, 8, 8, 0, 0};
    uint8_t pixelInBlk[6] = {16, 16, 16, 16, 8, 8};

    for(blk_num = 4; blk_num < 6; blk_num++)
    {
        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][0] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][0] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 0);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].FDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Block %d\n"), blk_num);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Before Interpolation %d, %d, forward %d\n"),
            pCurrMB->Block[blk_num].IntrpMV[0][1] + (XFieldPos * pixelInBlk[blk_num] + Xshift[blk_num])*4,
            pCurrMB->Block[blk_num].IntrpMV[1][1] + (YFieldPos * pixelInBlk[blk_num] + Yshift[blk_num])*4 , 1);

        WriteParams(-1,VC1_ENC_PRED,VM_STRING("Predicted pels\n"));
        for (i = 0; i < 8; i ++)
        {
            for (j = 0; j < 8; j++)
            {
                WriteParams(-1,VC1_ENC_PRED,VM_STRING("%d "),pCurrMB->Block[blk_num].BDst[j + i*8]);
            }
            WriteParams(-1,VC1_ENC_PRED,VM_STRING("\n"));
        }

    }
}

void VC1EncDebug::PrintFieldMV(int32_t forward)
{
    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("predictor_flag = %d\n"), pCurrMB->Block[0].FieldMV.predFlag[forward]);
    if(pCurrMB->Block[0].FieldMV.scaleType[forward] == 0)
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("opposite\n"));
    else  if(pCurrMB->Block[0].FieldMV.scaleType[forward] == 1)
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("same\n"));

    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("AX = %d AY = %d\n"),
                                    pCurrMB->Block[0].FieldMV.pred[0][forward][0],
                                    pCurrMB->Block[0].FieldMV.pred[0][forward][1]);

    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("CX = %d CY = %d\n"),
                                    pCurrMB->Block[0].FieldMV.pred[2][forward][0],
                                    pCurrMB->Block[0].FieldMV.pred[2][forward][1]);

    if(pCurrMB->Block[0].FieldMV.hybrid[forward])
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("hybrid\n"));
    else
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("no hybrid\n"));
}

void VC1EncDebug::PrintFieldMV(int32_t forward, int32_t blk)
{
    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("predictor_flag = %d\n"), pCurrMB->Block[blk].FieldMV.predFlag[forward]);
    if(pCurrMB->Block[blk].FieldMV.scaleType[forward] == 0)
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("opposite\n"));
    else  if(pCurrMB->Block[blk].FieldMV.scaleType[forward] == 1)
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("same\n"));

    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("AX = %d AY = %d\n"),
                                    pCurrMB->Block[blk].FieldMV.pred[0][forward][0],
                                    pCurrMB->Block[blk].FieldMV.pred[0][forward][1]);

    WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("CX = %d CY = %d\n"),
                                    pCurrMB->Block[blk].FieldMV.pred[2][forward][0],
                                    pCurrMB->Block[blk].FieldMV.pred[2][forward][1]);

    if(pCurrMB->Block[blk].FieldMV.hybrid[forward])
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("hybrid\n"));
    else
        WriteParams(-1,VC1_ENC_FIELD_MV,VM_STRING("no hybrid\n"));
}

void VC1EncDebug::PrintMVInfo()
{
    switch(pCurrMB->MBType)
    {
    case (UMC_VC1_ENCODER::VC1_ENC_I_MB):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA):
        break;

    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F):

        //MV diff
       if(!(pCurrMB->skip))
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

        if((pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_B_MB_F)
            || (pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F))
            WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));

        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);

        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B):
        //MV diff
        if(!(pCurrMB->skip))
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][1], pCurrMB->Block[0].difMV[1][1]);

        WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));

            //MV predictors
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_FB):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB):

        //MV diff
        if(!(pCurrMB->skip))
        {
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][1], pCurrMB->Block[0].difMV[1][1]);

            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);
        }

        WriteParams(-1,VC1_ENC_MV, VM_STRING("BDirecton\n"));
        WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));

        //MV predictors
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);

        WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));

        //MV
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

        break;

    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV):

        //Block 0
        if(!pCurrMB->Block[0].intra)
        {
            //MV diff
            if(!(pCurrMB->skip) || FieldPic)
                WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                        pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        }

        //Block 1
        if(!pCurrMB->Block[1].intra)
        {
            //MV diff
            if(!(pCurrMB->skip) || FieldPic)
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[1].difMV[0][0], pCurrMB->Block[1].difMV[1][0]);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[1].PredMV[0][0], pCurrMB->Block[1].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0]);
        }
        //Block 2
        if(!pCurrMB->Block[2].intra)
        {
            //MV diff
            if(!(pCurrMB->skip) || FieldPic)
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[2].difMV[0][0], pCurrMB->Block[2].difMV[1][0]);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[2].PredMV[0][0], pCurrMB->Block[2].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0]);
        }

        //Block 3
        if(!pCurrMB->Block[3].intra)
        {
            //MV diff
            if(!(pCurrMB->skip) || FieldPic)
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[3].difMV[0][0], pCurrMB->Block[3].difMV[1][0]);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[3].PredMV[0][0], pCurrMB->Block[3].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0]);
        }
        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT):
                WriteParams(-1,VC1_ENC_MV, VM_STRING("direct\n"));
        break;
    };
};

void VC1EncDebug::PrintMVFieldInfo()
{
    switch(pCurrMB->MBType)
    {
    case (UMC_VC1_ENCODER::VC1_ENC_I_MB):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA):
        break;

    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F):
        {

        //MV diff
         WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

        PrintFieldMV(0);

        if((pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_B_MB_F)
            || (pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F))
            WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));

        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        }
        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B):
        {
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][1], pCurrMB->Block[0].difMV[1][1]);

        PrintFieldMV(1);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));

            //MV predictors
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);
        }

        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_FB):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB):

        WriteParams(-1,VC1_ENC_MV, VM_STRING("BDirecton\n"));
        //MV diff
         WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

        PrintFieldMV(0);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));

        //MV predictors
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);

       //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[0].difMV[0][1], pCurrMB->Block[0].difMV[1][1]);

        PrintFieldMV(1);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));

        //MV
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

        break;

    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV):
    case (UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV):

        //Block 0
        if(!pCurrMB->Block[0].intra)
        {
            //MV diff
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                        pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

            PrintFieldMV(0, 0);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        }

        //Block 1
        if(!pCurrMB->Block[1].intra)
        {
            //MV diff
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[1].difMV[0][0], pCurrMB->Block[1].difMV[1][0]);

            PrintFieldMV(0, 1);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[1].PredMV[0][0], pCurrMB->Block[1].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0]);
        }
        //Block 2
        if(!pCurrMB->Block[2].intra)
        {
            //MV diff
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[2].difMV[0][0], pCurrMB->Block[2].difMV[1][0]);

            PrintFieldMV(0, 2);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[2].PredMV[0][0], pCurrMB->Block[2].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0]);
        }

        //Block 3
        if(!pCurrMB->Block[3].intra)
        {
            //MV diff
            WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[3].difMV[0][0], pCurrMB->Block[3].difMV[1][0]);

            PrintFieldMV(0, 3);

            //MV predictor
            WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
                pCurrMB->Block[3].PredMV[0][0], pCurrMB->Block[3].PredMV[1][0]);

            //MV
            WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
                pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0]);
        }
        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT):
        WriteParams(-1,VC1_ENC_MV, VM_STRING("direct\n"));
        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_F_4MV):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F_4MV):
    {
        WriteParams(-1,VC1_ENC_MV, VM_STRING("4MV forward\n"));
        //Block 0
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[0].difMV[0][0], pCurrMB->Block[0].difMV[1][0]);

       // WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        PrintFieldMV(0, 0);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);

        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n")); 
        
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        //PrintFieldMV(1, 0);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //Block 1
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[1].difMV[0][0], pCurrMB->Block[1].difMV[1][0]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        PrintFieldMV(0, 1);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[1].PredMV[0][0], pCurrMB->Block[1].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        //PrintFieldMV(1, 1);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[1].PredMV[0][1], pCurrMB->Block[1].PredMV[1][1]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[1].MV[0][1], pCurrMB->Block[1].MV[1][1]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //Block 2
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[2].difMV[0][0], pCurrMB->Block[2].difMV[1][0]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        PrintFieldMV(0, 2);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[2].PredMV[0][0], pCurrMB->Block[2].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0]);

        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));  
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        //PrintFieldMV(1, 2);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[2].PredMV[0][1], pCurrMB->Block[2].PredMV[1][1]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[2].MV[0][1], pCurrMB->Block[2].MV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //Block 3
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[3].difMV[0][0], pCurrMB->Block[3].difMV[1][0]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        PrintFieldMV(0, 3);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[3].PredMV[0][0], pCurrMB->Block[3].PredMV[1][0]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        //PrintFieldMV(1, 3);

        //        //MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[3].PredMV[0][1], pCurrMB->Block[3].PredMV[1][1]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[3].MV[0][1], pCurrMB->Block[3].MV[1][1]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));
    }
        break;
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_B_4MV):
    case (UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B_4MV):
    {
        WriteParams(-1,VC1_ENC_MV, VM_STRING("4MV backward\n"));
        //Block 0
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                    pCurrMB->Block[0].difMV[0][1], pCurrMB->Block[0].difMV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        PrintFieldMV(1, 0);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[0].PredMV[0][1], pCurrMB->Block[0].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[0].MV[0][1], pCurrMB->Block[0].MV[1][1]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        //PrintFieldMV(0, 0);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[0].PredMV[0][0], pCurrMB->Block[0].PredMV[1][0]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[0].MV[0][0], pCurrMB->Block[0].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));


        //Block 1
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[1].difMV[0][1], pCurrMB->Block[1].difMV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        PrintFieldMV(1, 1);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[1].PredMV[0][1], pCurrMB->Block[1].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[1].MV[0][1], pCurrMB->Block[1].MV[1][1]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        //PrintFieldMV(0, 1);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[1].PredMV[0][0], pCurrMB->Block[1].PredMV[1][0]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[1].MV[0][0], pCurrMB->Block[1].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //Block 2
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[2].difMV[0][1], pCurrMB->Block[2].difMV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        PrintFieldMV(1, 2);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[2].PredMV[0][1], pCurrMB->Block[2].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[2].MV[0][1], pCurrMB->Block[2].MV[1][1]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        //PrintFieldMV(0, 2);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[2].PredMV[0][0], pCurrMB->Block[2].PredMV[1][0]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[2].MV[0][0], pCurrMB->Block[2].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //Block 3
        //MV diff
        WriteParams(-1,VC1_ENC_MV, VM_STRING("DMV_X = %d, DMV_Y = %d\n"),
                pCurrMB->Block[3].difMV[0][1], pCurrMB->Block[3].difMV[1][1]);

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("backward\n"));
        PrintFieldMV(1, 3);
        //MV predictor
        WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
            pCurrMB->Block[3].PredMV[0][1], pCurrMB->Block[3].PredMV[1][1]);

        //MV
        WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
            pCurrMB->Block[3].MV[0][1], pCurrMB->Block[3].MV[1][1]);
        WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

        //WriteParams(-1,VC1_ENC_MV, VM_STRING("forward\n"));
        //PrintFieldMV(0, 3);

        ////MV predictor
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("PredictorX = %d, PredictorY = %d\n"),
        //    pCurrMB->Block[3].PredMV[0][0], pCurrMB->Block[3].PredMV[1][0]);

        ////MV
        //WriteParams(-1,VC1_ENC_MV,  VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),
        //    pCurrMB->Block[3].MV[0][0], pCurrMB->Block[3].MV[1][0]);
        //WriteParams(-1,VC1_ENC_MV, VM_STRING("\n\n"));

    }
        break;
    };
};

void VC1EncDebug::PrintRunLevel(int32_t blk_num)
{
    int32_t i = 0;
    int32_t SBNum = 0;
    int32_t pairsNum = 0;
    int32_t last = 0;
    int32_t mode = 0;
    int32_t coefNum = 0;

    for(SBNum = 0; SBNum < 4; SBNum++)
    {
        pairsNum = pCurrMB->Block[blk_num].Pairs[SBNum];

        for(i = 0; i < pairsNum; i++)
        {
            if(i == (pairsNum - 1)) last = 1;
            mode = pCurrMB->Block[blk_num].Mode[coefNum];

            switch(mode)
            {
            case 0:
                break;
            case 1:
            case 2:
            case 3:
                WriteParams(-1,VC1_ENC_COEFFS,VM_STRING("Index = ESCAPE\n"));
                WriteParams(-1,VC1_ENC_COEFFS,VM_STRING("Index = ESCAPE Mode %d\n"), mode);
                break;
            };

            WriteParams(-1,VC1_ENC_COEFFS,VM_STRING("AC Run=%2d Level=%c%3d Last=%d\n"),
                pCurrMB->Block[blk_num].Run [coefNum],
                (pCurrMB->Block[blk_num].Level[coefNum] < 0) ? '-' : '+',
                (pCurrMB->Block[blk_num].Level[coefNum] < 0) ?
                -pCurrMB->Block[blk_num].Level[coefNum] :
            pCurrMB->Block[blk_num].Level[coefNum],
                last);
            coefNum++;
        }

        last = 0;
    }
};

void VC1EncDebug::PrintDblkInfo(uint32_t start, uint32_t end)
{
    if(!VTSFlag || FrameType == 0)
        PrintDblkInfoNoVTS(start, end);
    else
        PrintDblkInfoVTS(start, end);
};
void VC1EncDebug::PrintFieldDblkInfo(bool bSecondField, uint32_t start, uint32_t end)
{
    if(!VTSFlag || FrameType == 0)
        PrintFieldDblkInfoNoVTS(bSecondField, start, end);
    else
        PrintFieldDblkInfoVTS(bSecondField, start, end);
};
void VC1EncDebug::PrintDblkInfoVTS(uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = 0;

    //horizontal info
    pCurrMB = MBs + start * MBWidth;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
             //horizontal info
            //block 0
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 0);

            //internal horizontal block 0
            if((pCurrMB->InUpHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 0);

            if((pCurrMB->InUpHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 0);

            //block 1
            if((pCurrMB->InHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 1);

            if((pCurrMB->InHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 1);

            //internal horizontal block 1
            if((pCurrMB->InUpHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 1);

            if((pCurrMB->InUpHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 1);

            if(i != end - 1)
            {
                //block 2
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 2);
            }

            //internal horizontal block 2
            if((pCurrMB->InBotHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 2);

            if((pCurrMB->InBotHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 2);

            if(i != end - 1)
            {
                //block 3
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 3);
            }

            //internal horizontal block 3
            if((pCurrMB->InBotHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 3);

            if((pCurrMB->InBotHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 3);

            if(i != end - 1)
            {
                //chroma
                //block 4
                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0xC) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 4);
            }

            //internal horizontal block 4
            if((pCurrMB->InHorEdgeU &  0x3) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 4);

            if((pCurrMB->InHorEdgeU &  0x0C) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 4);

            if(i != end - 1)
            {
                //block 5
                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 5);
            }
            //internal horizontal block 5
            if((pCurrMB->InHorEdgeV &  0x3) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 5);

            if((pCurrMB->InHorEdgeV &  0x0C) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 5);

            NextMB();
        }
        k++;
    }

    //vertical info
    pCurrMB = MBs + start * MBWidth;
    k = 0;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            //block 0
            if((pCurrMB->InVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 0);

            //internal vertical block 0
            if((pCurrMB->InLeftVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 0);

            if((pCurrMB->InLeftVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 0);

            //block 2
            if((pCurrMB->InVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 2);

            if((pCurrMB->InVerEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 2);

            //internal vertical block 2
            if((pCurrMB->InLeftVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 2);

            if((pCurrMB->InLeftVerEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 2);


            if(j != MBWidth - 1)
            {
                //block 1
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 1);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 1);

                //internal vertical block 1
                if((pCurrMB->InRightVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 1);

                if((pCurrMB->InRightVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 1);

                //block 3
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 3);

                //internal vertical block 3
                if((pCurrMB->InRightVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 3);

                if((pCurrMB->InRightVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 3);

                //block 4
                if(((pCurrMB + 1)->ExVerEdgeU & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 4);

                //internal vertical block 4
                if((pCurrMB->InVerEdgeU &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 4);

                if((pCurrMB->InVerEdgeU &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 4);

                //block 5
                if(((pCurrMB + 1)->ExVerEdgeV & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 5);

                //internal vertical block 5
                if((pCurrMB->InVerEdgeV &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 5);

                if((pCurrMB->InVerEdgeV &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 5);
            }
            else
            {
                //internal vertical block 1
                if((pCurrMB->InRightVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 1);

                if((pCurrMB->InRightVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 1);

                //internal vertical block 3
                if((pCurrMB->InRightVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 3);

                if((pCurrMB->InRightVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 3);

                //internal vertical block 4
                if((pCurrMB->InVerEdgeU &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 4);

                if((pCurrMB->InVerEdgeU &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 4);

                //internal vertical block 5
                if((pCurrMB->InVerEdgeV &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 5);

                if((pCurrMB->InVerEdgeV &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 5);
            }

            NextMB();
        }
        k++;
    }
};

void VC1EncDebug::PrintFieldDblkInfoVTS(bool bSecondField, uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = 0;

    //horizontal info
    pCurrMB = MBs + start*MBWidth;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
             //horizontal info
            //block 0
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 0);

            //internal horizontal block 0
            if((pCurrMB->InUpHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 0);

            if((pCurrMB->InUpHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 0);

            //block 1
            if((pCurrMB->InHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 1);

            if((pCurrMB->InHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 1);

            //internal horizontal block 1
            if((pCurrMB->InUpHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 1);

            if((pCurrMB->InUpHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 1);

            if(i != end - 1)
            {
                //block 2
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 2);
            }

            //internal horizontal block 2
            if((pCurrMB->InBotHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 2);

            if((pCurrMB->InBotHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 2);

            if(i != end - 1)
            {
                //block 3
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 3);
            }

            //internal horizontal block 3
            if((pCurrMB->InBotHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 3);

            if((pCurrMB->InBotHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 3);

            if(i != end - 1)
            {
                //chroma
                //block 4
                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0xC) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 4);
            }

            //internal horizontal block 4
            if((pCurrMB->InHorEdgeU &  0x3) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 4);

            if((pCurrMB->InHorEdgeU &  0x0C) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 4);

            if(i != end - 1)
            {
                //block 5
                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 5);

            }
            
            //internal horizontal block 5
            if((pCurrMB->InHorEdgeV &  0x3) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 VTS, blk = %d\n"), 5);

            if((pCurrMB->InHorEdgeV &  0x0C) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 VTS, blk = %d\n"), 5);

            NextMB();
        }
        k++;
    }

    //vertical info
    pCurrMB = MBs + start*MBWidth;
    k = 0;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            //block 0
            if((pCurrMB->InVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 0);

            //internal vertical block 0
            if((pCurrMB->InLeftVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 0);

            if((pCurrMB->InLeftVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 0);

            //block 2
            if((pCurrMB->InVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 2);

            if((pCurrMB->InVerEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 2);

            //internal vertical block 2
            if((pCurrMB->InLeftVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 2);

            if((pCurrMB->InLeftVerEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 2);


            if(j != MBWidth - 1)
            {
                //block 1
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 1);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 1);

                //internal vertical block 1
                if((pCurrMB->InRightVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 1);

                if((pCurrMB->InRightVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 1);

                //block 3
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 3);

                //internal vertical block 3
                if((pCurrMB->InRightVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 3);

                if((pCurrMB->InRightVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 3);

                //block 4
                if(((pCurrMB + 1)->ExVerEdgeU & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 4);

                //internal vertical block 4
                if((pCurrMB->InVerEdgeU &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 4);

                if((pCurrMB->InVerEdgeU &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 4);

                //block 5
                if(((pCurrMB + 1)->ExVerEdgeV & 0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 5);

                //internal vertical block 5
                if((pCurrMB->InVerEdgeV &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 5);

                if((pCurrMB->InVerEdgeV &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 5);
            }
            else
            {
                //internal vertical block 1
                if((pCurrMB->InRightVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 1);

                if((pCurrMB->InRightVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 1);

                //internal vertical block 3
                if((pCurrMB->InRightVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 3);

                if((pCurrMB->InRightVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 3);

                //internal vertical block 4
                if((pCurrMB->InVerEdgeU &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 4);

                if((pCurrMB->InVerEdgeU &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 4);

                //internal vertical block 5
                if((pCurrMB->InVerEdgeV &  0x3) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 VTS, blk = %d\n"), 5);

                if((pCurrMB->InVerEdgeV &  0x0C) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 VTS, blk = %d\n"), 5);
            }

            NextMB();
        }
        k++;
    }
};

void VC1EncDebug::PrintDblkInfoNoVTS(uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = 0;

    //horizontal info
    pCurrMB = MBs + start * MBWidth;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
    //static FILE* f;
    //if (!f)
    //    f=fopen("print.txt","wb");

    //fprintf(f, "pCurrMB->ExHorEdgeLuma = %d\n",pCurrMB->ExHorEdgeLuma);
    //fprintf(f, "pCurrMB->InHorEdgeLuma = %d\n",pCurrMB->InHorEdgeLuma);
    //fprintf(f, "pCurrMB->InUpHorEdgeLuma = %d\n",pCurrMB->InUpHorEdgeLuma);
    //fprintf(f, "pCurrMB->InBotHorEdgeLuma = %d\n\n",pCurrMB->InBotHorEdgeLuma);

             //horizontal info
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 1);

            if((pCurrMB->InHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 1);

            if(i != end - 1)
            {
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 5);
            }

            NextMB();
        }
        k++;
    }

    //vertical info
    pCurrMB = MBs + start * MBWidth;
    k = 0;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
    //static FILE* f;

    //if (!f)
    //    f=fopen("print.txt","wb");

    //fprintf(f, "pCurrMB->ExVerEdgeLuma = %d\n",pCurrMB->ExVerEdgeLuma);
    //fprintf(f, "pCurrMB->InVerEdgeLuma = %d\n",pCurrMB->InVerEdgeLuma);
    //fprintf(f, "pCurrMB->InLeftVerEdgeLuma = %d\n",pCurrMB->InLeftVerEdgeLuma);
    //fprintf(f, "pCurrMB->InRightVerEdgeLuma = %d\n\n",pCurrMB->InRightVerEdgeLuma);

            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 2);

            if((pCurrMB->InVerEdgeLuma &  0xC) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 2);

            if(j != MBWidth - 1)
            {
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 1);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 1);


                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 5);
            }

            NextMB();
        }
        k++;
    }

};
void VC1EncDebug::PrintFieldDblkInfoNoVTS(bool bSecondField, uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = 0;
    //horizontal info
    pCurrMB = MBs + start * MBWidth;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
             //horizontal info
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InHorEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 0);

            if((pCurrMB->InHorEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 1);

            if((pCurrMB->InHorEdgeLuma &  0x8) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 1);

            if(i != end - 1)
            {
                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 2);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 3);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeU & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 4);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + MBWidth)->ExHorEdgeV & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("hor2 = 0, blk = %d\n"), 5);
            }

            NextMB();
        }

        k++;
    }

    //vertical info
    pCurrMB = MBs + start * MBWidth;
    k = 0;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
            WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("X = %d, Y = %d\n"), j, k);

            if((pCurrMB->InVerEdgeLuma &  0x1) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x2) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 0);

            if((pCurrMB->InVerEdgeLuma &  0x4) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 2);

            if((pCurrMB->InVerEdgeLuma &  0xC) == 0)
                WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 2);

            if(j != MBWidth - 1)
            {
                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 1);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 1);


                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x4) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeLuma &  0x8) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 3);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeU & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 4);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x1) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver1 = 0, blk = %d\n"), 5);

                if(((pCurrMB + 1)->ExVerEdgeV & 0x2) == 0)
                    WriteParams(-1, VC1_ENC_DEBLK_EDGE, VM_STRING("ver2 = 0, blk = %d\n"), 5);
            }

            NextMB();
        }

        k++;
    }

};
void VC1EncDebug::PrintRestoredFrame(uint8_t* pY, int32_t Ystep, uint8_t* pU, int32_t Ustep, uint8_t* pV, int32_t Vstep, bool BFrame)
{
    if(VC1_ENC_DEBLOCKING || VC1_ENC_SMOOTHING)
    {
        static FILE* f=0;
        static FILE* f1=0;
        static uint32_t framenum = 0;

        FILE* out = 0;
        if (!f)
        f =fopen("ENC_Deblock.txt","wb");
        if (!f1)
        f1=fopen("ENC_Deblock_B.txt","wb");

        if(!BFrame)
            out = f;
        else
            out = f1;
        assert(out!=NULL);

        int32_t i, j, k, t;
        uint8_t* ptr = NULL;

        fprintf(out, "Frame %d\n",framenum);

        for(i = 0; i < MBHeight; i++)
        {
            for (j = 0; j < MBWidth; j++)
            {
                ptr = pY + i * 16 * Ystep + 16*j;

                fprintf(out, "X == %d Y == %d\n", j, i);

                fprintf(out, "LUMA\n");

                for(k = 0; k<16; k++)
                {
                    for(t = 0; t < 16; t++)
                    {
                        fprintf(out, "%d ", *(ptr + k*Ystep + t));
                    }
                    fprintf(out, "\n");
                }


                fprintf(out, "Chroma U\n");

                if(!NV12)
                {
                    ptr = pU + i*Ustep*8 + 8*j;
                    for(k = 0; k < 8; k++)
                    {
                        for(int32_t t = 0; t < 8; t++)
                        {
                            fprintf(out, "%d ", *(ptr + k*Ustep + t));
                        }
                        fprintf(out, "\n");
                    }
                }
                else
                {
                    ptr = pU + i*Ustep*8 + 16*j;
                   for(k = 0; k < 8; k++)
                    {
                        for(int32_t t = 0; t < 8; t++)
                        {
                            fprintf(out, "%d ", *(ptr + k*Ustep + t*2));
                        }
                        fprintf(out, "\n");
                    }
                }


                fprintf(out, "Chroma V\n");

                if(!NV12)
                {
                    ptr = pV + i*Vstep*8 + 8*j;
                    for(k = 0; k < 8; k++)
                    {
                        for(int32_t t = 0; t < 8; t++)
                        {
                            fprintf(out, "%d ", *(ptr + k*Vstep + t));
                        }
                        fprintf(out, "\n");
                    }
                }
                else
                {
                    ptr = pV + i*Vstep*8 + 16*j;
                    for(k = 0; k < 8; k++)
                    {
                        for(int32_t t = 0; t < 8; t++)
                        {
                            fprintf(out, "%d ", *(ptr + k*Vstep + t*2));
                        }
                        fprintf(out, "\n");
                    }
                }
            }
        }

        //if (!f1)
        //    f1=fopen("encRestoredFrames.yuv","wb");
        //for (i=0; i < MBHeight*16; i++)
        //{
        //    fwrite(pY+i*Ystep, 1, MBWidth*16, f1);

        //}
        //for (i=0; i < MBHeight*8; i++)
        //{
        //    fwrite(pU+i*Ustep, 1, MBWidth*8, f1);

        //}
        //for (i=0; i < MBHeight*8; i++)
        //{
        //    fwrite(pV+i*Vstep, 1, MBWidth*8, f1);

        //}
        //fflush(f1);

        fflush(f);
        framenum++;
    }
}

void VC1EncDebug::PrintSmoothingDataFrame(uint8_t MBXpos, uint8_t MBYpos, VC1EncoderMBData* pCurRData,
                                         VC1EncoderMBData* pLeftRData,
                                         VC1EncoderMBData* pLeftTopRData,
                                         VC1EncoderMBData* pTopRData)
{
    return;

    uint32_t i = 0, j = 0;

    if(VC1_ENC_DEBUG & VC1_ENC_SMOOTHING)
    {
        static FILE* f=0;
        static uint32_t framenum = 0;

        if (!f)
            f =fopen("enc_smoothInfo.txt","wb");

        if(MBXpos == 0 && MBYpos == 0)
        {
            fprintf(f, "Frame %d\n",framenum);
            framenum++;
        }

        fprintf(f,"maroblock-%d,%d\n",MBXpos,MBYpos);

        if (MBXpos > 0 && MBYpos > 0)
        {
            if(pLeftTopRData)
            {
                fprintf(f,"left upper maroblock\n luma\n");
                for (i=0;i<16;i++)
                {
                    for (j=0;j<16;j++)
                        fprintf(f,"%3d ",pLeftTopRData->m_pBlock[0][j + i *16]);
                    
                    fprintf(f,"\n ");
                }

                fprintf(f,"U\n");
                for (i=0;i<8;i++)
                {
                    for (j=0;j<8;j++)
                        fprintf(f,"%3d ",pLeftTopRData->m_pBlock[4][j + i *8]);
                    
                    fprintf(f,"\n ");
                }
                fprintf(f,"V\n");
                for (i=0;i<8;i++)
                {
                    for (j=0;j<8;j++)
                        fprintf(f,"%3d ",pLeftTopRData->m_pBlock[5][j + i *8]);
                    
                    fprintf(f,"\n ");
                }
            }
        }


        if (MBXpos>0)
        {
            if(pLeftRData)
            {
                fprintf(f,"left  maroblock\n luma\n");
                for (i=0;i<16;i++)
                {
                    for (j=0;j<16;j++)
                        fprintf(f,"%3d ",pLeftRData->m_pBlock[0][j + i *16]);
                    
                    fprintf(f,"\n ");
                }
                fprintf(f,"U\n");
                for (i=0;i<8;i++)
                {
                    for (j=0;j<8;j++)
                        fprintf(f,"%3d ",pLeftRData->m_pBlock[4][j + i *8]);
                    
                    fprintf(f,"\n ");
                }
                fprintf(f,"V\n");
                for (i=0;i<8;i++)
                {
                    for (j=0;j<8;j++)
                        fprintf(f,"%3d ",pLeftRData->m_pBlock[5][j + i *8]);
                    
                    fprintf(f,"\n ");
                }
            }
        }

        if(pCurRData)
        {
            fprintf(f,"current maroblock\n luma\n");
            for (i=0;i<16;i++)
            {
                for (j=0;j<16;j++)
                    fprintf(f,"%3d ",pCurRData->m_pBlock[0][j + i *16]);
                
                fprintf(f,"\n ");
            }
            fprintf(f,"U\n");
            for (i=0;i<8;i++)
            {
                for (j=0;j<8;j++)
                    fprintf(f,"%3d ",pCurRData->m_pBlock[4][j + i *8]);
                
                fprintf(f,"\n ");
            }
            fprintf(f,"V\n");
            for (i=0;i<8;i++)
            {
                for (j=0;j<8;j++)
                    fprintf(f,"%3d ",pCurRData->m_pBlock[5][j + i *8]);
                
                fprintf(f,"\n ");
            }
        }

        }
}


void VC1EncDebug::PrintBlkVTSInfo()
{
    int32_t blk_num = 0;

    if (pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_I_MB)
        return;

    if(pCurrMB->skip || pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_I_MB
        || pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA
        || pCurrMB->MBType == UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA)
        return;

    for(blk_num = 0; blk_num < 6; blk_num ++)
    {
        WriteParams(-1,VC1_ENC_CBP,VM_STRING("VTS blk = %d %d\n"), blk_num, pCurrMB->Block[blk_num].VTSType);

    }
}

void VC1EncDebug::PrintPictureType()
{
    switch(FrameType)
    {
        case VC1_ENC_I_FRAME:       WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = I frame\n"));           break;
        case VC1_ENC_P_FRAME:       WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = P frame\n"));           break;
        //case VC1_ENC_P_FRAME_MIXED: WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = P mixed frame\n"));     break;
        case VC1_ENC_B_FRAME:       WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = B frame\n"));           break;
        case VC1_ENC_BI_FRAME:      WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = BI frame\n"));          break;
        case VC1_ENC_SKIP_FRAME:    WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = SKIP frame\n"));        break;
        case VC1_ENC_I_I_FIELD:     WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = I_I field frame\n"));   break;
        case VC1_ENC_I_P_FIELD:     WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = I_P field frame\n"));   break;
        case VC1_ENC_P_I_FIELD:     WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = P_I field frame\n"));   break;
        case VC1_ENC_P_P_FIELD:     WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = P_P field frame\n"));   break;
        case VC1_ENC_B_B_FIELD:     WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = B_B field frame\n"));   break;
        case VC1_ENC_B_BI_FIELD:    WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = B_BI field frame\n"));  break;
        case VC1_ENC_BI_B_FIELD:    WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = BI_B field frame\n"));  break;
        case VC1_ENC_BI_BI_FIELD:   WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = BI_BI field frame\n")); break;
        default:                    WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Picture type = %d\n"), FrameType);     break;
    }

    WriteParams(-1,VC1_ENC_POSITION, VM_STRING("Frame %d\n"), FrameCount);
    //WriteParams(-1,VC1_ENC_POSITION, "Frame %d = %d\n\n", FrameCount, FrameSize);
}

void VC1EncDebug::PrintFieldInfo(bool bSecondField, uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t t = 0;
    int32_t blk_num = 0;

    if(bSecondField && start == MBHeight/2)
    {
        //assert(0);
        WriteParams(-1,VC1_ENC_POSITION, VM_STRING("\t\t\tSecond Field \n"));
        XFieldPos = 0;
        YFieldPos = 0;
    }
    else
    {
        XFieldPos = 0;
        YFieldPos = start - MBHeight/2*bSecondField;
    }

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
            WriteParams(-1,VC1_ENC_POSITION, VM_STRING("\t\t\tX: %d, Y: %d\n"), XFieldPos, YFieldPos);

            //------MV information----------
            PrintMVFieldInfo();

            //----CBP---------------
            //if(!(pCurrMB->skip))
            {
                WriteParams(-1,VC1_ENC_CBP,VM_STRING("Read CBPCY: 0x%02X\n"), pCurrMB->CBP);
                //PrintBlkVTSInfo();
                WriteParams(-1,VC1_ENC_CBP,VM_STRING("Predicted CBPCY = 0x%02X\n"), pCurrMB->PredCBP);
            }


            //----quantization parameters
            {
                WriteParams(-1,VC1_ENC_QUANT, VM_STRING("MB Quant = %d\n"), pCurrMB->MQuant);
                WriteParams(-1,VC1_ENC_QUANT, VM_STRING("HalfQ = %d\n"), pCurrMB->HalfQP);
            }

            for(blk_num = 0; blk_num < 6; blk_num++)
            {
                if(pCurrMB->Block[blk_num].intra)
                {
                    //---BEGIN---DC coef information--------------
                    if(pCurrMB->Block[blk_num].ACPred!= -1)
                    {
                    if(pCurrMB->Block[blk_num].DCDirection == VC1_ENC_LEFT)
                        WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("DC left prediction\n"));
                    else
                        WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("DC top prediction\n"));
                    }

                    WriteParams(-1,VC1_ENC_COEFFS, VM_STRING("DC diff = %d\n"), pCurrMB->Block[blk_num].DCDiff);

                }

                //---BEGIN---run - level coefficients--------------
                PrintRunLevel(blk_num);

                if(pCurrMB->Block[blk_num].intra)
                {
                    //---BEGIN---AC coef information--------------
                    if((pCurrMB->Block[blk_num].ACPred) && (pCurrMB->Block[blk_num].ACPred!= -1))
                    {
                        WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("Block %d\n"), blk_num);
                        WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("AC prediction\n"));

                        for(t = 0; t < 7; t++)
                            WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("%d "),pCurrMB->Block[blk_num].AC[t]);

                        WriteParams(-1,VC1_ENC_AC_PRED,VM_STRING("\n"));
                    }

                    //---BEGIN---DC coef information--------------
                    WriteParams(-1,VC1_ENC_COEFFS, VM_STRING("DC = %d\n"), pCurrMB->Block[blk_num].DC);
                }

                //----Block difference---------------
                if(pCurrMB->Block[blk_num].intra || pCurrMB->CBP)
                    PrintBlockDifference(blk_num);
            }

            //------interpolation---------------
            PrintInterpolationInfoField();

            NextMB();
        }
    }

    if(DeblkFlag)
        PrintFieldDblkInfo(bSecondField, start, end);
}

void VC1EncDebug::PrintFrameInfo(uint32_t start, uint32_t end)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t t = 0;
    int32_t blk_num = 0;

    for(i = start; i < end; i++)
    {
        for(j = 0; j < MBWidth; j++)
        {
            WriteParams(-1,VC1_ENC_POSITION, VM_STRING("\t\t\tX: %d, Y: %d\n"), j, i);

            //------MV information----------
            PrintMVInfo();

            //----CBP---------------
            if(!(pCurrMB->skip))
            {
                WriteParams(-1,VC1_ENC_CBP,VM_STRING("Read CBPCY: 0x%02X\n"), pCurrMB->CBP);
                //PrintBlkVTSInfo();
                WriteParams(-1,VC1_ENC_CBP,VM_STRING("Predicted CBPCY = 0x%02X\n"), pCurrMB->PredCBP);
            }

            //----quantization parameters
            {
                WriteParams(-1,VC1_ENC_QUANT, VM_STRING("MB Quant = %d\n"), pCurrMB->MQuant);
                WriteParams(-1,VC1_ENC_QUANT, VM_STRING("HalfQ = %d\n"), pCurrMB->HalfQP);
            }

            for(blk_num = 0; blk_num < 6; blk_num++)
            {
                if(pCurrMB->Block[blk_num].intra)
                {
                    //---BEGIN---DC coef information--------------
                    if(pCurrMB->Block[blk_num].ACPred!= -1)
                    {
                    if(pCurrMB->Block[blk_num].DCDirection == VC1_ENC_LEFT)
                        WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("DC left prediction\n"));
                    else
                        WriteParams(-1,VC1_ENC_COEFFS|VC1_ENC_AC_PRED, VM_STRING("DC top prediction\n"));
                    }

                    WriteParams(-1,VC1_ENC_COEFFS, VM_STRING("DC diff = %d\n"), pCurrMB->Block[blk_num].DCDiff);

                }

                //---BEGIN---run - level coefficients--------------
                PrintRunLevel(blk_num);

                if(pCurrMB->Block[blk_num].intra)
                {
                    //---BEGIN---AC coef information--------------
                    if((pCurrMB->Block[blk_num].ACPred) && (pCurrMB->Block[blk_num].ACPred!= -1))
                    {
                        WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("Block %d\n"), blk_num);
                        WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("AC prediction\n"));

                        for(t = 0; t < 7; t++)
                            WriteParams(-1,VC1_ENC_AC_PRED, VM_STRING("%d "),pCurrMB->Block[blk_num].AC[t]);

                        WriteParams(-1,VC1_ENC_AC_PRED,VM_STRING("\n"));
                    }

                    //---BEGIN---DC coef information--------------
                    WriteParams(-1,VC1_ENC_COEFFS, VM_STRING("DC = %d\n"), pCurrMB->Block[blk_num].DC);
                }

                //----Block difference---------------
                if(pCurrMB->Block[blk_num].intra || pCurrMB->CBP)
                    PrintBlockDifference(blk_num);
            }

            //------interpolation---------------
            PrintInterpolationInfo();

            NextMB();
        }
    }

    if(DeblkFlag)
        PrintDblkInfo(start, end);
}

void VC1EncDebug::PrintFrameInfoSlices()
{
    uint32_t i = 0;

    for(i = 0; i < NumSlices; i++)
    {
        PrintFrameInfo(Slices[i][0], Slices[i][1]);
    }
}

void VC1EncDebug::PrintFieldInfoSlices(bool bSecondField)
{
    uint32_t i = 0;

    i = NumSlices/2 * bSecondField;

    for(i; i < NumSlices/2 + NumSlices/2*bSecondField; i++)
    {
        WriteParams(-1,VC1_ENC_POSITION, VM_STRING("\t\t\tSlice: %d\n"), i);
        PrintFieldInfo(bSecondField, Slices[i][0], Slices[i][1]);
    }
}

void VC1EncDebug::WriteFrameInfo()
{
    int32_t frameDebug = VC1_ENC_FRAME_DEBUG;

    if((!frameDebug) || (FrameCount >= VC1_ENC_FRAME_MIN) && (FrameCount <= VC1_ENC_FRAME_MAX))
    {
        PrintPictureType();

        pCurrMB = MBs;
        XPos = 0;
        YPos = 0;
        XFieldPos = 0;
        YFieldPos = 0;

        if(!FieldPic)
        {
            if(NumSlices < 2)
                PrintFrameInfo(0, MBHeight);
            else
                PrintFrameInfoSlices();
        }
        else
        {
            if(NumSlices <= 2)
            {
                PrintFieldInfo(false, 0, MBHeight/2);
                PrintFieldInfo(true, MBHeight/2, MBHeight);
            }
            else
            {
               PrintFieldInfoSlices(false);
               PrintFieldInfoSlices(true);
            }
        }
    }

    memset(MBs, 0, sizeof(VC1MBDebugInfo) * MBWidth * MBHeight);
    FrameCount++;
    RefNum = 0;
    NumSlices = 0;
    SetCurrMBFirst();
};


//static void PrintField(uint8_t* pY, int32_t Ystep, uint8_t* pU, int32_t Ustep, uint8_t* pV, int32_t Vstep, FILE* out, uint32_t Width, uint32_t Height)
//{
//    static uint32_t framenum = 0;
//
//   int32_t i = 0;
//   uint8_t* ptr = NULL;
//
//   for (i=0; i < Height; i++)
//    {
//        fwrite(pY+i*Ystep, 1, Width, out);
//
//    }
//    for (i=0; i < Height/2; i++)
//    {
//        fwrite(pU+i*Ustep, 1, Width/2, out);
//
//    }
//    for (i=0; i < Height/2; i++)
//    {
//        fwrite(pV+i*Vstep, 1, Width/2, out);
//
//    }
//
//    fflush(out);
//    framenum++;
//}

#endif
}
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

