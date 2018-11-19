// Copyright (c) 2006-2018 Intel Corporation
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

#ifndef __MEMBUFFOUT_H__
#define __MEMBUFFOUT_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER) || defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)
#include <stdio.h>
#include "basestream.h"
#include "basestreamout.h"


class CMemBuffOutput : public CBaseStreamOutput
{
public:
  CMemBuffOutput(void);
  ~CMemBuffOutput(void);

  JERRCODE Open(uint8_t* pBuf, int buflen);
  JERRCODE Close(void);

  JERRCODE Write(void* buf,uic_size_t len,uic_size_t* cnt);

  size_t GetPosition() { return (size_t)m_currpos; }

private:
  JERRCODE Open(vm_char* /*name*/) { return JPEG_NOT_IMPLEMENTED; }

protected:
  uint8_t*  m_buf;
  int     m_buflen;
  int     m_currpos;

};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER || UMC_ENABLE_MJPEG_VIDEO_ENCODER
#endif // __MEMBUFFOUT_H__

