// Copyright (c) 2019-2020 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#pragma once
#include "av1ehw_ddi.h"

#if defined(_DEBUG)
    #define DDI_TRACE
#endif

namespace AV1EHW
{

typedef enum tagENCODER_TYPE
{
    ENCODER_DEFAULT = 0
    , ENCODER_REXT
    , ENCODER_SCC
} ENCODER_TYPE;

#ifdef DDI_TRACE
class DDITracer
{
private:
    FILE* m_log;
    ENCODER_TYPE m_type;
public:
    DDITracer(ENCODER_TYPE type = ENCODER_DEFAULT);
    ~DDITracer();

    template<class T> void Trace(T const &, mfxU32) {};
    template<class T> void TraceArray(T const * b, mfxU32 n) {
        if (b)
        for (mfxU32 i = 0; i < n; i ++) Trace(b[i], i);
    };
    inline void TraceArray(void const *, mfxU32) {};

#if !defined(MFX_VA_LINUX)
    void Trace(ENCODE_COMPBUFFERDESC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_AV1 const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_AV1 const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SLICE_HEADER_AV1 const & b, mfxU32 idx);
    void Trace(ENCODE_PACKEDHEADER_DATA const & b, mfxU32 idx);
    void Trace(ENCODE_EXECUTE_PARAMS const & b, mfxU32 idx);
    void Trace(ENCODE_QUERY_STATUS_PARAMS const & b, mfxU32 idx);
    void Trace(ENCODE_QUERY_STATUS_SLICE_PARAMS const & b, mfxU32 idx);

    void Trace(D3D11_VIDEO_DECODER_EXTENSION const & b, mfxU32 idx);

    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_AV1_REXT const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SLICE_HEADER_AV1_REXT const & b, mfxU32 idx);

    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_AV1_SCC const & b, mfxU32 idx);
#endif

    inline void Trace(GUID const & guid, mfxU32) { TraceGUID(guid, m_log); };
    void Trace(const char* name, mfxU32 value);
    template<mfxU32 N> inline void Trace(const char name[N], mfxU32 value) { Trace((const char*) name, value);  }

    static void TraceGUID(GUID const & guid, FILE*);
    void Trace(ENCODE_CAPS_AV1 const & b, mfxU32 idx);
};
#else
class DDITracer
{
public:
    DDITracer(ENCODER_TYPE type = ENCODER_DEFAULT) { type; };
    ~DDITracer(){};
    template<class T> inline void Trace(T const &, mfxU32) {};
    template<class T> inline void TraceArray(T const *, mfxU32) {};
    static void TraceGUID(GUID const &, FILE*) {};
    void Trace(ENCODE_CAPS_AV1 const &, mfxU32) {};
};
#endif

}
#endif