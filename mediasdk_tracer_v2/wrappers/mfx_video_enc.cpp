#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try {
        Log::WriteLog("function: MFXVideoENC_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam* in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if (in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if (out) Log::WriteLog(dump_mfxVideoParam("out", out));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Query) proc)(session, in, out);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Query called");

        Log::WriteLog(dump_mfxSession("session", session));
        if (in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if (out) Log::WriteLog(dump_mfxVideoParam("out", out));

        Log::WriteLog("function: MFXVideoENC_Query(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try {
        Log::WriteLog("function: MFXVideoENC_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if (request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_QueryIOSurf) proc)(session, par, request);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_QueryIOSurf called");

        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if (request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Log::WriteLog("function: MFXVideoENC_QueryIOSurf(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par)
{
    try {
        Log::WriteLog("function: MFXVideoENC_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Init) proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Init called");

        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Log::WriteLog("function: MFXVideoENC_Init(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Reset(mfxSession session, mfxVideoParam *par)
{
    try {
        Log::WriteLog("function: MFXVideoENC_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Reset) proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Reset called");

        Log::WriteLog(dump_mfxSession("session", session));
        if (par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Log::WriteLog("function: MFXVideoENC_Reset(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Close(mfxSession session)
{
    try {
        Log::WriteLog("function: MFXVideoENC_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Close) proc)(session);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Close called");

        Log::WriteLog(dump_mfxSession("session", session));

        Log::WriteLog("function: MFXVideoENC_Close(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp)
{
    try {
        Log::WriteLog("function: MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_ProcessFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if (in) Log::WriteLog(dump_mfxENCInput("in", in));
        if (out) Log::WriteLog(dump_mfxENCOutput("out", out));
        Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_ProcessFrameAsync) proc)(session, in, out, syncp);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_ProcessFrameAsync called");

        Log::WriteLog(dump_mfxSession("session", session));
        if (in) Log::WriteLog(dump_mfxENCInput("in", in));
        if (out) Log::WriteLog(dump_mfxENCOutput("out", out));
        Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        Log::WriteLog("function: MFXVideoENC_ProcessFrameAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
