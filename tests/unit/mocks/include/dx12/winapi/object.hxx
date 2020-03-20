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

#include <d3d12.h>

namespace mocks { namespace dx12 { namespace winapi
{
    template <typename T>
    struct object_base_impl
        : T
    {
        HRESULT GetPrivateData(REFGUID, UINT*, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateData(REFGUID, UINT, const void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateDataInterface(REFGUID, const IUnknown*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetName(LPCWSTR) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

    template <typename T>
    struct object_impl
        : object_base_impl<T>
    {};

    template <typename ...T>
    struct object_impl<unknown_impl<T...> >
        : object_base_impl<unknown_impl<T...> >
    {
        object_impl()
        {
            mock_unknown<ID3D12Object>(*this);
        }
    };

} } }