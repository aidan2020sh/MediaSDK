/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _INTEL_PAVP_CORE_API_H
#define _INTEL_PAVP_CORE_API_H

#ifdef __cplusplus
extern "C" {
#endif


#include "intel_pavp_types.h"

/* 
**    GUID defining the Intel PAVP Device extension
**    See PAVP API/DDI design for more information
**    {7460004-7533-4E1A-B8E3-FF206BF5CE47}
*/
static const GUID DXVA2_Intel_Pavp = 
{ 0x7460004, 0x7533, 0x4e1a, { 0xbd, 0xe3, 0xff, 0x20, 0x6b, 0xf5, 0xce, 0x47 } };


#pragma pack(push)
#pragma pack(8)


typedef enum
{
    PAVP_MEMORY_PROTECTION_NONE     = 0,
    PAVP_MEMORY_PROTECTION_LITE     = 1,
    PAVP_MEMORY_PROTECTION_STATIC   = 2,
    PAVP_MEMORY_PROTECTION_DYNAMIC  = 4
} PAVP_MEMORY_PROTECTION_MASK;

//typedef enum
//{
//    PAVP_STATIC_MEMORY_0M       = 0,
//    PAVP_STATIC_MEMORY_96M      = 1,
//    PAVP_STATIC_MEMORY_128M     = 2
//} PAVP_STATIC_MEMORY_SIZE_MASK;

typedef enum
{
    PAVP_KEY_EXCHANGE_DEFAULT       = 0,
    PAVP_KEY_EXCHANGE_CANTIGA       = 1,
    PAVP_KEY_EXCHANGE_EAGLELAKE     = 2,
    PAVP_KEY_EXCHANGE_HEAVY         = 4,
    PAVP_KEY_EXCHANGE_IRONLAKE      = 8,
    PAVP_KEY_EXCHANGE_DAA           = 16
} PAVP_KEY_EXCHANGE_MASK;

typedef enum
{
    // Available session types
    PAVP_SESSION_TYPE_DECODE        = 1,
    PAVP_SESSION_TYPE_TRANSCODE     = 2,
} PAVP_SESSION_TYPE;

typedef struct tagPAVP_QUERY_CAPS
{
    UINT AvailableMemoryProtection;     // Indicates supported protection modes
    UINT AvailableKeyExchangeProtocols; // Indicates supported key exchange method
    UINT Reserved1;
    PAVP_MEMORY_PROTECTION_MASK     eCurrentMemoryMode; // only 1 bit set
    UINT Reserved2;
} PAVP_QUERY_CAPS;

typedef struct tagPAVP_QUERY_CAPS2
{
    UINT AvailableMemoryProtection;     // Indicates supported protection modes
    UINT AvailableKeyExchangeProtocols; // Indicates supported key exchange method
    UINT Reserved1;
    PAVP_MEMORY_PROTECTION_MASK     eCurrentMemoryMode; // only 1 bit set
    UINT Reserved2;
    UINT AvailableSessionTypes;         // Indicates decode / transcode
    UINT AvailableEncryptionTypes;      // Indicates none/CTR/CBC AES types
    UINT AvailableCounterTypes;         // Indicates available HW counter bits
} PAVP_QUERY_CAPS2;

// Decoder Extension Function Codes
typedef enum tagPAVP_FUNCTION_ID
{
    PAVP_KEY_EXCHANGE          = 0x200,
//    PAVP_USE_PROTECTED_MEMORY  = 0x201,
//    PAVP_GET_CONNECTION_STATE  = 0x202,
    PAVP_GET_FRESHNESS         = 0x203,
    PAVP_SET_FRESHNESS         = 0x204,
    PAVP_SET_WINDOW_POSITION   = 0x205,
//    PAVP_SET_MEMORY_MODE       = 0x206,
//    PAVP_SET_PLANE_ENABLE      = 0x208,
    PAVP_GET_FRESHNESS_ENCRYPT = 0x209,
    PAVP_SET_FRESHNESS_ENCRYPT = 0x20A,
    PAVP_SET_STREAM_KEY        = 0x20B,
    PAVP_ME_PASS_THROUGH       = 0x20C
} PAVP_FUNCTION_ID;

// Device creation parameters
typedef struct tagPAVP_CREATE_DEVICE
{
    PAVP_KEY_EXCHANGE_MASK    eDesiredKeyExchangeMode;  // Only one bit should be set
} PAVP_CREATE_DEVICE, *PPAVP_CREATE_DEVICE;

typedef struct tagPAVP_CREATE_DEVICE2
{
    DWORD                   eDesiredKeyExchangeMode : 16;  // Only one bit should be set
    DWORD                   PavpVersion             :  8;  // For version 0, struct is just 32-bits of PAVP_KEY_EXCHANGE_MASK
    DWORD                   ePavpSessionType        :  8;  // Only one bit should be set
} PAVP_CREATE_DEVICE2, *PPAVP_CREATE_DEVICE2;

// Fixed key exchange parameters
typedef struct tagPAVP_FIXED_EXCHANGE_PARAMS
{
    DWORD   FixedKey[4];
    DWORD   SessionKey[4];
} PAVP_FIXED_EXCHANGE_PARAMS;

//// Use PAVP protected memory during allocation struct
//typedef struct tagPAVP_USE_PROTECTED_MEMORY_PARAMS
//{
//    BOOL    bUseProtectedMemory;
//} PAVP_USE_PROTECTED_MEMORY_PARAMS;

// PAVP get freshness struct
typedef struct tagPAVP_GET_FRESHNESS_PARAMS
{
    DWORD   Freshness[4];
} PAVP_GET_FRESHNESS_PARAMS;

#if defined(_WINDEF_)
typedef struct tagPAVP_SET_WINDOW_POSITION_PARAMS
{
    RECT WindowPosition;
    RECT VisibleContent;
    HDC  hdcMonitorId;
} PAVP_SET_WINDOW_POSITION_PARAMS;
#endif

typedef enum
{
    PAVP_SET_KEY_DECRYPT    = 0, // Update Sn_d
    PAVP_SET_KEY_ENCRYPT    = 1, // Update Sn_e
    PAVP_SET_KEY_BOTH       = 2, // Update both Sn_d and Sn_e
    PAVP_SET_KEY_WYSIWYS    = 4, // Enable heavy mode and update Sn_d (bitmap)
} PAVP_SET_KEY_TYPE;

typedef struct tagPAVP_SET_STREAM_KEY_PARAMS
{
    PAVP_SET_KEY_TYPE   StreamType;
    DWORD               EncryptedDecryptKey[4];
    DWORD               EncryptedEncryptKey[4];
} PAVP_SET_STREAM_KEY_PARAMS;

typedef struct tagPAVP_ENCRYPTION_MODE
{
    PAVP_ENCRYPTION_TYPE    eEncryptionType;
    PAVP_COUNTER_TYPE       eCounterMode;
} PAVP_ENCRYPTION_MODE;

#if defined(__DIRECTX_VA__)
typedef struct _DXVA_Intel_Pavp_Protocol
{
    DXVA_EncryptProtocolHeader  EncryptProtocolHeader;
    DWORD                       dwBufferSize;
    DWORD                       dwAesCounter;
} DXVA_Intel_Pavp_Protocol, *PDXVA_Intel_Pavp_Protocol;

typedef struct _DXVA_Intel_Pavp_Protocol2
{
    DXVA_EncryptProtocolHeader  EncryptProtocolHeader;
    DWORD                       dwBufferSize;
    DWORD                       dwAesCounter[4];
    PAVP_ENCRYPTION_MODE        PavpEncryptionMode;
} DXVA_Intel_Pavp_Protocol2, *PDXVA_Intel_Pavp_Protocol2;
#endif


#pragma pack(pop)


#ifdef __cplusplus
}
#endif

#endif//_INTEL_PAVP_CORE_API_H