/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_UTILS_DRM_H__
#define __VAAPI_UTILS_DRM_H__

#if defined(LIBVA_DRM_SUPPORT)

#include <va/va_drm.h>
#include "vaapi_utils.h"
#include <stdlib.h>
#include <string.h>

class DRMLibVA : public CLibVA
{
public:
    DRMLibVA(const std::string& devicePath = "", int type = MFX_LIBVA_DRM);
    virtual ~DRMLibVA(void);

protected:
    int m_fd;
    MfxLoader::VA_DRMProxy m_vadrmlib;

private:
    DISALLOW_COPY_AND_ASSIGN(DRMLibVA);
};

#endif // #if defined(LIBVA_DRM_SUPPORT)

#endif // #ifndef __VAAPI_UTILS_DRM_H__
