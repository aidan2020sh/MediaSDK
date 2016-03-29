/*
//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//    Copyright (c) 2001-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER) || defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#ifndef __JPEGBASE_H__
#include "jpegbase.h"
#endif


// raw quant tables must be in zigzag order
const Ipp8u DefaultLuminanceQuant[64] =
{
  16,  11,  12,  14,  12,  10,  16,  14,
  13,  14,  18,  17,  16,  19,  24,  40,
  26,  24,  22,  22,  24,  49,  35,  37,
  29,  40,  58,  51,  61,  60,  57,  51,
  56,  55,  64,  72,  92,  78,  64,  68,
  87,  69,  55,  56,  80, 109,  81,  87,
  95,  98, 103, 104, 103,  62,  77, 113,
 121, 112, 100, 120,  92, 101, 103,  99
};


// raw quant tables must be in zigzag order
const Ipp8u DefaultChrominanceQuant[64] =
{
  17,  18,  18,  24,  21,  24,  47,  26,
  26,  47,  99,  66,  56,  66,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99
};


const Ipp8u DefaultLuminanceDCBits[16] =
{
  0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


const Ipp8u DefaultLuminanceDCValues[256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b
};


const Ipp8u DefaultChrominanceDCBits[16] =
{
  0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};


const Ipp8u DefaultChrominanceDCValues[256] =
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b
};


const Ipp8u DefaultLuminanceACBits[16] =
{
  0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
  0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d
};


const Ipp8u DefaultLuminanceACValues[256] =
{
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};


const Ipp8u DefaultChrominanceACBits[16] =
{
  0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
  0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
};


const Ipp8u DefaultChrominanceACValues[256] =
{
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};


const vm_char* GetErrorStr(JERRCODE code)
{
  const vm_char* str;

  switch(code)
  {
    case JPEG_OK:               str = VM_STRING("no error"); break;
    case JPEG_NOT_IMPLEMENTED:  str = VM_STRING("requested feature is not implemented"); break;
    case JPEG_ERR_INTERNAL:     str = VM_STRING("internal error"); break;
    case JPEG_ERR_PARAMS:       str = VM_STRING("wrong parameters"); break;
    case JPEG_ERR_BUFF:         str = VM_STRING("buffer too small"); break;
    case JPEG_ERR_FILE:         str = VM_STRING("file io operation failed"); break;
    case JPEG_ERR_ALLOC:        str = VM_STRING("memory allocation failed"); break;
    case JPEG_ERR_BAD_DATA:     str = VM_STRING("bad segment data"); break;
    case JPEG_ERR_DHT_DATA:     str = VM_STRING("bad huffman table segment"); break;
    case JPEG_ERR_DQT_DATA:     str = VM_STRING("bad quant table segment"); break;
    case JPEG_ERR_SOS_DATA:     str = VM_STRING("bad scan segment"); break;
    case JPEG_ERR_SOF_DATA:     str = VM_STRING("bad frame segment"); break;
    case JPEG_ERR_RST_DATA:     str = VM_STRING("wrong restart marker"); break;
    default:                    str = VM_STRING("unknown code"); break;
  }

  return str;
} // GetErrorStr()


int get_num_threads(void)
{
  int maxThreads = 1;
#ifdef _OPENMP
#ifdef __INTEL_COMPILER
  kmp_set_blocktime(0);
#endif
#pragma omp parallel shared(maxThreads)
  {
#pragma omp master
    {
      maxThreads = omp_get_num_threads();
    }
  }
#endif
  return maxThreads;
} // get_num_threads()


void set_num_threads(int maxThreads)
{
  maxThreads;
#ifdef _OPENMP
      omp_set_num_threads(maxThreads);
#endif
  return;
} // set_num_threads()


JERRCODE CMemoryBuffer::Allocate(int size)
{
  Delete();

  m_buffer_size = size;

  m_buffer = (Ipp8u*)ippMalloc(m_buffer_size);
  if(0 == m_buffer)
  {
    m_buffer_size = 0;
    return JPEG_ERR_ALLOC;
  }

  return JPEG_OK;
} // CMemoryBuffer::Allocate()


JERRCODE CMemoryBuffer::Delete(void)
{
  if(m_buffer != 0)
    ippFree(m_buffer);

  m_buffer_size = 0;
  m_buffer      = 0;

  return JPEG_OK;
} // CMemoryBuffer::Delete()

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER || UMC_ENABLE_MJPEG_VIDEO_ENCODER

