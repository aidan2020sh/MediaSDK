#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

// CORE interface functions
mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(mfxSession session=" + ToString(session) + ", mfxBufferAllocator *allocator=" + ToString(allocator) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetBufferAllocator];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("allocator", allocator));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetBufferAllocator) proc) (session, allocator);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetBufferAllocator called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("allocator", allocator));
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(mfxSession session=" + ToString(session) + ", mfxFrameAllocator *allocator=" + ToString(allocator) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetFrameAllocator];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("allocator", allocator));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetFrameAllocator) proc) (session, allocator);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetFrameAllocator called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("allocator", allocator));
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetHandle(mfxSession session=" + ToString(session) + ", mfxHandleType type=" + ToString(type) + ", mfxHDL hdl=" + ToString(hdl) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetHandle];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", &hdl));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetHandle) proc) (session, type, hdl);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetHandle called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", &hdl));
        Log::WriteLog("function: MFXVideoCORE_SetHandle(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_GetHandle(mfxSession session=" + ToString(session) + ", mfxHandleType type=" + ToString(type) + ", mfxHDL *hdl=" + ToString(hdl) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_GetHandle];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", hdl));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_GetHandle) proc) (session, type, hdl);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_GetHandle called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", hdl));
        Log::WriteLog("function: MFXVideoCORE_GetHandle(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    try{
        TracerSyncPoint *sp = (TracerSyncPoint*)syncp;
        if (!sp->syncPoint) {
            // already synced
            Log::WriteLog("Already synced");
            return MFX_ERR_NONE;
        }
        Log::WriteLog("function: MFXVideoCORE_SyncOperation(mfxSession session=" + ToString(session) + ", mfxSyncPoint syncp=" + ToString(syncp) + ", mfxU32 wait=" + ToString(wait) + ") +");
        if(sp) sp->component == ENCODE ? Log::WriteLog("SyncOperation(ENCODE," + TimeToString(sp->timer.GetTime()) + ")") : (sp->component == DECODE ? Log::WriteLog("SyncOperation(DECODE, " + ToString(sp->timer.GetTime()) + " sec)") : Log::WriteLog("SyncOperation(VPP, "  + ToString(sp->timer.GetTime()) + " sec)"));

        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SyncOperation];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("syncp", &syncp));
        Log::WriteLog(dump_mfxU32("wait", wait));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SyncOperation) proc) (session, sp->syncPoint, wait);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SyncOperation called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("syncp", &sp->syncPoint));
        Log::WriteLog(dump_mfxU32("wait", wait));
        Log::WriteLog("function: MFXVideoCORE_SyncOperation(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
