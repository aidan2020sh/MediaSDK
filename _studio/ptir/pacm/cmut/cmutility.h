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

#include "cmrt_cross_platform.h"
//#include "cm_rt.h"
#include "iostream"
#include "string"
using namespace std;

#define CM_CASE_RESULT_STR(result)  \
  case result:  \
    return #result;

namespace cmut
{
  inline UINT DivUp(UINT x, UINT y) {
    return (x + y - 1) / y;
  }
  
  inline const char * cm_result_to_str(int result)
  {
    switch (result) {
      CM_CASE_RESULT_STR(CM_SUCCESS)
      CM_CASE_RESULT_STR(CM_FAILURE)
      CM_CASE_RESULT_STR(CM_NOT_IMPLEMENTED)
      CM_CASE_RESULT_STR(CM_OUT_OF_HOST_MEMORY)
      CM_CASE_RESULT_STR(CM_SURFACE_ALLOCATION_FAILURE)
      CM_CASE_RESULT_STR(CM_EXCEED_SURFACE_AMOUNT)
      CM_CASE_RESULT_STR(CM_EXCEED_KERNEL_ARG_AMOUNT)
      CM_CASE_RESULT_STR(CM_EXCEED_KERNEL_ARG_SIZE_IN_BYTE)
      CM_CASE_RESULT_STR(CM_INVALID_ARG_INDEX)
      CM_CASE_RESULT_STR(CM_INVALID_ARG_VALUE)
      CM_CASE_RESULT_STR(CM_INVALID_ARG_SIZE)
      CM_CASE_RESULT_STR(CM_INVALID_THREAD_INDEX)
      CM_CASE_RESULT_STR(CM_INVALID_WIDTH)
      CM_CASE_RESULT_STR(CM_INVALID_HEIGHT)
    default:
      return "CM_RESULT_UNKNOWN";
    }
  }
};

#define BEGIN_TEST_CORE \
  try {

#define END_TEST_CORE \
  } catch (std::exception & exp) {  \
    cout << exp.what () << endl;    \
    testResult = TEST_TARGET_FAIL;  \
  }

#define ISA_FILE_NAME           IsaFileName (string (__FILE__), ".cpp").c_str ()
#define ISA_FILE_NAME2(exeName) IsaFileName (string (exeName), ".exe").c_str ()

#define TEST_TARGET_PASS    0
#define TEST_TARGET_FAIL    -1
#define TEST_TARGET_BLOCK   -2
#define TEST_FAIL           -100

#define SHOW_TEST_RESULT(testResult) \
  do {  \
    switch (testResult) { \
    case TEST_TARGET_PASS:  \
      cout << testResult << ": Test target passes" << endl;  \
      break;  \
    case TEST_TARGET_FAIL:  \
      cout << testResult << ": Test target fails" << endl;  \
      break;  \
    case TEST_TARGET_BLOCK:  \
      cout << testResult << ": Test target blocks (test case aborts)" << endl;  \
      break;  \
    case TEST_FAIL:  \
      cout << testResult << ": Test fails" << endl;  \
      break;  \
    default:  \
      cout << testResult << endl; \
      break;  \
    } \
  } while (false)

template<typename ty, int size>
void write_array(const char * str, ty * data, int highlightId)
{
  cout << str;
  for (int i = 0; i < size; ++i) {
    if (i == highlightId) {
      cout << "[" << data[i] << "], ";
    } else {
      cout << data[i] << ", ";
    }
  }
  cout << endl;
}

template<int size>
void write_array(const char * str, const char * data, int highlightId)
{
  cout << str;
  for (int i = 0; i < size; ++i) {
    if (i == highlightId) {
      cout << "[" << (int)data[i] << "], ";
    } else {
      cout << (int)data[i] << ", ";
    }
  }
  cout << endl;
}

template<int size>
void write_array(const char * str, const unsigned char * data, int highlightId)
{
  cout << str;
  for (int i = 0; i < size; ++i) {
    if (i == highlightId) {
      cout << "[" << (unsigned int)data[i] << "], ";
    } else {
      cout << (unsigned int)data[i] << ", ";
    }
  }
  cout << endl;
}

template<int size, typename ty>
void write(ty * expects, ty * actuals, int highlightId)
{
  write_array<ty, size>("Expect: ", expects, highlightId);
  write_array<ty, size>("Actual: ", actuals, highlightId);
}

template<int size>
void write(const char * expects, const char * actuals, int highlightId)
{
  write_array<size>("Expect: ", expects, highlightId);
  write_array<size>("Actual: ", actuals, highlightId);
}

template<int size>
void write(const unsigned char * expects, const unsigned char * actuals, int highlightId)
{
  write_array<size>("Expect: ", expects, highlightId);
  write_array<size>("Actual: ", actuals, highlightId);
}

#ifdef __GNUC__

// For Linux
#include <stdexcept>
#include <string>

class cm_bad_alloc : public std::runtime_error {
public:
  cm_bad_alloc() 
  : std::runtime_error("cm bad allocation")
      {
      }

  cm_bad_alloc(const char *_Message) 
  : std::runtime_error(_Message)
    {  // construct from message string with no memory allocation
    }
};

#else //For Windows

// Define an exception class derived from std::exception that we can throw with a string (disallowed
// for bad_alloc in the C++11 standard) 
// Use this instead of bad_alloc

// If we're in an environment that doesn't define _THROW0 then provide a default definition
// (probably any non-windows)
#ifndef _THROW0
#define _THROW0 throw()
#endif

class cm_bad_alloc : public std::exception {
public:
  cm_bad_alloc() _THROW0()
  : std::exception("cm bad allocation", 1)
      {
      }

  cm_bad_alloc(const char *_Message) _THROW0()
  : std::exception(_Message, 1)
    {  // construct from message string with no memory allocation
    }
};
#endif