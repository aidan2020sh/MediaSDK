#include "mfx_video_encode.h"
#include "mfx_structures.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

// ENCODE interface functions
mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam *in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("in", in));
        Log::WriteLog(dump_mfxVideoParam("out", out));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Query called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("in", in));
        Log::WriteLog(dump_mfxVideoParam("out", out));
        Log::WriteLog("function: MFXVideoENCODE_Query(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_QueryIOSurf called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog(dump_mfxFrameAllocRequest("request", request));
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Init called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoENCODE_Init(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");

        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Reset called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoENCODE_Reset(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Close called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXVideoENCODE_Close(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetVideoParam called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(mfxSession session=" + ToString(session) + ", mfxEncodeStat *stat=" + ToString(stat) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetEncodeStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxEncodeStat("stat", stat));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxEncodeStat *stat)) proc) (session, stat);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetEncodeStat called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxEncodeStat("stat", stat));
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    try{
        TracerSyncPoint * sp = new TracerSyncPoint();
        sp->syncPoint = (*syncp);
        sp->component = ENCODE;

        Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(mfxSession session=" + ToString(session) + ", mfxEncodeCtrl *ctrl=" + ToString(ctrl) + ", mfxFrameSurface1 *surface=" + ToString(surface) + ", mfxBitstream *bs=" + ToString(bs) + ", mfxSyncPoint *syncp=" + ToString(syncp) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_EncodeFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxEncodeCtrl("ctrl", ctrl));
        Log::WriteLog(dump_mfxFrameSurface1("surface", surface));
        Log::WriteLog(dump_mfxBitstream("bs", bs));
        Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)) proc) (session, ctrl, surface, bs, &sp->syncPoint);
        std::string elapsed = TimeToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog(">> MFXVideoENCODE_EncodeFrameAsync called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxEncodeCtrl("ctrl", ctrl));
        Log::WriteLog(dump_mfxFrameSurface1("surface", surface));
        Log::WriteLog(dump_mfxBitstream("bs", bs));
        Log::WriteLog(dump_mfxSyncPoint("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
