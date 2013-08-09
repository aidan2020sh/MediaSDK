/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: libmfx_core_operation.h

\* ****************************************************************************** */
#ifndef __LIBMFX_CORE_OPERATOR_H__
#define __LIBMFX_CORE_OPERATOR_H__

#include <vector>
#include <vm_interlocked.h>
#include <umc_mutex.h>
#include <umc_automatic_mutex.h>

class VideoCORE;

class OperatorCORE
{
public:
    OperatorCORE(VideoCORE* pCore):m_refCounter(1)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_Cores.push_back(pCore);
        pCore->SetCoreId(0);
    };

    void AddCore(VideoCORE* pCore)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_Cores.push_back(pCore);
        pCore->SetCoreId((mfxU32)m_Cores.size()-1);
    }
    void RemoveCore(VideoCORE* pCore)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();
        for (;it != m_Cores.end();it++)
        {
            if (*it == pCore)
            {
                m_Cores.erase(it);
                return;
            }
        }
    }

    // functor to run fuction from child cores
    bool  IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface, mfxFrameAllocResponse *response)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        bool sts;
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();

        for (;it != m_Cores.end();it++)
        {
            sts = (*it)->IsOpaqSurfacesAlreadyMapped(pOpaqueSurface, NumOpaqueSurface, response, false);
            if (sts)
                return sts;
        }
        return false;
    }
    
    // functor to run fuction from child cores
    bool CheckOpaqRequest(mfxFrameAllocRequest *request, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        bool sts;
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();

        for (;it != m_Cores.end();it++)
        {
            sts = (*it)->CheckOpaqueRequest(request, pOpaqueSurface, NumOpaqueSurface, false);
            if (!sts)
                return sts;
        }
        return true;
    }

    // functor to run fuction from child cores
    template <typename func, typename arg, typename arg2>
    mfxStatus DoFrameOperation(func functor, arg par, arg2 out)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        mfxStatus sts;
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();

        for (;it != m_Cores.end();it++)
        {
            sts = ((*it)->*functor)(par, out, false);
            // if it is correct Core we can return
            if (MFX_ERR_NONE == sts)
                return sts;
        }
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // functor to run fuction from child cores
    template <typename func, typename arg>
    mfxStatus DoCoreOperation(func functor, arg par)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        mfxStatus sts;
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();

        for (;it != m_Cores.end();it++)
        {
            sts = ((*it)->*functor)(par, false);
            // if it is correct Core we can return
            if (MFX_ERR_NONE == sts)
                return sts;
        }
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // functor to run fuction from child cores
    template <typename func, typename arg>
    mfxFrameSurface1* GetSurface(func functor, arg par)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        mfxFrameSurface1* pSurf;
        std::vector<VideoCORE*>::iterator it = m_Cores.begin();
        for (;it != m_Cores.end();it++)
        {
            pSurf = ((*it)->*functor)(par, false);
            // if it is correct Core we can return
            if (pSurf)
                return pSurf;
        }
        return 0;
    }

    // Increment reference counter of the object.
    virtual
    void AddRef(void)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_refCounter++;
    };
    // Decrement reference counter of the object.
    // If the counter is equal to zero, destructor is called and
    // object is removed from the memory.
    virtual
    void Release(void)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_refCounter--;

        if (0 == m_refCounter)
        {
            delete this;
        }
    };
    // Get the current reference counter value
    virtual
    mfxU32 GetNumRef(void)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        return m_refCounter;
    };
private:

    virtual ~OperatorCORE()
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_Cores.clear();
    };
    // self and child cores
    std::vector<VideoCORE*>  m_Cores;

    // Reference counters
    mfxU32 m_refCounter;

    UMC::Mutex m_guard;

    // Forbid the assignment operator
    OperatorCORE & operator = (const OperatorCORE &);

};
#endif


