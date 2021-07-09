/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2021 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

enum HWType
{
    MFX_HW_UNKNOWN   = 0,
    MFX_HW_LAKE      = 0x100000,
    MFX_HW_LRB       = 0x200000,
    MFX_HW_SNB       = 0x300000,

    MFX_HW_IVB       = 0x400000,

    MFX_HW_HSW       = 0x500000,
    MFX_HW_HSW_ULT   = 0x500001,

    MFX_HW_VLV       = 0x600000,

    MFX_HW_BDW       = 0x700000,

    MFX_HW_CHV       = 0x800000,

    MFX_HW_SKL       = 0x900000,

    MFX_HW_APL       = 0xa00000,

    MFX_HW_KBL       = 0xb00000,
    MFX_HW_GLK       = MFX_HW_KBL + 1,
    MFX_HW_CFL       = MFX_HW_KBL + 2,

    MFX_HW_CNL       = 0xc00000,

    MFX_HW_ICL       = 0xd00000,

    MFX_HW_LKF       = MFX_HW_ICL + 10,
    MFX_HW_JSL       = MFX_HW_LKF + 1,

    MFX_HW_TGL       = 0xe00000,

    MFX_HW_RKL       = MFX_HW_TGL + 2,
    MFX_HW_DG1       = MFX_HW_TGL + 3,
    MFX_HW_ADL_S     = MFX_HW_TGL + 4,
    MFX_HW_ADL_P     = MFX_HW_TGL + 5,
    MFX_HW_XE_HP_SDV     = MFX_HW_TGL + 6,
    MFX_HW_DG2       = MFX_HW_XE_HP_SDV + 1,
    MFX_HW_PVC       = MFX_HW_XE_HP_SDV + 2,

    MFX_HW_MTL       = 0xf00000,
    MFX_HW_ARL       = MFX_HW_MTL + 1
    MFX_HW_ELG       = MFX_HW_MTL + 2
};

enum OSFamily
{
    MFX_OS_FAMILY_UNKNOWN = 0,
    MFX_OS_FAMILY_WINDOWS,
    MFX_OS_FAMILY_LINUX
};

enum OSWinVersion
{
    MFX_WIN_VER_UNKNOWN = 0,
    MFX_WIN_VER_W7,
    MFX_WIN_VER_W81,
    MFX_WIN_VER_WS2012R2,
    MFX_WIN_VER_W10,
    MFX_WIN_VER_WS2016
};
