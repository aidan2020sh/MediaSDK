// Copyright (c) 2019 Intel Corporation
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

#include "mocks/include/unknown.h"

#include <d3d9.h>

namespace mocks { namespace dx9 { namespace winapi
{
    struct query_impl
        : unknown_impl<IDirect3DQuery9>
    {
        HRESULT GetDevice(IDirect3DDevice9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3DQUERYTYPE GetType() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        DWORD GetDataSize() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Issue(DWORD /*dwIssueFlags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetData(void* /*pData*/, DWORD /*dwSize*/, DWORD /*dwGetDataFlags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }