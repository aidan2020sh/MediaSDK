/* ****************************************************************************** *\

Copyright (C) 2012-2014 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: main.cpp

\* ****************************************************************************** */

#include "mfx_dispatcher.h"
#include "mfx_load_dll.h"
#include "mfx_dispatcher_log.h"
#include "mfx_library_iterator.h"
#include "mfx_critical_section.h"

#include <string.h> /* for memset on Linux */
#include <memory>
#include <stdlib.h> /* for qsort on Linux */
#include "mfx_load_plugin.h"
#include "mfx_plugin_hive.h"

// module-local definitions
namespace
{

    const
    struct
    {
        // instance implementation type
        eMfxImplType implType;
        // real implementation
        mfxIMPL impl;
        // adapter numbers
        mfxU32 adapterID;

    } implTypes[] =
    {
        // MFX_IMPL_AUTO case
        {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE,  0},
        {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE,  0},

        // MFX_IMPL_ANY case
        {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE,  0},
        {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE2, 1},
        {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE3, 2},
        {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE4, 3},
        {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE,  0},
        {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO,  0}
    };

    const
    struct
    {
        // start index in implTypes table for specified implementation
        mfxU32 minIndex;
        // last index in implTypes table for specified implementation
        mfxU32 maxIndex;

    } implTypesRange[] =
    {    
        {0, 1},  // MFX_IMPL_AUTO    
        {1, 1},  // MFX_IMPL_SOFTWARE    
        {0, 0},  // MFX_IMPL_HARDWARE    
        {2, 6},  // MFX_IMPL_AUTO_ANY    
        {2, 5},  // MFX_IMPL_HARDWARE_ANY    
        {3, 3},  // MFX_IMPL_HARDWARE2    
        {4, 4},  // MFX_IMPL_HARDWARE3    
        {5, 5},  // MFX_IMPL_HARDWARE4
        {2, 6},  // MFX_IMPL_RUNTIME, same as MFX_IMPL_HARDWARE_ANY 
        {7, 7}   // MFX_IMPL_AUDIO
    };

    MFX::mfxCriticalSection dispGuard = 0;

} // namespace

using namespace MFX;
//
// Implement DLL exposed functions. MFXInit and MFXClose have to do
// slightly more than other. They require to be implemented explicitly.
// All other functions are implemented implicitly.
//

typedef MFXVector<MFX_DISP_HANDLE*> HandleVector;
typedef MFXVector<mfxStatus>        StatusVector;

struct VectorHandleGuard
{
    VectorHandleGuard(HandleVector& aVector): m_vector(aVector) {}
    ~VectorHandleGuard()
    {
        HandleVector::iterator it = m_vector.begin(), 
                               et = m_vector.end();
        for ( ; it != et; ++it)
        {
            delete *it;
        }
    }

    HandleVector& m_vector;
private:
    void operator=(const VectorHandleGuard&);
};


int HandleSort (const void * plhs, const void * prhs)
{
    const MFX_DISP_HANDLE * lhs = *(const MFX_DISP_HANDLE **)plhs;
    const MFX_DISP_HANDLE * rhs = *(const MFX_DISP_HANDLE **)prhs;

    if (lhs->apiVersion < rhs->apiVersion) 
    {
        return -1;
    }
    if (rhs->apiVersion < lhs->apiVersion) 
    {
        return 1;
    }

    // if versions are equal prefer library with HW 
    if (lhs->loadStatus == MFX_WRN_PARTIAL_ACCELERATION && rhs->loadStatus == MFX_ERR_NONE)
    {
        return 1;
    }
    if (lhs->loadStatus == MFX_ERR_NONE && rhs->loadStatus == MFX_WRN_PARTIAL_ACCELERATION)
    {
        return -1;
    }

    return 0;
}

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXInit)(mfxIMPL impl, mfxVersion *pVer, mfxSession *session)
{
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    DISPATCHER_LOG_BLOCK( ("MFXInit (impl=%s, pVer=%d.%d session=0x%p\n"
        , DispatcherLog_GetMFXImplString(impl).c_str()
        , (pVer)? pVer->Major : 0
        , (pVer)? pVer->Minor : 0
        , session));

    mfxStatus mfxRes;
    HandleVector allocatedHandle;
    VectorHandleGuard handleGuard(allocatedHandle);

    MFX_DISP_HANDLE *pHandle;
    msdk_disp_char dllName[MFX_MAX_DLL_PATH];
    MFX::MFXLibraryIterator libIterator;

    // there iterators are used only if the caller specified implicit type like AUTO
    mfxU32 curImplIdx, maxImplIdx;
    // particular implementation value
    mfxIMPL curImpl;
    // implementation method masked from the input parameter
    // special case for audio library
    const mfxIMPL implMethod = (impl & MFX_IMPL_AUDIO)?(sizeof(implTypesRange)/sizeof(implTypesRange[0])-1):(impl & (MFX_IMPL_VIA_ANY - 1));

    // implementation interface masked from the input parameter
    mfxIMPL implInterface = impl & -MFX_IMPL_VIA_ANY;
    mfxIMPL implInterfaceOrig = implInterface;
    mfxVersion requiredVersion = {{MFX_VERSION_MINOR, MFX_VERSION_MAJOR}};

    // check error(s)
    if (NULL == session)
    {
        return MFX_ERR_NULL_PTR;
    }
    if (((MFX_IMPL_AUTO > implMethod) || (MFX_IMPL_RUNTIME < implMethod)) && !(impl & MFX_IMPL_AUDIO))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // set the minimal required version
    if (pVer)
    {
        requiredVersion = *pVer;
    }

    try
    {
        // reset the session value
        *session = 0;

        // allocate the dispatching handle and call-table
        pHandle = new MFX_DISP_HANDLE(requiredVersion);
    }
    catch(...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    DISPATCHER_LOG_OPERATION({
        if (pVer)
        {
            DISPATCHER_LOG_INFO((("Required API version is %u.%u\n"), pVer->Major, pVer->Minor));
        }
        else
        {
            DISPATCHER_LOG_INFO((("Default API version is %u.%u\n"), DEFAULT_API_VERSION_MAJOR, DEFAULT_API_VERSION_MINOR));
        }
    });

    // main query cycle
    curImplIdx = implTypesRange[implMethod].minIndex;
    maxImplIdx = implTypesRange[implMethod].maxIndex;

    do
    {
        int currentStorage = MFX::MFX_STORAGE_ID_FIRST;
        implInterface = implInterfaceOrig;
        do
        {
            // initialize the library iterator
            mfxRes = libIterator.Init(implTypes[curImplIdx].implType, 
                implInterface,
                implTypes[curImplIdx].adapterID,
                currentStorage);

            // look through the list of installed SDK version,
            // looking for a suitable library with higher merit value.
            if (MFX_ERR_NONE == mfxRes)
            {

                if (
                    MFX_LIB_HARDWARE == implTypes[curImplIdx].implType
                    && (!implInterface 
                    || MFX_IMPL_VIA_ANY == implInterface))
                {
                    implInterface = libIterator.GetImplementationType();
                }

                do
                {
                    eMfxImplType implType;

                    // select a desired DLL
                    mfxRes = libIterator.SelectDLLVersion(dllName,
                        sizeof(dllName) / sizeof(dllName[0]),
                        &implType,
                        pHandle->apiVersion);
                    if (MFX_ERR_NONE != mfxRes)
                    {
                        break;
                    }
                    DISPATCHER_LOG_INFO((("loading library %S\n"), MSDK2WIDE(dllName)));

                    // try to load the selected DLL
                    curImpl = implTypes[curImplIdx].impl;
                    mfxRes = pHandle->LoadSelectedDLL(dllName, implType, curImpl, implInterface);
                    // unload the failed DLL
                    if (MFX_ERR_NONE != mfxRes)
                    {
                        pHandle->Close();
                    }
                    else
                    {
                        // Have to check application folder for alternative runtime
                        libIterator.GetSubKeyName(pHandle->subkeyName, sizeof(pHandle->subkeyName)/sizeof(pHandle->subkeyName[0])) ;
                        pHandle->storageID = libIterator.GetStorageID();
                        allocatedHandle.push_back(pHandle);
                        pHandle = new MFX_DISP_HANDLE(requiredVersion);
                    }

                } while (MFX_ERR_NONE != mfxRes);
            }

            // select another registry key
            currentStorage += 1;

        } while ((MFX_ERR_NONE != mfxRes) && (MFX::MFX_STORAGE_ID_LAST >= currentStorage));

    } while ((MFX_ERR_NONE != mfxRes) && (++curImplIdx <= maxImplIdx));


    curImplIdx = implTypesRange[implMethod].minIndex;
    maxImplIdx = implTypesRange[implMethod].maxIndex;

    do
    {
        implInterface = implInterfaceOrig;
        // initialize the library iterator
        mfxRes = libIterator.Init(implTypes[curImplIdx].implType, 
            implInterface,
            implTypes[curImplIdx].adapterID,
            MFX::MFX_APP_FOLDER);

        // look through the list of installed SDK version,
        // looking for a suitable library with higher merit value.
        if (MFX_ERR_NONE == mfxRes)
        {

            if (
                MFX_LIB_HARDWARE == implTypes[curImplIdx].implType
                && (!implInterface 
                || MFX_IMPL_VIA_ANY == implInterface))
            {
                implInterface = libIterator.GetImplementationType();
            }

            do
            {
                eMfxImplType implType;

                // select a desired DLL
                mfxRes = libIterator.SelectDLLVersion(dllName,
                    sizeof(dllName) / sizeof(dllName[0]),
                    &implType,
                    pHandle->apiVersion);
                if (MFX_ERR_NONE != mfxRes)
                {
                    break;
                }
                DISPATCHER_LOG_INFO((("loading library %S\n"), MSDK2WIDE(dllName)));

                // try to load the selected DLL
                curImpl = implTypes[curImplIdx].impl;
                mfxRes = pHandle->LoadSelectedDLL(dllName, implType, curImpl, implInterface);
                // unload the failed DLL
                if (MFX_ERR_NONE != mfxRes)
                {
                    pHandle->Close();
                }
                else
                {
                    // Have to check application folder for alternative runtime
                    pHandle->storageID = libIterator.GetStorageID();
                    allocatedHandle.push_back(pHandle);
                    pHandle = new MFX_DISP_HANDLE(requiredVersion);
                }

            } while (MFX_ERR_NONE != mfxRes);
        }
    } while ((MFX_ERR_NONE != mfxRes) && (++curImplIdx <= maxImplIdx));

    // use the legacy default DLL search mechanism     
    // if hard-coded software library's path and app folder from the registry fails - try all system folders
     if (allocatedHandle.size() == 0)
     {
        // set current library index again
        curImplIdx = implTypesRange[implMethod].minIndex;
        do
        {
            implInterface = implInterfaceOrig;
            
            if (impl & MFX_IMPL_AUDIO)
            {
                mfxRes = MFX::mfx_get_default_audio_dll_name(dllName,
                    sizeof(dllName) / sizeof(dllName[0]),
                    implTypes[curImplIdx].implType);
            }
            else
            {
                mfxRes = MFX::mfx_get_default_dll_name(dllName,
                    sizeof(dllName) / sizeof(dllName[0]),
                    implTypes[curImplIdx].implType);
            }

            if (MFX_ERR_NONE == mfxRes)
            {
                DISPATCHER_LOG_INFO((("loading default library %S\n"), MSDK2WIDE(dllName)))

                    // try to load the selected DLL using default DLL search mechanism
                    if (MFX_LIB_HARDWARE == implTypes[curImplIdx].implType)
                    {
                        if (!implInterface) 
                        {
                            implInterface = MFX_IMPL_VIA_ANY;
                        }
                        mfxRes = MFX::GetImplementationType(implTypes[curImplIdx].adapterID, &implInterface, NULL, NULL);
                    }
                    if (MFX_ERR_NONE == mfxRes)
                    {
                        // try to load the selected DLL using default DLL search mechanism
                        mfxRes = pHandle->LoadSelectedDLL(dllName,
                            implTypes[curImplIdx].implType,
                            implTypes[curImplIdx].impl,
                            implInterface);
                    }
                    // unload the failed DLL
                    if ((MFX_ERR_NONE != mfxRes) &&
                        (MFX_WRN_PARTIAL_ACCELERATION != mfxRes))
                    {
                        pHandle->Close();
                    }
                    else
                    {                    
                        pHandle->storageID = MFX::MFX_UNKNOWN_KEY;
                        allocatedHandle.push_back(pHandle);
                        pHandle = new MFX_DISP_HANDLE(requiredVersion);
                    }
            }
        }
        while ((MFX_ERR_NONE > mfxRes) && (++curImplIdx <= maxImplIdx));
    }
     delete pHandle;

    if (allocatedHandle.size() == 0)
        return MFX_ERR_NOT_FOUND;

    // select dll with version with lowest version number still greater or equal to requested
    qsort(&(*allocatedHandle.begin()), allocatedHandle.size(), sizeof(MFX_DISP_HANDLE*), &HandleSort);
    HandleVector::iterator candidate = allocatedHandle.begin();

    // check the final result of loading
    try 
    {
        pHandle = *candidate;
        //pulling up current mediasdk version, that required to match plugin version
        mfxVersion apiVerActual;
        mfxStatus stsQueryVersion;
        stsQueryVersion = MFXQueryVersion((mfxSession)pHandle, &apiVerActual);
        if (MFX_ERR_NONE !=  stsQueryVersion) 
        {
            DISPATCHER_LOG_ERROR((("MFXQueryVersion returned: %d, cannot load plugins\n"), mfxRes))
        }
        else
        {
            MFX::MFXPluginStorage & hive = pHandle->pluginHive;

            HandleVector::iterator it = allocatedHandle.begin(),
                                   et = allocatedHandle.end();
            for (; it != et; ++it)
            {
                // Registering default plugins set
                MFX::MFXDefaultPlugins defaultPugins(apiVerActual, *it, (*it)->implType);
                hive.insert(hive.end(), defaultPugins.begin(), defaultPugins.end());

                if ((*it)->storageID != MFX::MFX_UNKNOWN_KEY)
                {
                    // Scan HW plugins in subkeys of registry library
                    MFX::MFXPluginsInHive plgsInHive((*it)->storageID, (*it)->subkeyName, apiVerActual);
                    hive.insert(hive.end(), plgsInHive.begin(), plgsInHive.end());
                }
            }

            //setting up plugins records
            for(int i = MFX::MFX_STORAGE_ID_FIRST; i <= MFX::MFX_STORAGE_ID_LAST; i++) 
            {
                MFX::MFXPluginsInHive plgsInHive(i, NULL, apiVerActual);
                hive.insert(hive.end(), plgsInHive.begin(), plgsInHive.end());
            }

            MFX::MFXPluginsInFS plgsInFS(apiVerActual);
            hive.insert(hive.end(), plgsInFS.begin(), plgsInFS.end());
        }
    }
    catch(...)
    {
        DISPATCHER_LOG_ERROR((("unknown exception while loading plugins\n")))
    }

    // everything is OK. Save pointers to the output variable
    *candidate = 0; // keep this one safe from guard destructor 
    *((MFX_DISP_HANDLE **) session) = pHandle;

    return pHandle->loadStatus;

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXClose)(mfxSession session)
{
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;

    // check error(s)
    if (pHandle)
    {
        try
        {
            // unload the DLL library
            mfxRes = pHandle->Close();

            // it is possible, that there is an active child session.
            // can't unload library in that case.
            if (MFX_ERR_UNDEFINED_BEHAVIOR != mfxRes)
            {
                // release the handle
                delete pHandle;
            }
        }
        catch(...)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }

    return mfxRes;

} // mfxStatus MFXClose(mfxSession session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXJoinSession)(mfxSession session, mfxSession child_session)
{
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;
    MFX_DISP_HANDLE *pChildHandle = (MFX_DISP_HANDLE *) child_session;

    // get the function's address and make a call
    if ((pHandle) && (pChildHandle) && (pHandle->apiVersion == pChildHandle->apiVersion))
    {
        /* check whether it is audio session or video */
        int tableIndex = eMFXJoinSession; 
        mfxFunctionPointer pFunc;
        if (pHandle->impl & MFX_IMPL_AUDIO) 
        { 
            pFunc = pHandle->callAudioTable[tableIndex];
        }
        else
        {
            pFunc = pHandle->callTable[tableIndex];
        }

        if (pFunc)
        {
            // pass down the call
            mfxRes = (*(mfxStatus (MFX_CDECL *) (mfxSession, mfxSession)) pFunc) (pHandle->session, 
                pChildHandle->session);
        }
    }

    return mfxRes;

} // mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXCloneSession)(mfxSession session, mfxSession *clone)
{
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session;
    mfxVersion apiVersion;
    mfxIMPL impl;

    // check error(s)
    if (pHandle)
    {
        // initialize the clone session
        apiVersion = pHandle->apiVersion;
        impl = pHandle->impl | pHandle->implInterface;
        mfxRes = MFXInit(impl, &apiVersion, clone);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // join the sessions
        mfxRes = MFXJoinSession(session, *clone);
        if (MFX_ERR_NONE != mfxRes)
        {
            MFXClose(*clone);
            *clone = NULL;
            return mfxRes;
        }
    }

    return mfxRes;

} // mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)



mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXVideoUSER_Load)(mfxSession session, const mfxPluginUID *uid, mfxU32 version) 
{
    MFX_DISP_HANDLE &pHandle = *(MFX_DISP_HANDLE *) session;
    if (!&pHandle)
    {
        DISPATCHER_LOG_ERROR((("MFXVideoUSER_Load: session=NULL\n")));
        return MFX_ERR_NULL_PTR;
    }
    if (!uid)
    {
        DISPATCHER_LOG_ERROR((("MFXVideoUSER_Load: uid=NULL\n")));
        return MFX_ERR_NULL_PTR;
    }
    DISPATCHER_LOG_INFO((("MFXVideoUSER_Load: uid="MFXGUIDTYPE()" version=%d\n")
        , MFXGUIDTOHEX(uid)
        , version))
        size_t pluginsChecked = 0;
    PluginDescriptionRecord defaultPluginRecord;
    for (MFX::MFXPluginStorage::iterator i = pHandle.pluginHive.begin();i != pHandle.pluginHive.end(); i++, pluginsChecked++)
    {
        if (i->PluginUID != *uid)
        {
            if (i->Default) // PluginUID == 0 for default set
            {
                defaultPluginRecord = *i;
            }
            continue;
        }
        //check rest in records
        if (i->PluginVersion < version)
        {
            DISPATCHER_LOG_INFO((("MFXVideoUSER_Load: registered \"Plugin Version\" for GUID="MFXGUIDTYPE()" is %d, that is smaller that requested\n")
                , MFXGUIDTOHEX(uid)
                , i->PluginVersion))
                continue;
        }
        try {
            return pHandle.pluginFactory.Create(*i);
        }
        catch(...) {
            continue;
        }
    }

    // Specified UID was not found among individually registed plugins, now try load it from default set if any
    if (defaultPluginRecord.Default)
    {
        defaultPluginRecord.PluginUID = *uid;
        defaultPluginRecord.PluginVersion = (mfxU16)version;
        try {
            return pHandle.pluginFactory.Create(defaultPluginRecord);
        }
        catch(...) {
            return MFX_ERR_UNKNOWN;
        }
    }

    DISPATCHER_LOG_ERROR((("MFXVideoUSER_Load: cannot find registered plugin with requested UID, total plugins available=%d\n"), pHandle.pluginHive.size()));
    return MFX_ERR_NOT_FOUND;
}

mfxStatus DISPATCHER_EXPOSED_PREFIX(MFXVideoUSER_UnLoad)(mfxSession session, const mfxPluginUID *uid) 
{
    MFX_DISP_HANDLE &rHandle = *(MFX_DISP_HANDLE *) session;
    if (!&rHandle) 
    {
        DISPATCHER_LOG_ERROR((("MFXVideoUSER_UnLoad: session=NULL\n")));
        return MFX_ERR_NULL_PTR;
    }
    if (!uid)
    {
        DISPATCHER_LOG_ERROR((("MFXVideoUSER_Load: uid=NULL\n")));
        return MFX_ERR_NULL_PTR;
    }

    bool bDestroyed = rHandle.pluginFactory.Destroy(*uid);
    if (bDestroyed) 
    {
        DISPATCHER_LOG_INFO((("MFXVideoUSER_UnLoad : plugin with GUID="MFXGUIDTYPE()" unloaded\n"), MFXGUIDTOHEX(uid)));
    } else 
    {
        DISPATCHER_LOG_ERROR((("MFXVideoUSER_UnLoad : plugin with GUID="MFXGUIDTYPE()" not found\n"), MFXGUIDTOHEX(uid)));
    }

    return bDestroyed ? MFX_ERR_NONE : MFX_ERR_NOT_FOUND;
}

//
// implement all other calling functions.
// They just call a procedure of DLL library from the table.
//

// define for common functions (from mfxsession.h)
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value DISPATCHER_EXPOSED_PREFIX(func_name) formal_param_list \
{ \
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE; \
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session; \
    /* get the function's address and make a call */ \
    if (pHandle) \
{ \
    /* check whether it is audio session or video */ \
    int tableIndex = e##func_name; \
    mfxFunctionPointer pFunc; \
    if (pHandle->impl & MFX_IMPL_AUDIO) \
{ \
    pFunc = pHandle->callAudioTable[tableIndex]; \
} \
        else \
{ \
    pFunc = pHandle->callTable[tableIndex]; \
} \
    if (pFunc) \
{ \
    /* get the real session pointer */ \
    session = pHandle->session; \
    /* pass down the call */ \
    mfxRes = (*(mfxStatus (MFX_CDECL  *) formal_param_list) pFunc) actual_param_list; \
} \
} \
    return mfxRes; \
}

FUNCTION(mfxStatus, MFXQueryIMPL, (mfxSession session, mfxIMPL *impl), (session, impl))
    FUNCTION(mfxStatus, MFXQueryVersion, (mfxSession session, mfxVersion *version), (session, version))
    FUNCTION(mfxStatus, MFXDisjoinSession, (mfxSession session), (session))
    FUNCTION(mfxStatus, MFXSetPriority, (mfxSession session, mfxPriority priority), (session, priority))
    FUNCTION(mfxStatus, MFXGetPriority, (mfxSession session, mfxPriority *priority), (session, priority))

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value DISPATCHER_EXPOSED_PREFIX(func_name) formal_param_list \
{ \
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE; \
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session; \
    /* get the function's address and make a call */ \
    if (pHandle) \
{ \
    mfxFunctionPointer pFunc = pHandle->callTable[e##func_name]; \
    if (pFunc) \
{ \
    /* get the real session pointer */ \
    session = pHandle->session; \
    /* pass down the call */ \
    mfxRes = (*(mfxStatus (MFX_CDECL  *) formal_param_list) pFunc) actual_param_list; \
} \
} \
    return mfxRes; \
}

#include "mfx_exposed_functions_list.h"
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value DISPATCHER_EXPOSED_PREFIX(func_name) formal_param_list \
{ \
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE; \
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *) session; \
    /* get the function's address and make a call */ \
    if (pHandle) \
{ \
    mfxFunctionPointer pFunc = pHandle->callAudioTable[e##func_name]; \
    if (pFunc) \
{ \
    /* get the real session pointer */ \
    session = pHandle->session; \
    /* pass down the call */ \
    mfxRes = (*(mfxStatus (MFX_CDECL  *) formal_param_list) pFunc) actual_param_list; \
} \
} \
    return mfxRes; \
}

#include "mfxaudio_exposed_functions_list.h"
