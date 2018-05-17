/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once
#include <map>
#include <list>
#include <set>
#include <mutex>
#include <memory.h>

//#define BS_MEM_TRACE

#ifdef BS_MEM_TRACE
#include <stdio.h>
#endif

namespace BS_MEM
{

class MemBase
{
public:
    virtual void* Get() = 0;
    virtual ~MemBase(){}
};

template<class T> class MemObj : public MemBase
{
public:
    T* m_obj;
    bool m_del;

    MemObj(unsigned int count, bool zero)
    {
        m_obj = new T[count];
        m_del = true;

        if (zero)
            memset(m_obj, 0, sizeof(T) * count);
    }

    MemObj(T* obj)
        : m_obj(obj)
        , m_del(false)
    {}

    virtual ~MemObj()
    {
        if (m_del)
            delete[] m_obj;
    }

    void* Get() { return m_obj; }
};

class MemD
{
public:
    MemD()
        : locked(0)
        , to_delete(false)
        , mem(nullptr)
    {
    }

    ~MemD()
    {
        if (mem)
            delete mem;
    }

    unsigned int locked;
    bool to_delete;
    std::set<void*> base;
    std::set<void*> dep;
    MemBase* mem;
};

class Allocator
{
private:
    std::map<void*, MemD> m_mem;
    std::recursive_mutex m_mtx;
    bool m_zero;

    inline bool Touch(void* p) { return !!m_mem.count(p); }
    inline void __notrace(const char*, ...) {}

#ifdef BS_MEM_TRACE
#define BS_MEM_TRACE_F printf
#else
#define BS_MEM_TRACE_F __notrace
#endif

public:

    Allocator()
        : m_zero(false)
    {
    }

    ~Allocator()
    {
    }

    void SetZero(bool zero)
    {
        std::unique_lock<std::recursive_mutex> _lock(m_mtx);
        m_zero = zero;
    }

    bool touch(void* p)
    {
        std::unique_lock<std::recursive_mutex> _lock(m_mtx);
        BS_MEM_TRACE_F("BS_MEM::touch(%p)\n", p);
        return Touch(p);
    }

    void bound(void* dep, void* base)
    {
        std::unique_lock<std::recursive_mutex> _lock(m_mtx);
        BS_MEM_TRACE_F("BS_MEM::bound(%p, %p)\n", dep, base);

        if (!Touch(base) || !Touch(dep))
            throw std::bad_alloc();

        m_mem[base].dep.insert(dep);
        m_mem[dep].base.insert(base);
    }

    template<class T> T* alloc(void* base = nullptr, unsigned int count = 1)
    {
        if (count == 0)
            return nullptr;

        std::unique_lock<std::recursive_mutex> _lock(m_mtx);

        MemBase* pObj = new MemObj<T>(count, m_zero);
        T* p = (T*)pObj->Get();

        m_mem[p].mem = pObj;

        BS_MEM_TRACE_F("BS_MEM::alloc(%p, %d) = %p\n", base, count, p);
        if (base)
            bound(p, base);

        return p;
    }

    template<class T> T* alloc_nozero(void* base = nullptr, unsigned int count = 1)
    {
        if (count == 0)
            return nullptr;

        std::unique_lock<std::recursive_mutex> _lock(m_mtx);

        MemBase* pObj = new MemObj<T>(count, false);
        T* p = (T*)pObj->Get();

        m_mem[p].mem = pObj;

        BS_MEM_TRACE_F("BS_MEM::alloc_nozero(%p, %d) = %p\n", base, count, p);
        if (base)
            bound(p, base);

        return p;
    }

    void free(void* p)
    {
        std::unique_lock<std::recursive_mutex> _lock(m_mtx);
        BS_MEM_TRACE_F("BS_MEM::free(%p)", p);

        if (!Touch(p))
            throw std::bad_alloc();

        auto& d = m_mem[p];

        if (d.locked)
        {
            BS_MEM_TRACE_F(" - delayed\n", p);
            d.to_delete = true;
            return;
        }

        if (!d.base.empty())
        {
            BS_MEM_TRACE_F(" - delayed\n", p);
            return;
        }

        BS_MEM_TRACE_F(" - done\n", p);

        for (auto& pdep : d.dep)
        {
            auto& ddep = m_mem[pdep];

            ddep.base.erase(p);

            if (ddep.base.empty())
                free(pdep);
        }

        m_mem.erase(p);
    }

    void lock(void* p)
    {
        std::unique_lock<std::recursive_mutex> _lock(m_mtx);
        BS_MEM_TRACE_F("BS_MEM::lock(%p)\n", p);

        if (!Touch(p))
            throw std::bad_alloc();

        m_mem[p].locked++;
    }

    void unlock(void* p)
    {
        BS_MEM_TRACE_F("BS_MEM::unlock(%p)\n", p);

        if (!p)
            return;

        std::unique_lock<std::recursive_mutex> _lock(m_mtx);

        if (!Touch(p))
            throw std::bad_alloc();

        auto& d = m_mem[p];

        if (!m_mem[p].locked)
            throw std::bad_alloc();

        d.locked--;

        if (d.to_delete)
            free(p);
    }
#undef BS_MEM_TRACE_F
};

class AutoLock
{
public:
    Allocator* m_pAllocator;
    std::list<void*> m_ptr;

    AutoLock(Allocator* pAllocator, void* ptr = 0)
        : m_pAllocator(pAllocator)
    {
        if (m_pAllocator && ptr)
        {
            m_pAllocator->lock(ptr);
            m_ptr.push_back(ptr);
        }
    }

    ~AutoLock()
    {
        if (m_pAllocator)
        {
            for (auto p : m_ptr)
                m_pAllocator->unlock(p);
        }
    }
    
    void Add(void* p)
    {
        if (m_pAllocator)
        {
            m_pAllocator->lock(p);
            m_ptr.push_back(p);
        }
        else
            throw std::bad_alloc();
    }
};

};