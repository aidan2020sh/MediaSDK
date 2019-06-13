// Copyright (c) 2012-2018 Intel Corporation
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

#include "genx_hevce_ime_4mv_hsw_isa.h"

const unsigned char genx_hevce_ime_4mv_hsw[3061] = { 
0x43,0x49,0x53,0x41,0x03,0x01,0x01,0x00,0x07,0x49,0x6d,0x65,0x5f,0x34,0x6d,0x76,
0x2e,0x00,0x00,0x00,0x17,0x08,0x00,0x00,0xbd,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x02,0x45,0x08,0x00,0x00,0xb0,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x43,0x00,
0x00,0x49,0x6d,0x65,0x5f,0x34,0x6d,0x76,0x00,0x41,0x73,0x6d,0x4e,0x61,0x6d,0x65,
0x00,0x53,0x4c,0x4d,0x53,0x69,0x7a,0x65,0x00,0x53,0x75,0x72,0x66,0x61,0x63,0x65,
0x55,0x73,0x61,0x67,0x65,0x00,0x4f,0x75,0x74,0x70,0x75,0x74,0x00,0x4c,0x36,0x00,
0x4c,0x37,0x00,0x4c,0x38,0x00,0x4c,0x39,0x00,0x4c,0x31,0x30,0x00,0x4c,0x31,0x31,
0x00,0x4c,0x31,0x32,0x00,0x4c,0x31,0x33,0x00,0x4c,0x31,0x34,0x00,0x4c,0x31,0x35,
0x00,0x4c,0x31,0x36,0x00,0x4c,0x31,0x37,0x00,0x4c,0x31,0x38,0x00,0x4c,0x31,0x39,
0x00,0x4c,0x32,0x30,0x00,0x4c,0x32,0x31,0x00,0x4c,0x32,0x32,0x00,0x4c,0x32,0x33,
0x00,0x4c,0x32,0x34,0x00,0x4c,0x32,0x35,0x00,0x4c,0x32,0x36,0x00,0x4c,0x32,0x37,
0x00,0x4c,0x32,0x38,0x00,0x4c,0x32,0x39,0x00,0x4c,0x33,0x30,0x00,0x4c,0x33,0x31,
0x00,0x4c,0x33,0x32,0x00,0x4c,0x33,0x33,0x00,0x4c,0x33,0x34,0x00,0x4c,0x33,0x35,
0x00,0x4c,0x33,0x36,0x00,0x4c,0x33,0x37,0x00,0x4c,0x33,0x38,0x00,0x4c,0x33,0x39,
0x00,0x4c,0x34,0x30,0x00,0x4c,0x34,0x31,0x00,0x4c,0x34,0x32,0x00,0x4c,0x34,0x33,
0x00,0x4c,0x34,0x34,0x00,0x4c,0x34,0x35,0x00,0x4c,0x34,0x36,0x00,0x4c,0x34,0x37,
0x00,0x4c,0x34,0x38,0x00,0x4c,0x34,0x39,0x00,0x4c,0x35,0x30,0x00,0x4c,0x35,0x31,
0x00,0x4c,0x35,0x32,0x00,0x4c,0x35,0x33,0x00,0x4c,0x35,0x34,0x00,0x4c,0x35,0x35,
0x00,0x4c,0x35,0x36,0x00,0x4c,0x35,0x37,0x00,0x4c,0x35,0x38,0x00,0x4c,0x35,0x39,
0x00,0x4c,0x36,0x30,0x00,0x4c,0x36,0x31,0x00,0x4c,0x36,0x32,0x00,0x4c,0x36,0x33,
0x00,0x4c,0x36,0x34,0x00,0x4c,0x36,0x35,0x00,0x4c,0x36,0x36,0x00,0x01,0x00,0x32,
0x00,0x0c,0x00,0x12,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x00,0x12,0x01,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x00,0x64,0x40,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x10,0x00,0x64,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,
0x18,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x12,0x00,0x00,0x18,0x00,0x23,0x00,0x00,
0x00,0x00,0x00,0x13,0x00,0x02,0x10,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x14,0x00,
0x02,0x10,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x15,0x00,0x64,0x40,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x16,0x00,0x00,0x10,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x17,
0x00,0x00,0x10,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x08,0x00,0x23,
0x00,0x00,0x00,0x00,0x00,0x19,0x00,0x04,0x20,0x00,0x23,0x00,0x00,0x00,0x00,0x00,
0x1a,0x00,0x04,0x20,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x1b,0x00,0x23,0x02,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x00,0x05,0x20,0x00,0x23,0x00,0x00,0x00,0x00,
0x00,0x1d,0x00,0x41,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0x23,0x02,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0x00,0x23,0x02,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x20,0x00,0x03,0x10,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x21,0x00,0x23,
0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x22,0x00,0x41,0x02,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x23,0x00,0x41,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x24,0x00,
0x23,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0x00,0x13,0x01,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x28,0x00,0x04,0x20,0x00,0x23,0x00,0x20,0x00,0x00,0x00,0x29,
0x00,0x04,0x20,0x00,0x23,0x00,0x20,0x00,0x00,0x00,0x2a,0x00,0x04,0x20,0x00,0x23,
0x00,0x20,0x00,0x00,0x00,0x2b,0x00,0x04,0x20,0x00,0x23,0x00,0x20,0x00,0x00,0x00,
0x2c,0x00,0x04,0x40,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x2d,0x00,0x04,0x20,0x00,
0x23,0x00,0x20,0x00,0x00,0x00,0x2e,0x00,0x04,0x40,0x00,0x22,0x00,0x00,0x00,0x00,
0x00,0x2f,0x00,0x41,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x41,0x02,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x00,0x25,0x02,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x32,0x00,0x21,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x00,0x21,
0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x00,0x53,0x02,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x35,0x00,0x03,0x10,0x00,0x23,0x00,0x00,0x00,0x00,0x00,0x36,0x00,
0x53,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x37,0x00,0x52,0x04,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x38,0x00,0x02,0x10,0x00,0x23,0x00,0x20,0x00,0x00,0x00,0x39,
0x00,0x64,0x20,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x3a,0x00,0x53,0x08,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3b,0x00,0x03,0x08,0x00,0x4b,0x00,0x00,0x00,0x00,0x00,
0x3c,0x00,0x03,0x10,0x00,0x4a,0x00,0x00,0x01,0x00,0x00,0x3d,0x00,0x21,0x01,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x00,0x21,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x3f,0x00,0x00,0x01,0x00,0x4e,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x01,
0x00,0x4f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x25,0x00,0x02,0x00,0x00,
0x26,0x00,0x02,0x00,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x08,
0x00,0x01,0x00,0x0d,0x00,0x00,0x00,0x41,0x00,0x00,0x00,0x42,0x00,0x00,0x00,0x00,
0x03,0x09,0x00,0x01,0x00,0x01,0x04,0x00,0x01,0x00,0x0a,0x00,0x01,0x00,0x01,0x04,
0x00,0x01,0x00,0x0b,0x00,0x01,0x00,0x01,0x04,0x00,0x01,0x00,0x00,0x03,0x02,0x06,
0x00,0x20,0x00,0x04,0x00,0x02,0x07,0x00,0x24,0x00,0x04,0x00,0x02,0x08,0x00,0x28,
0x00,0x04,0x00,0x49,0x04,0x00,0x00,0xce,0x03,0x00,0x00,0x02,0x00,0x02,0x00,0x18,
0x67,0x65,0x6e,0x78,0x5f,0x68,0x65,0x76,0x63,0x65,0x5f,0x69,0x6d,0x65,0x5f,0x34,
0x6d,0x76,0x5f,0x30,0x2e,0x61,0x73,0x6d,0x03,0x00,0x01,0x00,0x31,0x00,0x00,0x31,
0x01,0x00,0x30,0x02,0x00,0x29,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x02,
0x00,0x01,0x00,0x00,0x00,0x21,0x01,0x31,0x03,0x00,0x29,0x00,0x00,0x00,0x00,0x21,
0x00,0x00,0x00,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x21,0x01,0x35,0x02,0x00,0x06,
0x05,0x00,0x00,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x29,0x04,0x00,0x00,0x00,0x24,
0x00,0x00,0x00,0x00,0x02,0x05,0x02,0x00,0x00,0x00,0x00,0x29,0x03,0x00,0x00,0x00,
0x25,0x00,0x02,0x00,0x00,0x02,0x05,0x02,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
0x00,0x26,0x00,0x00,0x05,0x00,0x02,0x00,0x21,0x00,0x00,0x00,0x21,0x01,0x05,0x02,
0x10,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x27,0x00,0x00,0x04,0x00,0x02,0x00,
0x20,0x00,0x00,0x00,0x21,0x01,0x05,0x02,0x10,0x00,0x00,0x00,0x29,0x04,0x00,0x00,
0x00,0x29,0x00,0x00,0x00,0x00,0x02,0x00,0x2a,0x00,0x00,0x00,0x55,0x02,0x29,0x00,
0x00,0x00,0x00,0x2b,0x00,0x00,0x03,0x00,0x02,0x05,0x01,0x00,0x00,0x04,0x77,0x29,
0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x16,0x00,0x02,0x05,0x03,0x30,0x00,0x00,0x00,
0x29,0x00,0x00,0x00,0x00,0x2d,0x00,0x00,0x17,0x00,0x02,0x05,0x03,0x28,0x00,0x00,
0x00,0x29,0x01,0x00,0x00,0x00,0x2e,0x00,0x00,0x00,0x00,0x02,0x00,0x2f,0x00,0x00,
0x16,0x33,0x02,0x01,0x01,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x02,0x00,0x2e,
0x00,0x00,0x00,0x33,0x02,0x05,0x03,0xf0,0xff,0xff,0xff,0x26,0x01,0x00,0x00,0x00,
0x31,0x00,0x00,0x00,0x00,0x02,0x00,0x30,0x00,0x00,0x00,0x33,0x02,0x05,0x01,0x01,
0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x32,0x00,0x00,0x00,0x00,0x02,0x05,0x03,
0xff,0x1f,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x32,0x00,0x00,0x01,0x00,0x02,0x05,
0x03,0xff,0x1f,0x00,0x00,0x29,0x01,0x00,0x00,0x00,0x33,0x00,0x00,0x00,0x00,0x02,
0x05,0x03,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x33,0x00,0x00,0x01,0x00,
0x02,0x00,0x33,0x00,0x00,0x01,0x21,0x01,0x05,0x03,0xfe,0xff,0xff,0xff,0x01,0x01,
0x00,0x00,0x00,0x33,0x00,0x00,0x00,0x00,0x02,0x00,0x33,0x00,0x00,0x00,0x33,0x02,
0x10,0x31,0x00,0x00,0x00,0x33,0x02,0x01,0x01,0x00,0x00,0x00,0x34,0x00,0x00,0x00,
0x00,0x02,0x00,0x32,0x00,0x00,0x00,0x33,0x02,0x10,0x31,0x00,0x00,0x00,0x33,0x02,
0x29,0x01,0x00,0x00,0x00,0x2e,0x00,0x00,0x00,0x00,0x02,0x00,0x2f,0x00,0x00,0x16,
0x33,0x02,0x01,0x01,0x00,0x00,0x00,0x35,0x00,0x00,0x00,0x00,0x02,0x00,0x2e,0x00,
0x00,0x00,0x33,0x02,0x05,0x03,0xf0,0xff,0xff,0xff,0x29,0x01,0x00,0x00,0x00,0x36,
0x00,0x00,0x00,0x00,0x02,0x00,0x32,0x00,0x00,0x00,0x33,0x02,0x01,0x01,0x00,0x00,
0x00,0x37,0x00,0x00,0x00,0x00,0x02,0x00,0x36,0x00,0x00,0x00,0x33,0x02,0x10,0x35,
0x00,0x00,0x00,0x33,0x02,0x2c,0x01,0x04,0x02,0x01,0x00,0x00,0x34,0x00,0x00,0x00,
0x33,0x02,0x05,0x03,0x00,0x00,0x00,0x00,0x29,0x01,0x01,0x00,0x00,0x33,0x00,0x00,
0x00,0x00,0x02,0x00,0x37,0x00,0x00,0x00,0x33,0x02,0x29,0x01,0x00,0x00,0x00,0x34,
0x00,0x00,0x00,0x00,0x02,0x10,0x34,0x00,0x00,0x00,0x33,0x02,0x2c,0x01,0x02,0x02,
0x02,0x00,0x00,0x34,0x00,0x00,0x00,0x33,0x02,0x05,0x03,0x00,0x00,0x00,0x00,0x29,
0x01,0x02,0x00,0x00,0x33,0x00,0x00,0x00,0x00,0x02,0x10,0x32,0x00,0x00,0x00,0x33,
0x02,0x29,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x02,0x00,0x39,0x00,0x00,
0x00,0x21,0x01,0x21,0x00,0x00,0x00,0x00,0x39,0x00,0x00,0x00,0x00,0x02,0x00,0x38,
0x00,0x00,0x00,0x21,0x01,0x05,0x03,0x02,0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,
0x38,0x00,0x00,0x00,0x00,0x02,0x00,0x3a,0x00,0x00,0x00,0x21,0x01,0x21,0x00,0x00,
0x00,0x00,0x3a,0x00,0x00,0x00,0x00,0x02,0x00,0x38,0x00,0x00,0x00,0x21,0x01,0x05,
0x03,0x80,0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x3b,0x00,0x00,0x04,0x00,0x02,
0x05,0x03,0x3f,0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x3c,0x00,0x00,0x09,0x00,
0x02,0x00,0x3d,0x00,0x01,0x18,0x21,0x01,0x29,0x00,0x00,0x00,0x00,0x3e,0x00,0x00,
0x08,0x00,0x02,0x00,0x3f,0x00,0x01,0x19,0x21,0x01,0x29,0x01,0x00,0x00,0x00,0x2e,
0x00,0x00,0x00,0x00,0x02,0x00,0x2f,0x00,0x00,0x16,0x33,0x02,0x01,0x01,0x00,0x00,
0x00,0x40,0x00,0x00,0x00,0x00,0x02,0x00,0x2e,0x00,0x00,0x00,0x33,0x02,0x05,0x03,
0xf0,0xff,0xff,0xff,0x26,0x01,0x00,0x00,0x00,0x41,0x00,0x00,0x00,0x00,0x02,0x00,
0x40,0x00,0x00,0x00,0x33,0x02,0x05,0x01,0x03,0x00,0x00,0x00,0x20,0x01,0x00,0x00,
0x00,0x42,0x00,0x00,0x00,0x00,0x02,0x00,0x41,0x00,0x00,0x00,0x33,0x02,0x05,0x01,
0x0f,0x00,0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x02,0x00,
0x42,0x00,0x00,0x01,0x21,0x01,0x24,0x00,0x00,0x00,0x00,0x43,0x00,0x00,0x00,0x00,
0x02,0x00,0x38,0x00,0x00,0x00,0x21,0x01,0x05,0x03,0x04,0x00,0x00,0x00,0x29,0x00,
0x00,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x02,0x00,0x42,0x00,0x00,0x00,0x21,0x01,
0x21,0x00,0x00,0x00,0x00,0x23,0x00,0x01,0x0a,0x00,0x02,0x00,0x44,0x00,0x00,0x00,
0x21,0x01,0x00,0x43,0x00,0x00,0x00,0x21,0x01,0x29,0x00,0x00,0x00,0x00,0x23,0x00,
0x01,0x1f,0x00,0x02,0x05,0x03,0x01,0x00,0x00,0x00,0x29,0x01,0x00,0x00,0x00,0x45,
0x00,0x00,0x00,0x00,0x02,0x00,0x46,0x00,0x00,0x00,0x33,0x02,0x29,0x01,0x00,0x00,
0x00,0x47,0x00,0x00,0x00,0x00,0x02,0x05,0x03,0x00,0x00,0x00,0x00,0x29,0x02,0x00,
0x00,0x00,0x48,0x00,0x00,0x00,0x00,0x02,0x00,0x49,0x00,0x00,0x08,0x44,0x02,0x54,
0x01,0x00,0x23,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x07,0x45,0x00,0x00,0x00,0x47,
0x00,0x00,0x00,0x48,0x00,0x00,0x00,0x4a,0x00,0x00,0x00,0x24,0x03,0x00,0x00,0x00,
0x4c,0x00,0x00,0x00,0x00,0x02,0x00,0x4d,0x00,0x00,0x08,0x55,0x02,0x05,0x03,0x01,
0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x4e,0x00,0x00,0x00,0x00,0x02,0x00,0x20,
0x00,0x00,0x00,0x21,0x01,0x05,0x02,0x08,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,
0x4f,0x00,0x00,0x00,0x00,0x02,0x00,0x21,0x00,0x00,0x00,0x21,0x01,0x05,0x02,0x02,
0x00,0x00,0x00,0x38,0x00,0x08,0x00,0x08,0x02,0x00,0x50,0x00,0x00,0x00,0x21,0x01,
0x00,0x51,0x00,0x00,0x00,0x21,0x01,0x4b,0x00,0x00,0x00,0x31,0x04,0x00,0x34,0x00,
0x00,0x00,0x31,0x05,0x00,0x05,0x00,0x00,0x00,0x29,0x2d,0xc0,0x22,0x04,0x00,0x00,
0x00,0xff,0x01,0xff,0x01,0x01,0x0d,0x01,0x20,0x07,0x02,0x00,0x00,0x01,0x02,0x00,
0x00,0x61,0x01,0x48,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x02,0x00,
0x00,0x20,0x0c,0x00,0x22,0x20,0x00,0x00,0x00,0x00,0x03,0x28,0x02,0x01,0x00,0x80,
0x00,0x61,0x01,0xa0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,
0x00,0xed,0x01,0xa6,0x21,0x00,0x00,0x00,0x00,0xff,0x1f,0xff,0x1f,0x01,0x00,0x00,
0x00,0xed,0x01,0xa4,0x21,0x00,0x00,0x00,0x00,0xff,0x1f,0xff,0x1f,0x05,0x00,0x00,
0x00,0x29,0x2d,0x2c,0x20,0x06,0x00,0x00,0x00,0xff,0x01,0xff,0x01,0x01,0x00,0x60,
0x00,0x61,0x01,0x40,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x02,0x80,
0x0a,0xad,0x03,0x60,0x20,0x40,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x01,0x00,0x00,
0x00,0xf1,0x01,0xb6,0x40,0x00,0x00,0x00,0x00,0x30,0x00,0x30,0x00,0x01,0x00,0x20,
0x00,0xed,0x01,0xa0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,
0x00,0xf1,0x01,0xb7,0x40,0x00,0x00,0x00,0x00,0x28,0x00,0x28,0x00,0x01,0x00,0x00,
0x00,0xf1,0x01,0xdf,0x40,0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,
0x00,0xf1,0x01,0xc4,0x40,0x00,0x00,0x00,0x00,0x3f,0x00,0x3f,0x00,0x01,0x00,0x00,
0x00,0xe1,0x00,0xac,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x77,0x41,0x00,0x00,
0x00,0x29,0x2d,0xa8,0x20,0xc0,0x02,0x00,0x00,0x10,0x00,0x10,0x00,0x40,0x02,0x00,
0x00,0x20,0x0c,0x00,0x22,0x24,0x00,0x00,0x00,0x00,0xc0,0x98,0x0a,0x06,0x00,0x00,
0x00,0x31,0x3e,0xc0,0x40,0xc0,0x00,0x00,0x00,0x02,0x00,0x02,0x00,0x40,0x00,0x20,
0x00,0xa5,0x3e,0x30,0x20,0xb6,0x00,0x45,0x00,0xf0,0xff,0xf0,0xff,0x0c,0x00,0x20,
0x00,0xad,0x3c,0x40,0x40,0x30,0x00,0x45,0x00,0x01,0x00,0x01,0x00,0x40,0x00,0x20,
0x00,0xa5,0x3e,0x30,0x20,0xb6,0x00,0x45,0x00,0xf0,0xff,0xf0,0xff,0x01,0x00,0x20,
0x00,0xad,0x01,0xa0,0x21,0x40,0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00,
0x00,0xad,0x3d,0xa2,0x20,0xa2,0x00,0x00,0x00,0xfe,0xff,0xfe,0xff,0x40,0x00,0x20,
0x00,0xad,0x15,0x40,0x40,0xa4,0x01,0x45,0x00,0x30,0x40,0x45,0x00,0x40,0x00,0x20,
0x00,0xa5,0x3e,0x30,0x20,0xb6,0x00,0x45,0x00,0xf0,0xff,0xf0,0xff,0x40,0x00,0x20,
0x00,0xad,0x35,0xa0,0x20,0xa0,0x00,0x45,0x00,0xa0,0x41,0x45,0x00,0x40,0x00,0x20,
0x05,0xad,0x35,0x20,0x20,0xa4,0x01,0x45,0x00,0xa0,0x41,0x45,0x00,0x01,0x00,0x20,
0x00,0xad,0x01,0xc0,0x21,0x40,0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x21,
0x00,0xad,0x01,0xa0,0x20,0xc0,0x01,0x45,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x20,
0x03,0xac,0x01,0x00,0x20,0x20,0x40,0x45,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x21,
0x00,0xad,0x01,0xa0,0x20,0xa4,0x41,0x45,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x20,
0x00,0xa5,0x3c,0x40,0x20,0x30,0x00,0x45,0x00,0x03,0x00,0x03,0x00,0x05,0x00,0x20,
0x00,0xb5,0x3c,0xa0,0x61,0x40,0x00,0x45,0x00,0x0f,0x00,0x0f,0x00,0x01,0x16,0x01,
0x20,0x07,0x0b,0x03,0x00,0x01,0x00,0x00,0x00,0x31,0x02,0xc8,0x40,0x99,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x20,0x00,0xb5,0x02,0x60,0x20,0xa0,0x01,0x87,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x31,0x02,0xc9,0x40,0x98,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0xbd,0x03,0x80,0x20,0xa0,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0xad,0x02,0x62,0x20,0x61,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0xa5,0x3d,0x20,0x20,0x62,0x00,0x00,
0x00,0x04,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x31,0x3e,0xc0,0x40,0xc0,0x00,0x00,
0x00,0x80,0x00,0x80,0x00,0x06,0x00,0x00,0x00,0xb1,0x16,0x40,0x60,0x60,0x00,0x00,
0x00,0x20,0x00,0x00,0x00,0x01,0x00,0x20,0x00,0xed,0x01,0x20,0x20,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x41,0x00,0x00,0x00,0x29,0x2d,0xaa,0x20,0x2c,0x00,0x00,
0x00,0x10,0x00,0x10,0x00,0x01,0x00,0x00,0x00,0x31,0x02,0xca,0x40,0x40,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x0d,0x01,0x20,0x07,0x02,0x00,0x00,0x01,0x16,0x01,
0x20,0x07,0x08,0x05,0x00,0x01,0x00,0x20,0x00,0xbd,0x03,0x30,0x21,0xd0,0x00,0x45,
0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x31,0x2e,0x0d,0x41,0x0d,0x01,0x00,
0x00,0xf8,0x00,0xf8,0x00,0x06,0x00,0x00,0x00,0x31,0x2e,0x0d,0x41,0x0d,0x01,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0xbd,0x03,0x04,0x21,0x20,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0xbd,0x03,0x00,0x21,0x80,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x31,0xcb,0x00,0x28,0x00,0x0d,0x08,0x10,0x41,0x00,0x00,
0x00,0x25,0x2d,0x44,0x20,0x2c,0x00,0x00,0x00,0x02,0x00,0x02,0x00,0x01,0x02,0x00,
0x00,0x61,0x00,0x48,0x20,0x00,0x00,0x00,0x00,0x07,0x00,0x01,0x00,0x40,0x02,0x00,
0x00,0x20,0x0c,0x00,0x22,0x28,0x00,0x00,0x00,0x00,0x80,0x0a,0x04,0x41,0x00,0x00,
0x00,0x25,0x2d,0x40,0x20,0xc0,0x02,0x00,0x00,0x08,0x00,0x08,0x00,0x09,0x00,0x60,
0x00,0xad,0x3d,0x60,0x20,0xb0,0x02,0x8d,0x00,0x01,0x00,0x01,0x00,0x31,0x02,0x60,
0x0c,0xa0,0x03,0x00,0x20,0x40,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x01,0x0d,0x01,
0x20,0x07,0x70,0x00,0x00,0x31,0x00,0x00,0x07,0xa0,0x0f,0x00,0x20,0x00,0x0e,0x00,
0x00,0x10,0x00,0x00,0x82
};