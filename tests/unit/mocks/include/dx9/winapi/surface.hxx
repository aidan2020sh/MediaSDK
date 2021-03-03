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

#pragma once

#include "resource.hxx"

namespace mocks { namespace dx9 { namespace winapi
{
    struct surface_impl
        : resource_impl<unknown_impl<IDirect3DSurface9> >
    {
        HRESULT GetContainer(REFIID, void** /*ppContainer*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDesc(D3DSURFACE_DESC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT LockRect(D3DLOCKED_RECT*, CONST RECT*, DWORD /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT UnlockRect() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDC(HDC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ReleaseDC(HDC) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
 
} } }
