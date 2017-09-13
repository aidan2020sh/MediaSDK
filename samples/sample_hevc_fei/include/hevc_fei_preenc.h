/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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

#ifndef __SAMPLE_FEI_PREENC_H__
#define __SAMPLE_FEI_PREENC_H__

#include "sample_hevc_fei_defs.h"

class IPreENC
{
public:
    IPreENC(MfxVideoParamsWrapper& preenc_pars);
    virtual ~IPreENC() {}

    virtual mfxStatus Init() = 0;
    virtual mfxStatus QueryIOSurf(mfxFrameAllocRequest* request) = 0;

    // prepare required internal resources (e.g. buffer allocation) for component initialization
    virtual mfxStatus PreInit();

    const MfxVideoParamsWrapper& GetVideoParam();
    mfxFrameInfo* GetFrameInfo();

    virtual mfxStatus PreEncFrame(HevcTask * task) = 0;

protected:
    mfxStatus ResetExtBuffers(const MfxVideoParamsWrapper & videoParams);

protected:
    MfxVideoParamsWrapper m_videoParams;

    std::vector<mfxExtFeiPreEncMVExtended>     m_mvs;
    std::vector<mfxExtFeiPreEncMBStatExtended> m_mbs;

private:
    DISALLOW_COPY_AND_ASSIGN(IPreENC);
};

class FEI_Preenc : public IPreENC
{
public:
    FEI_Preenc(MFXVideoSession* session, MfxVideoParamsWrapper& preenc_pars,
        const msdk_char* mvoutFile, const msdk_char* mbstatoutFile);
    ~FEI_Preenc();

    mfxStatus Init();
    mfxStatus Reset(mfxU16 width = 0, mfxU16 height = 0);
    mfxStatus QueryIOSurf(mfxFrameAllocRequest* request);

    // prepare required internal resources (e.g. buffer allocation) for component initialization
    mfxStatus PreInit();

    mfxStatus PreEncFrame(HevcTask * task);

private:
    mfxStatus PreEncMultiFrames(HevcTask * task);
    mfxStatus PreEncOneFrame(HevcTask & currTask, const RefIdxPair & refFramesIdx, const bool bDownsampleInput);

    mfxStatus DumpResult(HevcTask* task);

private:
    MFXVideoSession* m_pmfxSession;
    MFXVideoENC      m_mfxPREENC;

    typedef std::pair<mfxSyncPoint, std::pair<mfxENCInputWrap, mfxENCOutputWrap> > SyncPair;
    SyncPair m_syncp;

    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB m_default_MVMB;

    /* For I/O operations with extension buffers */
    std::auto_ptr<FileHandler> m_pFile_MV_out;
    std::auto_ptr<FileHandler> m_pFile_MBstat_out;

    DISALLOW_COPY_AND_ASSIGN(FEI_Preenc);

};

#endif // __SAMPLE_FEI_PREENC_H__
