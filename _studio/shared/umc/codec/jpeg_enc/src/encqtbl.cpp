// Copyright (c) 2001-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#include "umc_structures.h"

#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
#pragma warning (disable:1478)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "encqtbl.h"


CJPEGEncoderQuantTable::CJPEGEncoderQuantTable(void)
{
  m_id          = 0;
  m_precision   = 0;
  m_initialized = false;

  // align for max performance
  m_raw8u  = UMC::align_pointer<Ipp8u *>(m_rbf,CPU_CACHE_LINE);
  m_raw16u = UMC::align_pointer<Ipp16u *>(m_rbf,CPU_CACHE_LINE);
  m_qnt16u = UMC::align_pointer<Ipp16u *>(m_qbf,CPU_CACHE_LINE);
  m_qnt32f = UMC::align_pointer<Ipp32f *>(m_qbf,CPU_CACHE_LINE);

  memset(m_rbf, 0, sizeof(m_rbf));
  memset(m_qbf, 0, sizeof(m_qbf));

  return;
} // ctor


CJPEGEncoderQuantTable::~CJPEGEncoderQuantTable(void)
{
  m_id          = 0;
  m_precision   = 0;
  m_initialized = false;

  memset(m_rbf, 0, sizeof(m_rbf));
  memset(m_qbf, 0, sizeof(m_qbf));

  return;
} // dtor


JERRCODE CJPEGEncoderQuantTable::Init(int id,Ipp8u raw[64],int quality)
{
  IppStatus status;

  m_id        = id;
  m_precision = 0; // 8-bit precision

  MFX_INTERNAL_CPY(m_raw8u,raw,DCTSIZE2);

  // scale according quality parameter
  if(quality)
  {
    status = ippiQuantFwdRawTableInit_JPEG_8u(m_raw8u,quality);
    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiQuantFwdRawTableInit_JPEG_8u() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  status = ippiQuantFwdTableInit_JPEG_8u16u(m_raw8u,m_qnt16u);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantFwdTableInit_JPEG_8u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }

  m_initialized = true;

  return JPEG_OK;
} // CJPEGEncoderQuantTable::Init()


static
IppStatus ippiQuantFwdRawTableInit_JPEG_16u(
  Ipp16u* raw,
  int     quality)
{
  int i;
  int val;

  if(quality > 100)
    quality = 100;

  if(quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - (quality * 2);

  for(i = 0; i < DCTSIZE2; i++)
  {
    val = (raw[i] * quality + 50) / 100;
    if(val < 1)
    {
      raw[i] = (Ipp16u)1;
    }
    else if(val > 65535)
    {
      raw[i] = (Ipp16u)65535;
    }
    else
    {
      raw[i] = (Ipp16u)val;
    }
  }

  return ippStsNoErr;
} // ippiQuantFwdRawTableInit_JPEG_16u()


static
IppStatus ippiQuantFwdTableInit_JPEG_16u(
  Ipp16u* raw,
  Ipp32f* qnt)
{
  int       i;
  Ipp16u    wb[DCTSIZE2];
  IppStatus status;

  status = ippiZigzagInv8x8_16s_C1((const Ipp16s*)&raw[0],(Ipp16s*)&wb[0]);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiZigzagInv8x8_16s_C1() failed - ",status);
    return status;
  }

  for(i = 0; i < DCTSIZE2; i++)
  {
    qnt[i] = (float)(1.0 / wb[i]);
  }

  return ippStsNoErr;
} // ippiQuantFwdTableInit_JPEG_16u()


JERRCODE CJPEGEncoderQuantTable::Init(int id,Ipp16u raw[64],int quality)
{
  IppStatus status;

  m_id        = id;
  m_precision = 1; // 16-bit precision

  MFX_INTERNAL_CPY((Ipp8u*)m_raw16u,(Ipp8u*)raw,DCTSIZE2*sizeof(Ipp16u));

  if(quality)
  {
    status = ippiQuantFwdRawTableInit_JPEG_16u(m_raw16u,quality);
    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiQuantFwdRawTableInit_JPEG_16u() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  status = ippiQuantFwdTableInit_JPEG_16u(m_raw16u,m_qnt32f);

  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantFwdTableInit_JPEG_16u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }

  m_initialized = true;

  return JPEG_OK;
} // CJPEGEncoderQuantTable::Init()

#endif
