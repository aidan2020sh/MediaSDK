/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Encrypt tables for Rijndael
//
// Contents:
//    RijEncSbox[256]
//
//
//    Created: Tue 23-May-2002 10:35
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcprij.h"
#include "pcprijtables.h"


/*
// Reference to pcrijencryptpxca.c
// for details
*/

/*
// Pure Encryprion S-boxes
*/
#if (defined(_PX) && defined(_MERGED_BLD)) || (!defined(_MERGED_BLD) && !defined(_IPP_TRS))

const Ipp8u RijEncSbox[256] = {
   ENC_SBOX(none_t)
};

#endif /* _MERGED_BLD */

