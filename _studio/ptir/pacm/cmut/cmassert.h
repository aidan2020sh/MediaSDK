// Copyright (c) 1985-2018 Intel Corporation
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

#pragma once

#include "infrastructure.h"
#include "cmutility.h"
#include "math.h"

template<bool> struct assert_static;
template<> struct assert_static<true> {};

#define INT2STRING2(i) #i
#define INT2STRING(i)  INT2STRING2(i)

//#ifndef __LINUX__
//  #define CMUT_EXCEPTION2(message, exception) exception(__FILE__ ": " __FUNCTION__ "(" INT2STRING(__LINE__) "): error: " message);
//  #define CMUT_EXCEPTION(message) std::exception((std::string(__FILE__ ": " __FUNCTION__ "(" INT2STRING(__LINE__) "): error: ") + std::string(message)).c_str());
//#else
  #define CMUT_EXCEPTION2(message, exception) exception();
  #define CMUT_EXCEPTION(message) std::exception();
//#endif

//FIXME need better name
#define CM_FAIL_IF(condition, result) \
  do {  \
    if (condition) {  \
    throw CMUT_EXCEPTION(cmut::cm_result_to_str(result)); \
    } \
  } while (false)

#define CMUT_ASSERT_EQUAL(expect, actual, message)  \
  if (expect != (actual)) {                           \
    throw CMUT_EXCEPTION(message);                 \
  }
#define CMUT_ASSERT(condition)                        \
  if (!(condition)) {                                 \
    throw CMUT_EXCEPTION("Expect : " #condition);    \
  }
#define CMUT_ASSERT_MESSAGE(condition, message)       \
  if (!(condition)) {                                 \
    throw CMUT_EXCEPTION(message);                   \
  }

template<typename ty, unsigned int size>
class CmAssertEqual
{
public:
  typedef ty elementTy;
  enum { elementSize = size };
public:
  template<typename ty2>
  CmAssertEqual(const ty2 * pExpects) : dataView(pExpects)
  {
  }

  void Assert(const void * pData) const 
  {
    const ty * pActuals = (const ty *)pData;

    for (unsigned int i = 0; i < size; ++i) {
      if (!IsEqual(dataView[i], pActuals[i])) {
        write<size>((const ty *)dataView, pActuals, i);

        throw CMUT_EXCEPTION("Difference is detected");
      }
    }
  }
  
  template<typename ty2>
  bool IsEqual(ty2 x, ty2 y) const
  {
    return x == y;
  }

  bool IsEqual(float x, float y) const
  {
    return fabs(x - y) <= 1e-5;
  }

  bool IsEqual(double x, double y) const
  {
    return fabs(x - y) <= 1e-5;
  }

protected:
  data_view<ty, size> dataView;
};