/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __MFX_PLATFORM_DEFS_H__
#define __MFX_PLATFORM_DEFS_H__

#include <va/va.h>

typedef int D3DFORMAT;

#ifndef D3DDDIFORMAT
#define D3DDDIFORMAT        D3DFORMAT
#endif

typedef int             BOOL;
typedef char            CHAR;
typedef unsigned char   BYTE;
typedef short           SHORT;
typedef int             INT;


typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned int   UINT;

typedef int            LONG;
typedef unsigned int   ULONG;
typedef unsigned int   DWORD;

#endif // __MFX_PLATFORM_DEFS_H__
