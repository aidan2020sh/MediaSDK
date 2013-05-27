/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2012 Intel Corporation. All Rights Reserved.

File Name: libmfxsw.cpp

\* ****************************************************************************** */

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfx_check_hardware_support.h>
#include <mfx_trace.h>

#include <ippcore.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tchar.h>
#endif

// static section of the file
namespace
{

void* g_hModule = NULL; // DLL handle received in DllMain

} // namespace

#if !defined(_WIN32) && !defined(_WIN64)
/* These string constants set Media SDK version information for Linux, Android, OSX. */
#ifndef MFX_FILE_VERSION
    #define MFX_FILE_VERSION "0.0.0.0"
#endif
#ifndef MFX_PRODUCT_VERSION
    #define MFX_PRODUCT_VERSION "0.0.000.0000"
#endif

const char* g_MfxProductName = "mediasdk_product_name: Intel(r) Media SDK";
const char* g_MfxCopyright = "mediasdk_copyright: Copyright(c) 2007-2012 Intel Corporation";
const char* g_MfxFileVersion = "mediasdk_file_version: " MFX_FILE_VERSION;
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PRODUCT_VERSION;
#endif

mfxStatus MFXInit(mfxIMPL implParam, mfxVersion *ver, mfxSession *session)
{
    mfxSession pSession = 0;
    mfxVersion libver;
    mfxStatus mfxRes;
    int adapterNum = 0;
    mfxIMPL impl = implParam & (MFX_IMPL_VIA_ANY - 1);
    mfxIMPL implInterface = implParam & -MFX_IMPL_VIA_ANY;

#if defined(MFX_TRACE_ENABLE) && defined(MFX_VA)
    MFX_TRACE_INIT(NULL, (mfxU8)MFX_TRACE_OUTPUT_REG);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "ThreadName=MSDK app");
    }
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXInit");
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API, "^ModuleHandle^libmfx=", "%p", g_hModule);
#endif // defined(MFX_TRACE_ENABLE) && defined(MFX_VA)

    // check error(s)
    if ((MFX_IMPL_AUTO != impl) &&
        (MFX_IMPL_AUTO_ANY != impl) &&
#ifdef MFX_VA
        (MFX_IMPL_HARDWARE_ANY != impl) &&
        (MFX_IMPL_HARDWARE != impl) &&
        (MFX_IMPL_HARDWARE2 != impl) &&
        (MFX_IMPL_HARDWARE3 != impl) &&
        (MFX_IMPL_HARDWARE4 != impl))
#else // !MFX_VA
        (MFX_IMPL_SOFTWARE != impl))
#endif // MFX_VA
    {
        return MFX_ERR_UNSUPPORTED;
    }

#ifdef MFX_VA
    if ((0 != implInterface) &&
        (MFX_IMPL_VIA_D3D11 != implInterface) &&
        (MFX_IMPL_VIA_D3D9 != implInterface) &&
        (MFX_IMPL_VIA_ANY != implInterface))
    {
        return MFX_ERR_UNSUPPORTED;
    }
#endif // MFX_VA


    // set the adapter number
    switch (impl)
    {
    case MFX_IMPL_HARDWARE2:
        adapterNum = 1;
        break;

    case MFX_IMPL_HARDWARE3:
        adapterNum = 2;
        break;

    case MFX_IMPL_HARDWARE4:
        adapterNum = 3;
        break;

    default:
        adapterNum = 0;
        break;

    }

#ifdef MFX_VA
    // the hardware is not supported,
    // or the vendor is not Intel.
    //if (MFX_HW_UNKNOWN == MFX::GetHardwareType())
    //{
    //    // if it is a hardware library,
    //    // reject everything.
    //    // Do not work under any curcumstances.
    //    return MFX_ERR_UNSUPPORTED;
    //}
#endif // MFX_VA

    try
    {
        // reset output variable
        *session = 0;

        // create new session instance
        pSession = new _mfxSession(adapterNum);
        mfxRes = pSession->Init(implInterface, ver);

        // check the library version
        if (ver)
        {
            MFXQueryVersion(pSession, &libver);

            // check the numbers
            if ((libver.Major != ver->Major) ||
                (libver.Minor < ver->Minor))
            {
                mfxRes = MFX_ERR_UNSUPPORTED;
            }
        }
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE != mfxRes &&
        MFX_WRN_PARTIAL_ACCELERATION != mfxRes)
    {
        if (pSession)
        {
            delete pSession;
        }

        return mfxRes;
    }

    // save the handle
    *session = pSession;

    return mfxRes;

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxHDL *session)

mfxStatus MFXClose(mfxSession session)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXClose");
    mfxStatus mfxRes = MFX_ERR_NONE;

    // check error(s)
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    try
    {
        // parent session can't be closed,
        // because there is no way to let children know about parent's death.
        if (session->IsParentSession())
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        // child session should be uncoupled from the parent before closing.
        if (session->IsChildSession())
        {
            mfxRes = MFXDisjoinSession(session);
            if (MFX_ERR_NONE != mfxRes)
            {
                return mfxRes;
            }
        }

        // deallocate the object
        delete session;
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }
#if defined(MFX_TRACE_ENABLE) && defined(MFX_VA)
    MFX_TRACE_CLOSE();
#endif
    return mfxRes;

} // mfxStatus MFXClose(mfxHDL session)

#if defined(_WIN32) || defined(_WIN64)
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    // touch unreferenced parameters
    g_hModule = hModule;
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
