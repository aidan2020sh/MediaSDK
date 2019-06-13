/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#include "utest_common.h"
#include "mfx_pipeline_dec.h"
#include "mfx_decode_pipeline_config.h"
#include "mock_mfx_vpp.h"
#include "mock_mfx_pipeline_config.h"
#include "mock_mfx_pipeline_factory.h"
#include "mock_mfx_ivideo_session.h"
#include "mfx_test_utils.h"

const mfxF64 tolerance = 2e-6;
// input data
mfxU8  video1_20x20_3_nv12[] = {0x33, 0x39, 0x3c, 0x3b, 0x38, 0x35, 0x38, 0x38, 0x42, 0x4c, 0x3c, 0x3a,  0x2f, 0x33, 0x32, 0x36, 0x38, 0x38, 0x38, 0x31, 0x32, 0x36, 0x36, 0x36,  0x32, 0x2c, 0x2b, 0x29, 0x2a, 0x28, 0x29, 0x28, 0x29, 0x29, 0x29, 0x28,  0x29, 0x28, 0x28, 0x2a, 0x2a, 0x2c, 0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x29,  0x28, 0x28, 0x29, 0x29, 0x28, 0x28, 0x2a, 0x2b, 0x2a, 0x2a, 0x29, 0x27,  0x25, 0x23, 0x22, 0x23, 0x22, 0x21, 0x20, 0x21, 0x2a, 0x47, 0x36, 0x26,  0x5b, 0x46, 0x29, 0x2e, 0x2e, 0x2c, 0x2c, 0x2b, 0x2b, 0x2a, 0x2a, 0x29,  0x28, 0x29, 0x2a, 0x2a, 0x2b, 0x2d, 0x2d, 0x2c, 0x2a, 0x2c, 0x2e, 0x2c,  0x2d, 0x2a, 0x2b, 0x2d, 0x2d, 0x2e, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x31,  0x2f, 0x2e, 0x2f, 0x2e, 0x30, 0x2f, 0x30, 0x32, 0x30, 0x30, 0x30, 0x31,  0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x2c, 0x2a, 0x27, 0x25, 0x24,  0x24, 0x26, 0x26, 0x2a, 0x53, 0x47, 0x45, 0x3f, 0x36, 0x34, 0x34, 0x32,  0x32, 0x31, 0x2c, 0x2d, 0x2e, 0x25, 0x2a, 0x2c, 0x2f, 0x2f, 0x32, 0x38,  0x44, 0x36, 0x37, 0x36, 0x3f, 0x44, 0x48, 0x4c, 0x5f, 0x59, 0x54, 0x45,  0x39, 0x36, 0x39, 0x36, 0x38, 0x3e, 0x3e, 0x3b, 0x31, 0x34, 0x35, 0x33,  0x34, 0x34, 0x39, 0x3e, 0x41, 0x35, 0x2e, 0x36, 0x2e, 0x35, 0x33, 0x2e,  0x2b, 0x2b, 0x31, 0x2f, 0x31, 0x35, 0x35, 0x36, 0x31, 0x2c, 0x2b, 0x2a,  0x2a, 0x29, 0x29, 0x2a, 0x2a, 0x2a, 0x2b, 0x2a, 0x29, 0x2a, 0x29, 0x2a,  0x2b, 0x2d, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,  0x29, 0x2a, 0x2b, 0x2b, 0x2b, 0x2a, 0x29, 0x27, 0x25, 0x24, 0x23, 0x21,  0x21, 0x21, 0x22, 0x22, 0x2e, 0x4b, 0x33, 0x2a, 0x5b, 0x41, 0x2b, 0x2e,  0x2e, 0x2c, 0x2a, 0x2a, 0x2a, 0x29, 0x28, 0x29, 0x29, 0x2a, 0x2b, 0x2c,  0x2d, 0x2e, 0x2e, 0x2d, 0x2b, 0x2c, 0x2d, 0x2c, 0x2b, 0x2a, 0x2b, 0x2d,  0x2c, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2c, 0x2c, 0x2d,  0x2e, 0x2e, 0x2f, 0x30, 0x2e, 0x2f, 0x2e, 0x2e, 0x30, 0x30, 0x31, 0x31,  0x31, 0x31, 0x2f, 0x2a, 0x28, 0x25, 0x24, 0x24, 0x23, 0x24, 0x25, 0x2d,  0x51, 0x3f, 0x3a, 0x35, 0x32, 0x30, 0x30, 0x30, 0x2f, 0x2e, 0x38, 0x47,  0x59, 0x33, 0x2d, 0x2d, 0x2e, 0x2f, 0x2e, 0x33, 0x3c, 0x2a, 0x30, 0x38,  0x40, 0x42, 0x3f, 0x36, 0x58, 0x4b, 0x45, 0x41, 0x5c, 0x40, 0x3d, 0x35,  0x38, 0x3c, 0x3c, 0x37, 0x2e, 0x29, 0x24, 0x27, 0x2f, 0x30, 0x32, 0x33,  0x39, 0x32, 0x2f, 0x35, 0x2d, 0x32, 0x33, 0x2a, 0x28, 0x2d, 0x30, 0x2e,  0x2f, 0x38, 0x39, 0x39, 0x31, 0x2c, 0x2c, 0x2c, 0x2a, 0x2a, 0x2b, 0x2b,  0x2b, 0x2b, 0x2c, 0x2a, 0x2b, 0x2b, 0x2a, 0x2b, 0x2c, 0x2e, 0x2d, 0x2c,  0x2c, 0x2c, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2c, 0x2d, 0x2d, 0x2d,  0x2d, 0x2d, 0x2b, 0x29, 0x27, 0x26, 0x24, 0x23, 0x22, 0x22, 0x22, 0x23,  0x31, 0x4d, 0x33, 0x2e, 0x5d, 0x40, 0x2f, 0x31, 0x30, 0x2e, 0x2b, 0x29,  0x2a, 0x2a, 0x29, 0x2a, 0x2a, 0x2c, 0x2d, 0x2e, 0x30, 0x2f, 0x30, 0x2f,  0x2f, 0x2e, 0x2f, 0x2e, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x2f, 0x2f, 0x31,  0x31, 0x31, 0x31, 0x31, 0x30, 0x2e, 0x2e, 0x30, 0x2f, 0x30, 0x31, 0x31,  0x31, 0x30, 0x2f, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x30, 0x2b,  0x29, 0x26, 0x26, 0x25, 0x23, 0x24, 0x25, 0x30, 0x52, 0x37, 0x33, 0x31,  0x31, 0x30, 0x31, 0x31, 0x30, 0x30, 0x43, 0x57, 0x6c, 0x42, 0x2c, 0x2f,  0x2e, 0x30, 0x2e, 0x30, 0x35, 0x28, 0x24, 0x35, 0x40, 0x3d, 0x3a, 0x31,  0x39, 0x3f, 0x42, 0x4c, 0x6b, 0x47, 0x3e, 0x3b, 0x3b, 0x3b, 0x3d, 0x3b,  0x28, 0x21, 0x25, 0x31, 0x4f, 0x5a, 0x46, 0x31, 0x35, 0x36, 0x34, 0x2e,  0x27, 0x27, 0x2d, 0x2b, 0x28, 0x2a, 0x2c, 0x2f, 0x39, 0x44, 0x44, 0x40,  0x38, 0x2a, 0x28, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b,  0x2a, 0x2c, 0x2b, 0x2c, 0x2d, 0x2d, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2c, 0x2c, 0x2c, 0x2b, 0x2c, 0x2d, 0x2d, 0x2c, 0x2d, 0x2e, 0x2b, 0x2a,  0x28, 0x26, 0x25, 0x24, 0x23, 0x22, 0x22, 0x22, 0x2f, 0x4c, 0x33, 0x2d,  0x5c, 0x3e, 0x2d, 0x2f, 0x2f, 0x2e, 0x2a, 0x29, 0x29, 0x29, 0x2a, 0x2b,  0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x31, 0x30, 0x30, 0x2f, 0x30, 0x2d,  0x2c, 0x2c, 0x2e, 0x2f, 0x2f, 0x30, 0x30, 0x31, 0x32, 0x32, 0x31, 0x32,  0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x30, 0x31, 0x31, 0x31, 0x30, 0x31, 0x32,  0x32, 0x32, 0x33, 0x34, 0x34, 0x35, 0x33, 0x2c, 0x2a, 0x27, 0x26, 0x25,  0x25, 0x24, 0x24, 0x2f, 0x50, 0x36, 0x31, 0x2f, 0x30, 0x31, 0x31, 0x2f,  0x35, 0x3d, 0x40, 0x40, 0x4d, 0x4a, 0x2d, 0x2c, 0x2e, 0x30, 0x2f, 0x31,  0x33, 0x29, 0x24, 0x32, 0x38, 0x34, 0x35, 0x2e, 0x34, 0x3a, 0x53, 0x62,  0x6f, 0x58, 0x34, 0x34, 0x31, 0x3a, 0x38, 0x34, 0x27, 0x20, 0x2b, 0x31,  0x31, 0x41, 0x37, 0x32, 0x37, 0x37, 0x2d, 0x25, 0x24, 0x25, 0x29, 0x28,  0x26, 0x28, 0x2e, 0x36, 0x46, 0x46, 0x46, 0x3f, 0x42, 0x3e, 0x2b, 0x29,  0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2d,  0x2d, 0x2d, 0x2d, 0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2a, 0x2b, 0x2b, 0x2a,  0x2b, 0x2b, 0x2c, 0x2d, 0x2d, 0x2d, 0x2b, 0x29, 0x29, 0x27, 0x25, 0x23,  0x23, 0x23, 0x23, 0x22, 0x2f, 0x4e, 0x32, 0x2a, 0x50, 0x39, 0x2b, 0x2d,  0x2b, 0x2c, 0x2a, 0x29, 0x28, 0x2a, 0x29, 0x2b, 0x2d, 0x2f, 0x30, 0x31,  0x31, 0x30, 0x2f, 0x30, 0x2f, 0x2e, 0x30, 0x2e, 0x2d, 0x2c, 0x2e, 0x2f,  0x2f, 0x2f, 0x30, 0x31, 0x32, 0x32, 0x32, 0x32, 0x31, 0x2f, 0x30, 0x30,  0x30, 0x30, 0x32, 0x32, 0x32, 0x31, 0x32, 0x33, 0x35, 0x32, 0x34, 0x35,  0x34, 0x35, 0x33, 0x2c, 0x29, 0x28, 0x27, 0x26, 0x24, 0x24, 0x23, 0x31,  0x4c, 0x37, 0x38, 0x37, 0x33, 0x31, 0x34, 0x39, 0x3f, 0x3d, 0x3e, 0x3c,  0x33, 0x40, 0x39, 0x2f, 0x30, 0x30, 0x2e, 0x32, 0x33, 0x3a, 0x3f, 0x36,  0x33, 0x3e, 0x3e, 0x41, 0x3a, 0x32, 0x4f, 0x5b, 0x5f, 0x57, 0x3a, 0x25,  0x39, 0x3e, 0x35, 0x30, 0x27, 0x22, 0x34, 0x3a, 0x21, 0x26, 0x36, 0x39,  0x38, 0x38, 0x33, 0x2a, 0x29, 0x2f, 0x29, 0x29, 0x2b, 0x26, 0x23, 0x23,  0x2a, 0x38, 0x41, 0x3b, 0x42, 0x4e, 0x40, 0x2d, 0x2a, 0x2a, 0x2b, 0x2b,  0x2a, 0x2b, 0x2c, 0x2a, 0x2b, 0x2a, 0x2a, 0x2c, 0x2d, 0x2d, 0x2d, 0x2c,  0x2c, 0x2c, 0x2b, 0x2b, 0x2c, 0x2b, 0x2d, 0x2a, 0x2b, 0x2a, 0x2c, 0x2d,  0x2d, 0x2d, 0x2b, 0x29, 0x28, 0x26, 0x25, 0x23, 0x23, 0x23, 0x22, 0x22,  0x2e, 0x4e, 0x33, 0x26, 0x41, 0x30, 0x2a, 0x2c, 0x2b, 0x2b, 0x2a, 0x29,  0x28, 0x29, 0x29, 0x2b, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x30, 0x2f, 0x2e,  0x2e, 0x2d, 0x2e, 0x2d, 0x2d, 0x2c, 0x2c, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f,  0x30, 0x30, 0x2f, 0x2f, 0x30, 0x2e, 0x2f, 0x30, 0x31, 0x31, 0x33, 0x33,  0x32, 0x30, 0x32, 0x32, 0x34, 0x32, 0x33, 0x33, 0x34, 0x33, 0x32, 0x2c,  0x2a, 0x28, 0x27, 0x25, 0x23, 0x24, 0x22, 0x30, 0x49, 0x3c, 0x44, 0x3d,  0x38, 0x3c, 0x40, 0x42, 0x3b, 0x2f, 0x2e, 0x2a, 0x3c, 0x39, 0x2a, 0x2d,  0x2e, 0x30, 0x2f, 0x37, 0x39, 0x3d, 0x3b, 0x36, 0x5c, 0x7c, 0x7a, 0x90,  0x67, 0x4d, 0x4d, 0x52, 0x54, 0x65, 0x4b, 0x32, 0x42, 0x3b, 0x35, 0x34,  0x27, 0x2a, 0x40, 0x45, 0x3a, 0x34, 0x43, 0x49, 0x43, 0x32, 0x2f, 0x32,  0x2d, 0x32, 0x2c, 0x3c, 0x66, 0x63, 0x42, 0x26, 0x1f, 0x26, 0x25, 0x29,  0x2c, 0x2c, 0x35, 0x31, 0x2a, 0x27, 0x2a, 0x2b, 0x2c, 0x2c, 0x2c, 0x2c,  0x2c, 0x2b, 0x2b, 0x2d, 0x2e, 0x2e, 0x2d, 0x2e, 0x2d, 0x2d, 0x2c, 0x2c,  0x2c, 0x2b, 0x2d, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2e, 0x2d, 0x2c, 0x29,  0x27, 0x27, 0x25, 0x23, 0x22, 0x22, 0x21, 0x21, 0x2e, 0x4c, 0x31, 0x26,  0x41, 0x2f, 0x29, 0x2b, 0x2b, 0x29, 0x29, 0x28, 0x28, 0x28, 0x2a, 0x2e,  0x2d, 0x2e, 0x30, 0x31, 0x33, 0x32, 0x31, 0x2e, 0x2e, 0x2e, 0x2e, 0x2d,  0x2e, 0x2b, 0x2c, 0x2c, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x30, 0x2f, 0x2f,  0x2f, 0x2e, 0x30, 0x30, 0x30, 0x31, 0x32, 0x33, 0x33, 0x32, 0x32, 0x32,  0x33, 0x32, 0x33, 0x33, 0x32, 0x32, 0x30, 0x2b, 0x29, 0x26, 0x27, 0x25,  0x23, 0x23, 0x23, 0x32, 0x49, 0x3e, 0x42, 0x3e, 0x40, 0x41, 0x41, 0x3f,  0x40, 0x38, 0x2f, 0x33, 0x42, 0x50, 0x41, 0x29, 0x35, 0x31, 0x33, 0x43,  0x46, 0x44, 0x35, 0x69, 0x79, 0x82, 0x7c, 0x86, 0x73, 0x66, 0x3f, 0x34,  0x68, 0x6d, 0x5b, 0x40, 0x48, 0x32, 0x31, 0x33, 0x2b, 0x2e, 0x33, 0x31,  0x3b, 0x39, 0x32, 0x41, 0x44, 0x3d, 0x34, 0x32, 0x2f, 0x29, 0x2c, 0x34,  0x65, 0x70, 0x61, 0x46, 0x31, 0x2a, 0x31, 0x32, 0x3a, 0x33, 0x2b, 0x43,  0x41, 0x2d, 0x2b, 0x2a, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2b, 0x2d,  0x2e, 0x2f, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2c, 0x2c, 0x2a, 0x2d, 0x2c,  0x2c, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2c, 0x29, 0x27, 0x25, 0x24, 0x23,  0x22, 0x22, 0x21, 0x21, 0x2c, 0x45, 0x2d, 0x27, 0x44, 0x31, 0x29, 0x2a,  0x2b, 0x2a, 0x2a, 0x29, 0x29, 0x2a, 0x2f, 0x32, 0x30, 0x30, 0x32, 0x32,  0x34, 0x34, 0x33, 0x32, 0x30, 0x30, 0x30, 0x2f, 0x2e, 0x2d, 0x2d, 0x2e,  0x2f, 0x2f, 0x2e, 0x2f, 0x30, 0x30, 0x2f, 0x30, 0x30, 0x31, 0x32, 0x31,  0x31, 0x31, 0x34, 0x33, 0x34, 0x32, 0x33, 0x33, 0x35, 0x34, 0x34, 0x33,  0x33, 0x33, 0x32, 0x2b, 0x29, 0x26, 0x27, 0x25, 0x24, 0x24, 0x23, 0x32,  0x47, 0x3d, 0x42, 0x41, 0x3e, 0x3a, 0x3c, 0x3c, 0x3d, 0x3d, 0x3f, 0x34,  0x3c, 0x50, 0x3b, 0x37, 0x3b, 0x32, 0x32, 0x40, 0x46, 0x3d, 0x51, 0x8a,  0x83, 0x8f, 0x82, 0x6b, 0x5f, 0x49, 0x3c, 0x44, 0x68, 0x67, 0x53, 0x3d,  0x47, 0x2d, 0x2d, 0x35, 0x2d, 0x30, 0x36, 0x27, 0x31, 0x40, 0x30, 0x4a,  0x4d, 0x4c, 0x43, 0x3b, 0x33, 0x32, 0x47, 0x34, 0x47, 0x68, 0x57, 0x4c,  0x49, 0x3a, 0x3f, 0x3c, 0x3c, 0x39, 0x2f, 0x49, 0x43, 0x33, 0x2c, 0x2b,  0x2c, 0x2c, 0x2e, 0x2c, 0x2d, 0x2e, 0x2c, 0x2d, 0x2e, 0x2f, 0x2e, 0x2e,  0x30, 0x30, 0x2d, 0x2d, 0x2d, 0x2c, 0x2c, 0x2d, 0x2c, 0x2d, 0x2d, 0x2f,  0x2f, 0x2f, 0x2c, 0x29, 0x27, 0x26, 0x23, 0x23, 0x23, 0x22, 0x21, 0x22,  0x2a, 0x3e, 0x2b, 0x27, 0x44, 0x32, 0x2c, 0x2c, 0x2d, 0x2c, 0x2c, 0x2b,  0x2c, 0x2f, 0x34, 0x34, 0x33, 0x32, 0x34, 0x35, 0x36, 0x36, 0x33, 0x34,  0x33, 0x32, 0x31, 0x30, 0x30, 0x2f, 0x2f, 0x2f, 0x30, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x33, 0x33, 0x33, 0x32, 0x35, 0x35,  0x35, 0x34, 0x33, 0x34, 0x35, 0x35, 0x33, 0x33, 0x34, 0x35, 0x32, 0x2c,  0x2a, 0x29, 0x28, 0x25, 0x24, 0x24, 0x23, 0x33, 0x46, 0x3e, 0x40, 0x3c,  0x34, 0x2d, 0x3a, 0x3e, 0x3e, 0x3b, 0x42, 0x34, 0x45, 0x3c, 0x2b, 0x3b,  0x36, 0x32, 0x34, 0x3f, 0x43, 0x44, 0x75, 0x90, 0x81, 0x86, 0x5c, 0x3f,  0x3c, 0x40, 0x41, 0x5b, 0x5e, 0x64, 0x49, 0x3d, 0x2e, 0x23, 0x29, 0x34,  0x40, 0x3d, 0x2d, 0x30, 0x29, 0x34, 0x35, 0x47, 0x4d, 0x46, 0x41, 0x3e,  0x34, 0x40, 0x52, 0x55, 0x3a, 0x51, 0x4e, 0x44, 0x3d, 0x3f, 0x3c, 0x41,  0x37, 0x2a, 0x28, 0x35, 0x38, 0x30, 0x2c, 0x2c, 0x2c, 0x2b, 0x2c, 0x2c,  0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2d, 0x2e, 0x2e, 0x2d, 0x2c,  0x2c, 0x2d, 0x2d, 0x2c, 0x2c, 0x2c, 0x2d, 0x2f, 0x2e, 0x2e, 0x2c, 0x29,  0x28, 0x26, 0x24, 0x23, 0x23, 0x22, 0x22, 0x21, 0x2a, 0x47, 0x30, 0x2a,  0x49, 0x34, 0x2c, 0x2c, 0x2d, 0x2d, 0x2b, 0x2c, 0x2f, 0x31, 0x30, 0x2e,  0x2f, 0x30, 0x33, 0x35, 0x36, 0x36, 0x33, 0x33, 0x32, 0x33, 0x32, 0x30,  0x30, 0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x31, 0x32, 0x31, 0x31, 0x31,  0x30, 0x30, 0x30, 0x32, 0x32, 0x33, 0x34, 0x33, 0x34, 0x32, 0x33, 0x33,  0x33, 0x33, 0x33, 0x34, 0x34, 0x35, 0x32, 0x2c, 0x2a, 0x28, 0x26, 0x24,  0x23, 0x23, 0x22, 0x33, 0x44, 0x37, 0x32, 0x2d, 0x2a, 0x31, 0x40, 0x41,  0x40, 0x3d, 0x38, 0x2d, 0x25, 0x22, 0x32, 0x41, 0x3c, 0x37, 0x3a, 0x44,  0x40, 0x50, 0x4b, 0x35, 0x30, 0x2a, 0x46, 0x57, 0x39, 0x39, 0x4b, 0x4f,  0x40, 0x4a, 0x49, 0x42, 0x28, 0x24, 0x25, 0x27, 0x2e, 0x38, 0x38, 0x2e,  0x27, 0x3c, 0x37, 0x34, 0x43, 0x33, 0x33, 0x30, 0x40, 0x45, 0x4f, 0x61,  0x54, 0x3e, 0x40, 0x3a, 0x40, 0x3f, 0x2d, 0x28, 0x2a, 0x26, 0x27, 0x31,  0x2e, 0x2c, 0x2b, 0x2b, 0x2c, 0x2b, 0x2b, 0x2b, 0x2d, 0x2e, 0x2e, 0x2e};
mfxU8  video2_20x20_3_nv12[] = {0x2e, 0x2f, 0x3d, 0x40, 0x35, 0x33, 0x38, 0x3b, 0x3b, 0x38, 0x36, 0x34,  0x32, 0x33, 0x35, 0x36, 0x32, 0x3b, 0x3c, 0x33, 0x2d, 0x31, 0x33, 0x33,  0x32, 0x32, 0x31, 0x31, 0x2d, 0x29, 0x22, 0x23, 0x29, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x29,  0x28, 0x26, 0x25, 0x24, 0x23, 0x22, 0x22, 0x21, 0x30, 0x4d, 0x38, 0x29,  0x5a, 0x44, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b,  0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2f, 0x2f,  0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2e, 0x2b, 0x2a, 0x27, 0x23,  0x23, 0x24, 0x28, 0x2a, 0x4e, 0x4a, 0x42, 0x3d, 0x37, 0x35, 0x34, 0x34,  0x33, 0x33, 0x31, 0x31, 0x2f, 0x30, 0x31, 0x32, 0x32, 0x32, 0x33, 0x33,  0x40, 0x35, 0x35, 0x40, 0x40, 0x40, 0x40, 0x40, 0x68, 0x5f, 0x4d, 0x44,  0x48, 0x3b, 0x39, 0x3c, 0x3e, 0x3c, 0x3c, 0x3c, 0x2e, 0x34, 0x35, 0x31,  0x31, 0x31, 0x38, 0x42, 0x3c, 0x3a, 0x35, 0x32, 0x30, 0x31, 0x33, 0x34,  0x26, 0x30, 0x34, 0x2f, 0x30, 0x36, 0x38, 0x35, 0x31, 0x30, 0x2e, 0x2e,  0x2d, 0x2b, 0x29, 0x29, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x29, 0x28, 0x26, 0x25, 0x24,  0x23, 0x22, 0x22, 0x21, 0x30, 0x4d, 0x38, 0x29, 0x5a, 0x44, 0x2f, 0x2e,  0x2d, 0x2c, 0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b, 0x2d, 0x2e, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2f, 0x2f, 0x2e, 0x2e, 0x2d, 0x2d,  0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30,  0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x30, 0x2f, 0x2e, 0x2b, 0x2a, 0x27, 0x23, 0x23, 0x24, 0x2b, 0x2e,  0x44, 0x44, 0x3c, 0x38, 0x31, 0x2f, 0x2e, 0x2e, 0x2e, 0x30, 0x3e, 0x45,  0x59, 0x3b, 0x2a, 0x38, 0x33, 0x33, 0x33, 0x33, 0x3c, 0x31, 0x31, 0x3c,  0x3c, 0x3c, 0x3f, 0x40, 0x4d, 0x4c, 0x43, 0x3f, 0x50, 0x40, 0x3a, 0x3c,  0x3d, 0x3c, 0x3c, 0x3c, 0x28, 0x2c, 0x23, 0x22, 0x34, 0x37, 0x38, 0x3a,  0x3a, 0x35, 0x31, 0x2e, 0x2d, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2c, 0x29,  0x31, 0x3c, 0x3f, 0x38, 0x33, 0x30, 0x2b, 0x2a, 0x2b, 0x2d, 0x2e, 0x2d,  0x2d, 0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2a, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x23, 0x22,  0x30, 0x4d, 0x38, 0x29, 0x5a, 0x41, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a,  0x2a, 0x2a, 0x2a, 0x2b, 0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30,  0x30, 0x30, 0x2f, 0x2f, 0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e,  0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2e,  0x2b, 0x2a, 0x27, 0x23, 0x23, 0x24, 0x2b, 0x2e, 0x3d, 0x3d, 0x36, 0x32,  0x2d, 0x2d, 0x2d, 0x2e, 0x33, 0x37, 0x42, 0x49, 0x69, 0x46, 0x2d, 0x33,  0x32, 0x32, 0x32, 0x32, 0x35, 0x2a, 0x2a, 0x35, 0x35, 0x35, 0x36, 0x36,  0x37, 0x3c, 0x45, 0x4a, 0x60, 0x4c, 0x3c, 0x39, 0x3d, 0x3d, 0x3b, 0x3a,  0x2d, 0x28, 0x24, 0x2d, 0x43, 0x49, 0x3e, 0x38, 0x35, 0x33, 0x2f, 0x2d,  0x2b, 0x2c, 0x2c, 0x2b, 0x2b, 0x2a, 0x2c, 0x2e, 0x3b, 0x47, 0x47, 0x3e,  0x33, 0x31, 0x2c, 0x2a, 0x29, 0x2c, 0x2e, 0x2e, 0x2d, 0x2d, 0x2c, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x29,  0x29, 0x27, 0x26, 0x25, 0x24, 0x24, 0x23, 0x23, 0x30, 0x4d, 0x38, 0x29,  0x5a, 0x41, 0x30, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b,  0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2f, 0x2f,  0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2e, 0x2b, 0x2a, 0x27, 0x23,  0x23, 0x24, 0x2b, 0x2e, 0x3d, 0x3d, 0x37, 0x33, 0x2e, 0x2e, 0x2e, 0x33,  0x3c, 0x41, 0x42, 0x44, 0x49, 0x42, 0x34, 0x2e, 0x31, 0x31, 0x32, 0x31,  0x32, 0x2a, 0x2a, 0x35, 0x32, 0x32, 0x32, 0x32, 0x36, 0x3f, 0x51, 0x5a,  0x65, 0x55, 0x3f, 0x37, 0x3d, 0x3e, 0x3b, 0x39, 0x24, 0x1d, 0x24, 0x2d,  0x36, 0x3c, 0x3b, 0x31, 0x37, 0x34, 0x2f, 0x2d, 0x2b, 0x2c, 0x2e, 0x2b,  0x21, 0x1f, 0x2a, 0x30, 0x41, 0x4b, 0x47, 0x3d, 0x3b, 0x37, 0x31, 0x2d,  0x2a, 0x2b, 0x2c, 0x2c, 0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x2a, 0x29, 0x28, 0x27, 0x27,  0x25, 0x25, 0x24, 0x23, 0x30, 0x4d, 0x38, 0x29, 0x4f, 0x3d, 0x2f, 0x2e,  0x2c, 0x2c, 0x2b, 0x2b, 0x2a, 0x2a, 0x2a, 0x2b, 0x2d, 0x2e, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2f, 0x2f, 0x2e, 0x2e, 0x2d, 0x2d,  0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30,  0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x30, 0x2f, 0x2e, 0x2b, 0x2a, 0x27, 0x23, 0x23, 0x24, 0x2b, 0x2e,  0x40, 0x41, 0x3b, 0x38, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x3e, 0x3e,  0x3b, 0x3e, 0x38, 0x2b, 0x30, 0x31, 0x32, 0x32, 0x32, 0x35, 0x37, 0x38,  0x39, 0x38, 0x38, 0x39, 0x39, 0x38, 0x51, 0x68, 0x5e, 0x59, 0x43, 0x35,  0x3d, 0x3d, 0x3a, 0x38, 0x23, 0x22, 0x2e, 0x2f, 0x24, 0x28, 0x3b, 0x3c,  0x3a, 0x36, 0x31, 0x2e, 0x2d, 0x2e, 0x2e, 0x2e, 0x2d, 0x2d, 0x29, 0x23,  0x2a, 0x36, 0x3a, 0x3b, 0x3a, 0x37, 0x34, 0x31, 0x2c, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2a, 0x2a, 0x29, 0x28, 0x27, 0x27, 0x25, 0x25, 0x24, 0x24,  0x30, 0x4d, 0x38, 0x29, 0x44, 0x35, 0x2f, 0x2d, 0x2c, 0x2c, 0x2b, 0x2b,  0x2a, 0x2a, 0x2a, 0x2b, 0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30,  0x30, 0x30, 0x2f, 0x2f, 0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e,  0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2e,  0x2b, 0x2a, 0x27, 0x23, 0x23, 0x24, 0x2b, 0x2f, 0x42, 0x42, 0x40, 0x3c,  0x3a, 0x3b, 0x3b, 0x3b, 0x39, 0x38, 0x3b, 0x39, 0x39, 0x3f, 0x3b, 0x2c,  0x31, 0x31, 0x31, 0x32, 0x41, 0x3e, 0x3a, 0x3c, 0x5f, 0x6e, 0x7d, 0x84,  0x66, 0x55, 0x4c, 0x52, 0x5b, 0x5f, 0x4a, 0x35, 0x3d, 0x3b, 0x38, 0x37,  0x35, 0x3a, 0x40, 0x3c, 0x35, 0x35, 0x42, 0x4b, 0x3d, 0x3a, 0x36, 0x30,  0x30, 0x32, 0x35, 0x34, 0x60, 0x5a, 0x41, 0x2a, 0x1a, 0x21, 0x2c, 0x30,  0x32, 0x35, 0x35, 0x34, 0x33, 0x30, 0x2e, 0x2e, 0x2d, 0x2c, 0x2c, 0x2b,  0x2b, 0x2b, 0x2c, 0x2c, 0x2c, 0x2c, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x2a,  0x29, 0x28, 0x27, 0x27, 0x25, 0x25, 0x24, 0x23, 0x2e, 0x4b, 0x36, 0x27,  0x3a, 0x32, 0x2d, 0x2d, 0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2c,  0x2e, 0x2f, 0x30, 0x30, 0x31, 0x31, 0x2f, 0x31, 0x31, 0x30, 0x30, 0x2f,  0x2f, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2e, 0x2b, 0x2a, 0x27, 0x23,  0x23, 0x24, 0x2a, 0x30, 0x3f, 0x3f, 0x40, 0x3e, 0x39, 0x3b, 0x3d, 0x3d,  0x3c, 0x3c, 0x3c, 0x3a, 0x3d, 0x46, 0x42, 0x33, 0x31, 0x32, 0x33, 0x34,  0x51, 0x41, 0x45, 0x5a, 0x76, 0x7d, 0x84, 0x84, 0x78, 0x5e, 0x40, 0x3c,  0x66, 0x66, 0x51, 0x3c, 0x3b, 0x36, 0x32, 0x32, 0x2e, 0x35, 0x2e, 0x30,  0x39, 0x3d, 0x30, 0x36, 0x3e, 0x38, 0x3a, 0x32, 0x32, 0x35, 0x39, 0x36,  0x69, 0x6a, 0x5a, 0x45, 0x2e, 0x2b, 0x2a, 0x29, 0x2f, 0x33, 0x36, 0x37,  0x37, 0x34, 0x31, 0x30, 0x2e, 0x2e, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2d,  0x2e, 0x2e, 0x2e, 0x2e, 0x2d, 0x2d, 0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b,  0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2a, 0x2a, 0x29, 0x28, 0x27, 0x27,  0x25, 0x25, 0x24, 0x24, 0x2d, 0x4a, 0x35, 0x26, 0x38, 0x2f, 0x2c, 0x2d,  0x2c, 0x2c, 0x2b, 0x2b, 0x2b, 0x2b, 0x2c, 0x2d, 0x2f, 0x30, 0x30, 0x31,  0x31, 0x32, 0x2f, 0x32, 0x32, 0x30, 0x30, 0x30, 0x30, 0x2f, 0x2e, 0x2e,  0x2e, 0x2e, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31,  0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,  0x32, 0x31, 0x30, 0x2f, 0x2c, 0x2a, 0x27, 0x23, 0x23, 0x24, 0x2a, 0x31,  0x3e, 0x3e, 0x3e, 0x3c, 0x39, 0x3b, 0x3e, 0x3e, 0x40, 0x40, 0x3e, 0x3f,  0x43, 0x49, 0x41, 0x34, 0x31, 0x31, 0x33, 0x34, 0x49, 0x3b, 0x52, 0x77,  0x8a, 0x83, 0x75, 0x6e, 0x5a, 0x4a, 0x3e, 0x47, 0x69, 0x6a, 0x54, 0x3f,  0x3d, 0x35, 0x2d, 0x2d, 0x2f, 0x35, 0x3a, 0x28, 0x36, 0x3e, 0x38, 0x41,  0x42, 0x4a, 0x3b, 0x34, 0x2f, 0x38, 0x3d, 0x3f, 0x47, 0x5c, 0x57, 0x4f,  0x44, 0x3a, 0x43, 0x36, 0x32, 0x32, 0x35, 0x36, 0x36, 0x34, 0x33, 0x31,  0x2f, 0x2e, 0x2d, 0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2e,  0x2e, 0x2d, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,  0x2c, 0x2c, 0x2a, 0x2a, 0x29, 0x27, 0x26, 0x25, 0x24, 0x24, 0x23, 0x22,  0x2c, 0x49, 0x34, 0x25, 0x41, 0x30, 0x2c, 0x2d, 0x2d, 0x2d, 0x2c, 0x2c,  0x2c, 0x2c, 0x2c, 0x2d, 0x2f, 0x30, 0x31, 0x31, 0x32, 0x33, 0x2f, 0x33,  0x33, 0x31, 0x31, 0x31, 0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x30,  0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,  0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x31, 0x30, 0x2f,  0x2c, 0x2b, 0x27, 0x23, 0x23, 0x25, 0x29, 0x32, 0x3c, 0x3b, 0x3a, 0x38,  0x37, 0x3b, 0x3e, 0x3f, 0x40, 0x40, 0x3c, 0x38, 0x40, 0x3f, 0x3a, 0x34,  0x30, 0x34, 0x37, 0x38, 0x3b, 0x46, 0x6a, 0x84, 0x7e, 0x81, 0x53, 0x45,  0x3b, 0x42, 0x3f, 0x5d, 0x59, 0x5d, 0x53, 0x3e, 0x34, 0x2d, 0x2a, 0x2a,  0x34, 0x37, 0x34, 0x25, 0x2e, 0x39, 0x3c, 0x43, 0x45, 0x41, 0x3a, 0x38,  0x3a, 0x3b, 0x43, 0x4c, 0x40, 0x45, 0x51, 0x4a, 0x43, 0x3b, 0x38, 0x41,  0x30, 0x30, 0x32, 0x32, 0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2d, 0x2c,  0x2c, 0x2c, 0x2d, 0x2e, 0x2f, 0x2f, 0x30, 0x30, 0x2f, 0x2f, 0x2e, 0x2e,  0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2a, 0x2a,  0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x23, 0x22, 0x2d, 0x4a, 0x35, 0x26,  0x4c, 0x36, 0x2c, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2b, 0x2b, 0x2c, 0x2d,  0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x30, 0x33, 0x33, 0x31, 0x31, 0x31,  0x30, 0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x31,  0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,  0x32, 0x32, 0x32, 0x32, 0x32, 0x31, 0x30, 0x2f, 0x2c, 0x2a, 0x27, 0x23,  0x23, 0x25, 0x29, 0x32, 0x39, 0x37, 0x2f, 0x2e, 0x34, 0x39, 0x42, 0x41,  0x3f, 0x3d, 0x35, 0x2f, 0x29, 0x2a, 0x37, 0x38, 0x3a, 0x3b, 0x3e, 0x40,  0x4c, 0x55, 0x4a, 0x35, 0x3c, 0x2e, 0x4f, 0x5b, 0x3a, 0x41, 0x46, 0x5a,  0x43, 0x4d, 0x4f, 0x47, 0x2b, 0x29, 0x29, 0x29, 0x31, 0x35, 0x34, 0x35,  0x33, 0x36, 0x40, 0x37, 0x3c, 0x34, 0x33, 0x3b, 0x41, 0x3f, 0x47, 0x54,  0x57, 0x3a, 0x3d, 0x3e, 0x42, 0x40, 0x29, 0x2d, 0x2e, 0x2e, 0x2e, 0x2e,  0x2e, 0x2e, 0x2e, 0x2e, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2e, 0x2f};
const mfxU32 videoSize = 1800;
const mfxU32 size = 20;    // width == height == 20
const mfxU32 frameSizeBytes = size * size * 3 / 2;
const mfxU32 UVPlaneOffsetBytes = size * size;
const mfxU32 imageRowSizeBytes = size;
const mfxU32 frameCount = 3;
// reference results for test ssim_algorithm_1
const mfxF64 ssimPerFrame[frameCount * 3] =
{
    0.954158, 0.840583, 0.924375,
    0.943244, 0.936326, 0.963447,
    0.959811, 0.960266, 0.959929
};
const mfxF64 ssimAverage[3] = {0.952404, 0.912392, 0.949250};
const mfxF64 ssimMax[3]     = {0.959811, 0.960266, 0.963447};
const mfxF64 ssimMin[3]     = {0.943244, 0.840583, 0.924375};
// reference results for test ssim_algorithm_2
const mfxF64 ones9[frameCount * 3] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
const mfxF64 ones3[3] = {1, 1, 1};

struct VideoDataFixture
{
    mfxFrameSurface1 in1[frameCount];
    mfxFrameSurface1 in2[frameCount];

    VideoDataFixture()
    {
        MFX_FOR( frameCount, initializeFrame(in1[i], i, video1_20x20_3_nv12) );
        MFX_FOR( frameCount, initializeFrame(in2[i], i, video2_20x20_3_nv12) );

    };
    void VideoDataFixture::initializeFrame(mfxFrameSurface1 &surface, mfxU32 frameNumber, mfxU8 *data)
    {
        surface.Info.FourCC = MFX_FOURCC_NV12;
        surface.Info.CropH = surface.Info.CropW = size;
        surface.Info.CropX = surface.Info.CropY = 0;

        const mfxU32 frameOffset = frameSizeBytes * frameNumber;
        surface.Data.Y = data + frameOffset;
        surface.Data.UV = surface.Data.Y + UVPlaneOffsetBytes;
        surface.Data.Pitch = imageRowSizeBytes;
    }
};

SUITE(SSIMCmpOption)
{
    TEST(ssim_option_exists)
    {
        PipelineRunner<MFXPipelineConfigDecode>::CmdLineParams params;
        params[VM_STRING("-ssim")] = std::vector<tstring>();

        PipelineRunner<MFXPipelineConfigDecode> pipeline;

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));
    }

    void testCleanSSIMCalc(MFXSSIMCalc &ssimCalc)
    {
        mfxF64 out[3];
        mfxF64 expected[3];

        CHECK( vm_string_strcmp(VM_STRING("SSIM"), ssimCalc.GetMetricName()) == 0 );
        CHECK_EQUAL(MFX_ERR_UNKNOWN, ssimCalc.GetLastCmpResult(out));
        CHECK_EQUAL(MFX_ERR_UNKNOWN, ssimCalc.GetOveralResult(out));

        const mfxF64 infMaxSSIM = 1.1;
        MFX_FOR(3, expected[i] = infMaxSSIM);
        CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetMinResult(out));
        CHECK_ARRAY_EQUAL(expected, out, 3);

        const mfxF64 infMinSSIM = 0;
        MFX_FOR(3, expected[i] = infMinSSIM);
        CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetMaxResult(out));
        CHECK_ARRAY_EQUAL(expected, out, 3);
    }

    TEST(ssim_constructor)
    {
        MFXSSIMCalc ssimCalc;
        testCleanSSIMCalc(ssimCalc);
    }

    TEST(ssim_copy_constructor)
    {
        MFXSSIMCalc ssimCalc;
        MFXSSIMCalc ssimCopy(ssimCalc);
        testCleanSSIMCalc(ssimCopy);
    }

    void testSSIMAlgorithm(mfxFrameSurface1 image1[frameCount], mfxFrameSurface1 image2[frameCount], const mfxF64 ssimPFR[frameCount*3],
                                const mfxF64 ssimAV[3], const mfxF64 ssimMAX[3], const mfxF64 ssimMIN[3])
    {
        MFXSSIMCalc ssimCalc;
        mfxF64 out[frameCount*3];
        
        for (int i = 0; i < frameCount; i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.Compare(image1 + i, image2 + i));
            CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetLastCmpResult(out + i*3));
        }

        CHECK_ARRAY_CLOSE(ssimPFR, out, frameCount*3, tolerance);

        mfxF64 averageRes[3], minRes[3], maxRes[3];
        CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetOveralResult(averageRes));
        CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetMaxResult(maxRes));
        CHECK_EQUAL(MFX_ERR_NONE, ssimCalc.GetMinResult(minRes));

        CHECK_ARRAY_CLOSE(ssimAV, averageRes, 3, tolerance);
        CHECK_ARRAY_CLOSE(ssimMAX, maxRes, 3, tolerance);
        CHECK_ARRAY_CLOSE(ssimMIN, minRes, 3, tolerance);

        ssimCalc.Reset();
        testCleanSSIMCalc(ssimCalc);
    }

    TEST_FIXTURE(VideoDataFixture, SKIPPED_ssim_algorithm_1)
    {
        testSSIMAlgorithm(in1, in2, ssimPerFrame, ssimAverage, ssimMax, ssimMin);
    }
    
    TEST_FIXTURE(VideoDataFixture, SKIPPED_ssim_algorithm_2)
    {
        testSSIMAlgorithm(in1, in1, ones9, ones3, ones3, ones3);
    }
}