/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 Intel Corporation. All Rights Reserved.
//
*/

#define PRODUCT_NAME "Intel\xae Media SDK 2013"
#define FILE_VERSION 1,0,0,0
#define FILE_VERSION_STRING "1,0,0,0"
#define FILTER_NAME_PREFIX ""
#define FILTER_NAME_SUFFIX ""
#define PRODUCT_COPYRIGHT "Copyright\xa9 2003-2013 Intel Corporation"
#define PRODUCT_VERSION 1,0,0,0
#define PRODUCT_VERSION_STRING "1,0,0,0"

#define PE_NAME_STRING "Intel Media SDK Tracer"
#ifdef _WIN64
    #define DLL_NAME           "tracer_core64.dll"
#else 
    #define DLL_NAME           "tracer_core32.dll"
#endif
#define PE_NAME        _T(PE_NAME_STRING)
