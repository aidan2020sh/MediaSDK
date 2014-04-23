/**             
***
*** Copyright  (C) 1985-2014 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
**/ 
#pragma once

//#include "cmrt_cross_platform.h"
#include "cm_rt.h"
#include "cmdeviceex.h"
#include "iostream"

template<typename T> 
class CmSurface2DExT
{
public:
  CmSurface2DExT(CmDeviceEx & deviceEx, unsigned int width, unsigned int height, CM_SURFACE_FORMAT format)
    : format(format), deviceEx(deviceEx), height(height), malloced(false)
  {
#ifdef _DEBUG
    std::cout << "CmSurface2DExT (" << width << ", " << height << ")" << std::endl;
#endif
    Construct(width, height, format);
  }

  void Construct(unsigned int width, unsigned int height, CM_SURFACE_FORMAT format)
  {
    assert (format == CM_SURFACE_FORMAT_A8R8G8B8 || format == CM_SURFACE_FORMAT_A8);
    if (format != CM_SURFACE_FORMAT_A8R8G8B8 && format != CM_SURFACE_FORMAT_A8) {
      throw CMUT_EXCEPTION("fail"); 
    }

    pCmSurface2D = deviceEx.CreateSurface2D(width, height, format);

    this->format = format;
    this->height = height;
    switch (format) {
      case CM_SURFACE_FORMAT_A8R8G8B8:
        this->width = (width * 4) / sizeof(T);
        break;
      case CM_SURFACE_FORMAT_A8:
        this->width = width / sizeof(T);
        break;
      default:
        throw CMUT_EXCEPTION("fail");
        break;
    }

    pData = new T[width * height];
    malloced = true;
  }

  void Destruct()
  {
    if (pCmSurface2D != NULL) {
      deviceEx->DestroySurface(pCmSurface2D);
      pCmSurface2D = NULL;
    }
    
    if (malloced) {
      delete []pData;
      pData = NULL;
      malloced = false;
    }
  }
  //CmSurface2DExT (CmDeviceEx & deviceEx, const unsigned char * pData, unsigned int width, unsigned int height, CM_SURFACE_FORMAT format);
  ~CmSurface2DExT()
  {
    Destruct ();
  }

  operator CmSurface2D & () { return *pCmSurface2D; }
  CmSurface2D * operator -> () { return pCmSurface2D; }

  void Read(void * pSysMem, CmEvent* pEvent = NULL)
  {
    assert (pSysMem != NULL);
    if (pSysMem == NULL) {
      throw CMUT_EXCEPTION("fail"); 
    }

    int result = pCmSurface2D->ReadSurface((unsigned char *)pSysMem, pEvent);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  //void Read (void * pSysMem, const unsigned int stride, CmEvent* pEvent = NULL);

  void Write(void * pSysMem)
  {
    assert (pSysMem != NULL);
    if (pSysMem == NULL) {
      throw CMUT_EXCEPTION("fail"); 
    }

    int result = pCmSurface2D->WriteSurface((const unsigned char *)pSysMem, NULL);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  void Resize(unsigned int width, unsigned int height)
  {   
    Destruct();

    Construct(width, height, format);
  }

  T & Data(int id)
  {
    return pData[id];
  }
  T & Data(int row, int col)
  {
    return pData[row * width + col];
  }

  T * DataPtr() const { return pData; }
  T * DataPtr(int row) const { return pData + row * width; }

  void CommitWrite()
  {
    this->Write(pData);
  }

  void CommitRead()
  {
    this->Read(pData);
  }

  const unsigned int Height () const { return height; }
  const unsigned int Width () const { return width; }
  const unsigned int Pitch () const { return width * sizeof(T); }

protected:
  CmSurface2D * pCmSurface2D;
  CmDeviceEx & deviceEx;

  T * pData;
  bool malloced;

  unsigned int width;// in elements
  unsigned int height;
  CM_SURFACE_FORMAT format;
};
