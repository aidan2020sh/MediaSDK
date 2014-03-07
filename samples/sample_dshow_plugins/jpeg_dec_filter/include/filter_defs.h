/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/

#include "..\..\..\sample_common\include\current_date.h"

#define FILTER_NAME_STRING "Intel\xae Media SDK JPEG Decoder"
#define DLL_NAME           "jpeg_dec_filter.dll"
#define FILTER_NAME        _T(FILTER_NAME_STRING)
#define FILTER_GUID        CLSID_JPEGDecFilter
#define FILTER_CREATE      CJPEGDecVideoFilter::CreateInstance
#define FILTER_INPUT       { &MEDIATYPE_Video, &MEDIASUBTYPE_MJPG }
#define FILTER_OUTPUT      { &MEDIATYPE_Video, &MEDIASUBTYPE_NV12 }

#define FILTER_PROPERTIES2 { L"Video Decoder Properties", &CLSID_VideoPropertyPage, CVideoDecPropPage::CreateInstance, NULL, NULL }
#define FILTER_PROPERTIES3 { L"About", &CLSID_AboutPropertyPage, CAboutPropPage::CreateInstance, NULL, NULL }

TCHAR * GetCodecRegistryPath()    {return _T(REGISTRY_PATH_PREFIX) _T("JPEG Decoder");}