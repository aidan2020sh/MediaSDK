/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: libmfx_core_d3d9.h

\* ****************************************************************************** */
#include "mfx_common.h"

#if defined(MFX_VA)

#ifndef __LIBMFX_CORE__HW_H__
#define __LIBMFX_CORE__HW_H__

#include "umc_va_base.h"

mfxU32 ChooseProfile(mfxVideoParam * param, eMFXHWType hwType);
UMC::VideoAccelerationHW ConvertMFXToUMCType(eMFXHWType type);
bool IsHwMvcEncSupported();

#endif

#endif

