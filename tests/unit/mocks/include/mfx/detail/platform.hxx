// Copyright (c) 2019 Intel Corporation
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

#pragma once

namespace mocks { namespace mfx
{
    namespace detail
    {
        enum product_family
        {
            IGFX_UNKNOWN = 0,
            IGFX_GRANTSDALE_G,
            IGFX_ALVISO_G,
            IGFX_LAKEPORT_G,
            IGFX_CALISTOGA_G,
            IGFX_BROADWATER_G,
            IGFX_CRESTLINE_G,
            IGFX_BEARLAKE_G,
            IGFX_CANTIGA_G,
            IGFX_CEDARVIEW_G,
            IGFX_EAGLELAKE_G,
            IGFX_IRONLAKE_G,
            IGFX_GT,
            IGFX_IVYBRIDGE,
            IGFX_HASWELL,
            IGFX_VALLEYVIEW,
            IGFX_BROADWELL,
            IGFX_CHERRYVIEW,
            IGFX_SKYLAKE,
            IGFX_KABYLAKE,
            IGFX_COFFEELAKE,
            IGFX_WILLOWVIEW,
            IGFX_APOLLOLAKE,
            IGFX_GEMINILAKE,
            IGFX_GLENVIEW,
            IGFX_GOLDWATERLAKE,
            IGFX_CANNONLAKE,
            IGFX_CNX_G,
            IGFX_ICELAKE,
            IGFX_ICELAKE_LP,
            IGFX_LAKEFIELD,
            IGFX_JASPERLAKE,
            IGFX_LAKEFIELD_R,
            IGFX_TIGERLAKE_LP,
            IGFX_RYEFIELD,
            IGFX_ROCKETLAKE,
            IGFX_ALDERLAKE_S,
            IGFX_ALDERLAKE_P,

            IGFX_SOFIA_LTE1 = 1001,
            IGFX_SOFIA_LTE2 = 1002,

            IGFX_DG1        = 1210,
            IGFX_XE_HP      = 1250,
            IGFX_DG2        = 1270,
            IGFX_PVC        = 1271,
            IGFX_METEORLAKE = 1272,
            IGFX_ARROWLAKE  = 1273,
            IGFX_ELASTICG   = 1274,

            IGFX_GENNEXT               = 0x7ffffffe,

            PRODUCT_FAMILY_FORCE_ULONG = 0x7fffffff
        };
    }

    enum HW_type
    {
        HW_UNKNOWN   = 0,
        HW_SNB       = 0x300000,

        HW_IVB       = 0x400000,

        HW_HSW       = 0x500000,
        HW_HSW_ULT   = 0x500001,

        HW_VLV       = 0x600000,

        HW_BDW       = 0x700000,

        HW_CHT       = 0x800000,

        HW_SCL       = 0x900000,

        HW_APL       = 0x1000000,

        HW_KBL       = 0x1100000,
        HW_GLK       = HW_KBL + 1,
        HW_CFL       = HW_KBL + 2,

        HW_CNL       = 0x1200000,

        HW_ICL       = 0x1400000,
        HW_ICL_LP    = HW_ICL + 1,
        HW_CNX_G     = HW_ICL + 2,
        HW_LKF       = 0x1500000,
        HW_JSL       = 0x1500001, //HW_LKF + 1
        HW_EHL       = 0x1500002,

        HW_TGL_LP    = 0x1600000,
        HW_RKL       = HW_TGL_LP + 2,
        HW_DG1       = HW_TGL_LP + 3,
        HW_ADL_S     = HW_TGL_LP + 4,
        HW_ADL_P     = HW_TGL_LP + 5,

        HW_RYF       = HW_TGL_LP + 1,
        HW_XE_HP_SDV = HW_TGL_LP + 6,
        HW_DG2       = HW_XE_HP_SDV + 1,
        HW_PVC       = HW_XE_HP_SDV + 2,

        HW_MTL       = 0x1700000,
        HW_ARL       = HW_MTL + 1,
        HW_ELG       = HW_MTL + 2
    };

} }
