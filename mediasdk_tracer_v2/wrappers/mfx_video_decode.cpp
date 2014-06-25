#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

// DECODE interface functions
mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam *in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("in", in));
        Log::WriteLog(dump("out", out));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_Query) proc) (session, in, out);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_Query called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("in", in));
        Log::WriteLog(dump("out", out));
        Log::WriteLog("function: MFXVideoDECODE_Query(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_DecodeHeader(mfxSession session=" + ToString(session) + ", mfxBitstream *bs=" + ToString(bs) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeHeader];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_DecodeHeader) proc) (session, bs, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_DecodeHeader called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoDECODE_DecodeHeader(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog(dump("request", request));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_QueryIOSurf) proc) (session, par, request);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_QueryIOSurf called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog(dump("request", request));
        Log::WriteLog("function: MFXVideoDECODE_QueryIOSurf(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_Init) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_Init called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoDECODE_Init(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_Reset) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_Reset called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoDECODE_Reset(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_Close) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_Close called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog("function: MFXVideoDECODE_Close(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetVideoParam(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_GetVideoParam) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_GetVideoParam called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoDECODE_GetVideoParam(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetDecodeStat(mfxSession session=" + ToString(session) + ", mfxDecodeStat *stat=" + ToString(stat) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetDecodeStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("stat", stat));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_GetDecodeStat) proc) (session, stat);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_GetDecodeStat called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("stat", stat));
        Log::WriteLog("function: MFXVideoDECODE_GetDecodeStat(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_SetSkipMode(mfxSession session=" + ToString(session) + ", mfxSkipMode mode=" + ToString(mode) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_SetSkipMode];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("mode", mode));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_SetSkipMode) proc) (session, mode);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_SetSkipMode called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("mode", mode));
        Log::WriteLog("function: MFXVideoDECODE_SetSkipMode(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetPayload(mfxSession session=" + ToString(session) + ", mfxU64 *ts=" + ToString(ts) + ", mfxPayload *payload=" + ToString(payload) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetPayload];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU64("ts", (*ts)));
        Log::WriteLog(dump("payload", payload));

        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_GetPayload) proc) (session, ts, payload);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoDECODE_GetPayload called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU64("ts", (*ts)));
        Log::WriteLog(dump("payload", payload));
        Log::WriteLog("function: MFXVideoDECODE_GetPayload(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    try{
        TracerSyncPoint * sp = new TracerSyncPoint();
        sp->syncPoint = (*syncp);
        sp->component = DECODE;

        Log::WriteLog("function: MFXVideoDECODE_DecodeFrameAsync(mfxSession session=" + ToString(session) + ", mfxBitstream *bs=" + ToString(bs) + ", mfxFrameSurface1 *surface_work=" + ToString(surface_work) + ", mfxFrameSurface1 **surface_out=" + ToString(surface_out) + ", mfxSyncPoint *syncp=" + ToString(syncp) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("surface_work", surface_work));
        if(surface_out && (*surface_out))
            Log::WriteLog(dump("surface_out", (*surface_out)));
        Log::WriteLog(dump("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(fMFXVideoDECODE_DecodeFrameAsync) proc) (session, bs, surface_work, surface_out, &sp->syncPoint);
        std::string elapsed = TimeToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog(">> MFXVideoDECODE_DecodeFrameAsync called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("surface_work", surface_work));
        if(surface_out && (*surface_out))
            Log::WriteLog(dump("surface_out", (*surface_out)));
        Log::WriteLog(dump("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoDECODE_DecodeFrameAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
