//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MEMBUFFIN_H__
#define __MEMBUFFIN_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "basestream.h"
#include "basestreamin.h"

class CMemBuffInput : public CBaseStreamInput
{
public:
  CMemBuffInput(void);
  ~CMemBuffInput(void);

  JERRCODE Open(const Ipp8u* pBuf, size_t buflen);
  JERRCODE Close(void);

  JERRCODE Seek(long offset, int origin);
  JERRCODE Read(void* buf,uic_size_t len,uic_size_t* cnt);
  JERRCODE TellPos(size_t* pos);
  size_t NBytesRead(void) { return m_currpos; }

private:
  JERRCODE Open(vm_char* /*name*/) { return JPEG_NOT_IMPLEMENTED; }

protected:
  const Ipp8u *m_buf;
  size_t m_buflen;
  size_t m_currpos;

};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __MEMBUFFIN_H__

