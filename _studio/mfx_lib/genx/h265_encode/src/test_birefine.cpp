// Copyright (c) 2015-2018 Intel Corporation
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

#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#pragma warning(disable : 4505)
#include "cm_rt.h"
#include "../include/test_common.h"

int TestBiRefine32x32();
int TestBiRefine64x64();

typedef int (*TestFuncPtr)();

void RunTest(TestFuncPtr testFunc, char * kernelName)
{
    printf("%s... ", kernelName);
    try {
        int res = testFunc();
        if (res == PASSED)
            printf("passed\n");
        else if (res == FAILED)
            printf("FAILED!\n");
    }
    catch (std::exception & e) {
        printf("EXCEPTION: %s\n", e.what());
    }
    catch (int***) {
        printf("UNKNOWN EXCEPTION\n");
    }
}

int main()
{
    RunTest(TestBiRefine32x32, "TestBiRefine32x32");
//    RunTest(TestBiRefine64x64, "TestBiRefine64x64");
}