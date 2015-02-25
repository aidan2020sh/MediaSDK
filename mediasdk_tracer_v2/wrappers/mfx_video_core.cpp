#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

// CORE interface functions
mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator)
{
    try{
        DumpContext *context = new DumpContext();
        context->context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(mfxSession session=" + ToString(session) + ", mfxBufferAllocator *allocator=" + ToString(allocator) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetBufferAllocator_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context->dump("session", session));
        if(allocator) Log::WriteLog(context->dump("allocator", *allocator));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetBufferAllocator) proc) (session, allocator);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetBufferAllocator called");
        Log::WriteLog(context->dump("session", session));
        if(allocator) Log::WriteLog(context->dump("allocator", *allocator));
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(" + elapsed + ", " + context->dump_mfxStatus("status", status) + ") - \n\n");
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
        DumpContext *context = new DumpContext();
        context->context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(mfxSession session=" + ToString(session) + ", mfxFrameAllocator *allocator=" + ToString(allocator) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetFrameAllocator_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context->dump("session", session));
        if(allocator) Log::WriteLog(context->dump("allocator", *allocator));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetFrameAllocator) proc) (session, allocator);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetFrameAllocator called");
        Log::WriteLog(context->dump("session", session));
        if(allocator) Log::WriteLog(context->dump("allocator", *allocator));
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(" + elapsed + ", " + context->dump_mfxStatus("status", status) + ") - \n\n");
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
        DumpContext *context = new DumpContext();
        context->context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoCORE_SetHandle(mfxSession session=" + ToString(session) + ", mfxHandleType type=" + ToString(type) + ", mfxHDL hdl=" + ToString(hdl) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetHandle_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context->dump("session", session));
        Log::WriteLog(context->dump("type", type));
        Log::WriteLog(context->dump_mfxHDL("hdl", &hdl));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_SetHandle) proc) (session, type, hdl);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_SetHandle called");
        Log::WriteLog(context->dump("session", session));
        Log::WriteLog(context->dump("type", type));
        Log::WriteLog(context->dump_mfxHDL("hdl", &hdl));
        Log::WriteLog("function: MFXVideoCORE_SetHandle(" + elapsed + ", " + context->dump_mfxStatus("status", status) + ") - \n\n");
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
        DumpContext *context = new DumpContext();
        context->context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoCORE_GetHandle(mfxSession session=" + ToString(session) + ", mfxHandleType type=" + ToString(type) + ", mfxHDL *hdl=" + ToString(hdl) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_GetHandle_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context->dump("session", session));
        Log::WriteLog(context->dump("type", type));
        Log::WriteLog(context->dump_mfxHDL("hdl", hdl));

        Timer t;
        mfxStatus status = (*(fMFXVideoCORE_GetHandle) proc) (session, type, hdl);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoCORE_GetHandle called");
        Log::WriteLog(context->dump("session", session));
        Log::WriteLog(context->dump("type", type));
        Log::WriteLog(context->dump_mfxHDL("hdl", hdl));
        Log::WriteLog("function: MFXVideoCORE_GetHandle(" + elapsed + ", " + context->dump_mfxStatus("status", status) + ") - \n\n");
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
        if (Log::GetLogLevel() >= LOG_LEVEL_FULL) //call function with logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
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

            mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SyncOperation_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            Log::WriteLog(context.dump("session", session));
            Log::WriteLog(context.dump("syncp", syncp));
            Log::WriteLog(context.dump_mfxU32("wait", wait));
            
            Timer t;
            mfxStatus status = (*(fMFXVideoCORE_SyncOperation) proc) (session, sp->syncPoint, wait);
            std::string elapsed = TimeToString(t.GetTime());

            Log::WriteLog(">> MFXVideoCORE_SyncOperation called");
            Log::WriteLog(context.dump("session", session));
            Log::WriteLog(context.dump("syncp", sp->syncPoint));
            Log::WriteLog(context.dump_mfxU32("wait", wait));
            Log::WriteLog("function: MFXVideoCORE_SyncOperation(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
            
            return status;
        }
        else // call function without logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            TracerSyncPoint *sp = (TracerSyncPoint*)syncp;
            if (!sp->syncPoint) {
                // already synced
                return MFX_ERR_NONE;
            }
            
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SyncOperation_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            
            mfxStatus status = (*(fMFXVideoCORE_SyncOperation) proc) (session, sp->syncPoint, wait);
            
            return status;
        }
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
