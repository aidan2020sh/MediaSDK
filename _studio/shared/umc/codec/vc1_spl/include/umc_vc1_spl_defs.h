// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_SPLITTER)

#ifndef __UMC_VC1_SPL_DEFS_H__
#define __UMC_VC1_SPL_DEFS_H__


//typedef enum
//{
//    VC1_EndOfSequence              = 0x0A,
//    VC1_Slice                      = 0x0B,
//    VC1_Field                      = 0x0C,
//    VC1_FrameHeader                = 0x0D,
//    VC1_EntryPointHeader           = 0x0E,
//    VC1_SequenceHeader             = 0x0F,
//    VC1_SliceLevelUserData         = 0x1B,
//    VC1_FieldLevelUserData         = 0x1C,
//    VC1_FrameLevelUserData         = 0x1D,
//    VC1_EntryPointLevelUserData    = 0x1E,
//    VC1_SequenceLevelUserData      = 0x1F
//} VC1StartCode;

//enum //profile definitions
//{
//    VC1_PROFILE_SIMPLE = 0, //disables X8 Intraframe, Loop filter, DQuant, and
//                            //Multires while enabling the Fast Transform
//    VC1_PROFILE_MAIN   = 1, //The main profile is has all the simple profile
//                            //tools plus loop filter, dquant, and multires
//    VC1_PROFILE_RESERVED = 2,
//    VC1_PROFILE_ADVANCED= 3    //The complex profile has X8 Intraframe can use
//                      //the normal IDCT transform or the VC1 Inverse Transform
//};

//typedef enum
//{
//    I_FRAME              = 0,
//    P_FRAME              = 1,
//    B_FRAME              = 2,
//    BI_FRAME             = 3,
//    SKIPPED_FRAME        = 4,
//    I_I_FIELD_FRAME      = 5,
//    I_P_FIELD_FRAME      = 6,
//    P_I_FIELD_FRAME      = 7,
//    P_P_FIELD_FRAME      = 8,
//    B_B_FIELD_FRAME      = 9,
//    B_BI_FIELD_FRAME     = 10,
//    BI_B_FIELD_FRAME     = 10,
//    BI_BI_FIELD_FRAME    = 11
//}VC1FrameType;

////frame coding mode
//enum
//{
//    Progressive           = 0,
//    FrameInterlace        = 1,
//    FieldInterlace        = 2
//};

#endif //__UMC_VC1_SPL_DEFS_H__
#endif //UMC_ENABLE_VC1_SPLITTER
