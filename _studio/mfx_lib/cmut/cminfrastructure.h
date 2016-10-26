//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "cmrt_cross_platform.h"

#include "assert.h"

namespace mdfut {
#if defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#include "winbase.h"

class Clock
{
public:
  Clock()
    : duration(0), count(0)
  {
    QueryPerformanceFrequency(&freq);
  }

  bool Begin()
  {
    return !!QueryPerformanceCounter(&begin);
  }

  bool End()
  {
    if (!(!!QueryPerformanceCounter(&end))) {
      return false;
    }

    double time = (double)(end.QuadPart - begin.QuadPart) / (double)freq.QuadPart * 1000;
    duration += time;

    ++count;

    return true;
  }

  double Duration() const { return duration; }
  int Count() const { return count; }

protected:
  LARGE_INTEGER freq;
  LARGE_INTEGER begin;
  LARGE_INTEGER end;

  double duration; // ms
  int count;
};
#endif

#if defined(LINUX32) || defined (LINUX64)
class Clock
{
public:
  Clock()
    : duration(0), count(0)
  {
  }

  bool Begin()
  {
    return true;
  }

  bool End()
  {
    return true;
  }

  double Duration() const { return duration; }
  int Count() const { return count; }

protected:
  int freq;
  int begin;
  int end;

  double duration; // ms
  int count;
};
#endif
};

template<typename ty>
struct pointer_traits
{
  enum { dim = 0 };
  typedef ty tail_pointee_type;
};

template<typename ty, int n>
#ifdef __GNUC__
struct pointer_traits<ty [n]>
#else
struct ::pointer_traits<ty [n]>
#endif
{
  enum { dim = pointer_traits<ty>::dim + 1 };
  typedef typename pointer_traits<ty>::tail_pointee_type tail_pointee_type;
};

template<typename ty>
#ifdef __GNUC__
struct pointer_traits<ty *>
#else
struct ::pointer_traits<ty *>
#endif
{
  enum { dim = pointer_traits<ty>::dim + 1 };
  typedef typename pointer_traits<ty>::tail_pointee_type tail_pointee_type;
};

template<bool b>
struct bool2type {};

struct null_type;

template<typename type_list, unsigned int i>
struct type_at;

template<typename headTy, typename tailTy = null_type>
struct type_list
{
  template<unsigned int i>
  struct at
  {
    typedef typename type_at<type_list, i>::type type;
  };

  enum { size = 1 + tailTy::size };
};

template<typename headTy>
struct type_list<headTy, null_type>
{
  template<unsigned int i>
  struct at
  {
    typedef typename type_at<type_list, i>::type type;
  };

  enum { size = 1 };
};

template<>
struct type_list<null_type, null_type>
{
  enum { size = 0 };
};

#define type_list_1(t0)             type_list<t0>
#define type_list_2(t0, t1)         type_list<t0, type_list_1 (t1)>
#define type_list_3(t0, t1, t2)     type_list<t0, type_list_2 (t1, t2)>
#define type_list_4(t0, t1, t2, t3) type_list<t0, type_list_3 (t1, t2, t3)>

template<typename headTy, typename tailTy, unsigned int i>
struct type_at<type_list<headTy, tailTy>, i>
{
  typedef typename type_at<tailTy, i - 1>::type type;
};

template<typename headTy, typename tailTy>
struct type_at<type_list<headTy, tailTy>, 0>
{
  typedef headTy type;
};

template<typename headTy, unsigned int i>
struct type_at<type_list<headTy, null_type>, i>;

template<typename headTy>
struct type_at<type_list<headTy, null_type>, 0>
{
  typedef headTy type;
};

template<unsigned int i>
struct type_at<type_list<null_type, null_type>, i>;

template<typename ty, unsigned int size>
class data_view
{
protected:
  data_view(const data_view &);
  data_view & operator = (const data_view &);
public:
  template<typename ty2>
  explicit data_view (ty2 * pData)
  {
    this->pDatas = new ty[size];

    typedef typename ::pointer_traits<ty2>::tail_pointee_type tail_pointee_type;
    for (unsigned int i = 0; i < size; i++) {
      this->pDatas[i] = (ty)((tail_pointee_type *)pData)[i];
    }
  }

  template<typename ty2>
  explicit data_view (ty2 data)
  {
    this->pDatas = new ty[size];

    for (unsigned int i = 0; i < size; i++) {
      this->pDatas[i] = (ty)data;
    }
  }

  ~data_view()
  {
    delete []pDatas;
  }

  ty operator [] (unsigned int index) const { return pDatas[index]; }

  template<typename ty2>
  operator const ty2 * () const { return (const ty2 *)pDatas; }

protected:
  //ty datas[size];
  ty * pDatas;
};

//class typeless_pointer
//{
//public:
//  typeless_pointer (void * pPointer) : pTypelessPointer (pPointer) {}
//
//  virtual ~typeless_pointer () = 0 {}
//protected:
//  void * pTypelessPointer;
//};
//
//template<typename ty>
//class typed_pointer : public typeless_pointer
//{
//public:
//  typed_pointer (ty * pPointer) : pTypedPointer (pPointer) {}
//
//  virtual ~typed_pointer ()
//  {
//    delete pTypedPointer;
//  }
//
//protected:
//  ty * pTypedPointer;
//};

//template<typename headTy, typename tailTy>
//struct value_list
//{
//  template<unsigned int i>
//  struct at
//  {
//      typedef typename tailTy::at<i - 1>::type type;
//  };
//
//  template<>
//  struct at<0>
//  {
//    typedef headTy type;
//  };
//
//  template<unsigned int i>
//  typename at<0>::type value ()
//  {
//    return headValue;
//  }
//
//  template<unsigned int i>
//  typename at<i>::type value ()
//  {
//    return tailValue.value<i - 1> ();
//  }
//
//  headTy headValue;
//  tailTy tailValue;
//};
