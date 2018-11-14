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

#include "umc_vc1_enc_smoothing_adv.h"
#include "umc_vc1_enc_smoothing_sm.h"

namespace UMC_VC1_ENCODER
{
    extern fSmooth_I_MB_Adv Smooth_I_MBFunction_Adv_YV12[8] = 
    { 
        NoSmoothMB_I_Adv,            //right/top/left   000
        Smoth_TopRightMB_I_Adv_YV12, //right/top        001
        NoSmoothMB_I_Adv,            //right, left      010
        Smoth_RightMB_I_Adv_YV12,    //right            011
        Smooth_LeftMB_I_Adv_YV12,    //top/left         100
        Smoth_TopMB_I_Adv_YV12,      //top              101
        Smooth_LeftMB_I_Adv_YV12,    //left             110
        Smoth_MB_I_Adv_YV12          //common           111
    };

    extern fSmooth_I_MB_Adv Smooth_I_MBFunction_Adv_NV12[8] = 
    { 
        NoSmoothMB_I_Adv,            //right/top/left   000
        Smoth_TopRightMB_I_Adv_NV12, //right/top        001
        NoSmoothMB_I_Adv,            //right, left      010
        Smoth_RightMB_I_Adv_NV12,    //right            011
        Smooth_LeftMB_I_Adv_NV12,    //top/left         100
        Smoth_TopMB_I_Adv_NV12,      //top              101
        Smooth_LeftMB_I_Adv_NV12,    //left             110
        Smoth_MB_I_Adv_NV12          //common           111
    };

    extern fSmooth_P_MB_Adv Smooth_P_MBFunction_Adv_YV12[8] = 
    { 
        NoSmoothMB_P_Adv,            //right/top/left   000
        Smoth_TopRightMB_P_Adv_YV12, //right/top        001
        NoSmoothMB_P_Adv,            //right, left      010
        Smoth_RightMB_P_Adv_YV12,    //right            011
        Smooth_LeftMB_P_Adv_YV12,    //top/left         100
        Smoth_TopMB_P_Adv_YV12,      //top              101
        Smooth_LeftMB_P_Adv_YV12,    //left             110
        Smoth_MB_P_Adv_YV12          //common           111
    };

    extern fSmooth_P_MB_Adv Smooth_P_MBFunction_Adv_NV12[8] = 
    { 
        NoSmoothMB_P_Adv,            //right/top/left   000
        Smoth_TopRightMB_P_Adv_NV12, //right/top        001
        NoSmoothMB_P_Adv,            //right, left      010
        Smoth_RightMB_P_Adv_NV12,    //right            011
        Smooth_LeftMB_P_Adv_NV12,    //top/left         100
        Smoth_TopMB_P_Adv_NV12,      //top              101
        Smooth_LeftMB_P_Adv_NV12,    //left             110
        Smoth_MB_P_Adv_NV12          //common           111
    };

    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_YV12[8] = 
    { 
        NoSmoothMB_I_SM,            //right/top/left   000
        Smoth_TopRightMB_I_SM_YV12, //right/top        001
        NoSmoothMB_I_SM,            //right, left      010
        Smoth_RightMB_I_SM_YV12,    //right            011
        Smooth_LeftMB_I_SM_YV12,    //top/left         100
        Smoth_TopMB_I_SM_YV12,      //top              101
        Smooth_LeftMB_I_SM_YV12,    //left             110
        Smoth_MB_I_SM_YV12          //common           111
    };

    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_NV12[8] = 
    { 
        NoSmoothMB_I_SM,            //right/top/left   000
        Smoth_TopRightMB_I_SM_NV12, //right/top        001
        NoSmoothMB_I_SM,            //right, left      010
        Smoth_RightMB_I_SM_NV12,    //right            011
        Smooth_LeftMB_I_SM_NV12,    //top/left         100
        Smoth_TopMB_I_SM_NV12,      //top              101
        Smooth_LeftMB_I_SM_NV12,    //left             110
        Smoth_MB_I_SM_NV12          //common           111
    };

    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_YV12[8] = 
    { 
        NoSmoothMB_P_SM,            //right/top/left   000
        Smoth_TopRightMB_P_SM_YV12, //right/top        001
        NoSmoothMB_P_SM,            //right, left      010
        Smoth_RightMB_P_SM_YV12,    //right            011
        Smooth_LeftMB_P_SM_YV12,    //top/left         100
        Smoth_TopMB_P_SM_YV12,      //top              101
        Smooth_LeftMB_P_SM_YV12,    //left             110
        Smoth_MB_P_SM_YV12          //common           111
    };

    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_NV12[8] = 
    { 
        NoSmoothMB_P_SM,            //right/top/left   000
        Smoth_TopRightMB_P_SM_NV12, //right/top        001
        NoSmoothMB_P_SM,            //right, left      010
        Smoth_RightMB_P_SM_NV12,    //right            011
        Smooth_LeftMB_P_SM_NV12,    //top/left         100
        Smoth_TopMB_P_SM_NV12,      //top              101
        Smooth_LeftMB_P_SM_NV12,    //left             110
        Smoth_MB_P_SM_NV12          //common           111
    };
}

#endif
