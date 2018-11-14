// Copyright (c) 2003-2018 Intel Corporation
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

#include "ippcore.h"

#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
#pragma warning (disable:1478)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    // touch unreferenced parameters
    hModule = hModule;
    lpReserved = lpReserved;

    // initialize the IPP library
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ippInit();
        break;

    default:
        break;
    }

    return TRUE;

} // BOOL APIENTRY DllMain(HMODULE hModule,

#else // #if defined(_WIN32) || defined(_WIN64)
void __attribute__ ((constructor)) dll_init(void)
{
    ippStaticInit();
}
#endif // #if defined(_WIN32) || defined(_WIN64)
