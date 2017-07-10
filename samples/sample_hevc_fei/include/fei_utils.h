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

#ifndef __FEI_UTILS_H__
#define __FEI_UTILS_H__

#include "sample_defs.h"
#include "sample_utils.h"
#include "base_allocator.h"
#include "sample_hevc_fei_defs.h"

class SurfacesPool
{
public:
    SurfacesPool(MFXFrameAllocator* allocator = NULL);
    ~SurfacesPool();

    mfxStatus AllocSurfaces(mfxFrameAllocRequest& request);
    void SetAllocator(MFXFrameAllocator* allocator);
    mfxFrameSurface1* GetFreeSurface();
    mfxStatus LockSurface(mfxFrameSurface1* pSurf);
    mfxStatus UnlockSurface(mfxFrameSurface1* pSurf);

private:
    MFXFrameAllocator* m_pAllocator;
    std::vector<mfxFrameSurface1> m_pool;
    mfxFrameAllocResponse m_response;
};

class IVideoReader
{
public:
    IVideoReader(const sInputParams& inPars, const mfxFrameInfo& fi, SurfacesPool* sp);
    virtual ~IVideoReader() {}

    virtual mfxStatus Init() = 0;
    virtual mfxStatus GetOneFrame(mfxFrameSurface1* & pSurf) = 0;
    virtual void      Close() = 0;

protected:
    std::string   m_srcFileName;
    mfxFrameInfo  m_frameInfo; // info about video frames properties
    SurfacesPool* m_pOutSurfPool;
};

// reader of raw frames
class YUVReader : public IVideoReader
{
public:
    YUVReader(const sInputParams& inPars, const mfxFrameInfo& fi, SurfacesPool* sp);
    ~YUVReader();

    mfxStatus Init();
    mfxStatus GetOneFrame(mfxFrameSurface1* & pSurf);
    void      Close();

private:
    CSmplYUVReader   m_FileReader;
    mfxU32           m_srcColorFormat;
};

// TODO: implement decoder functionality
// class MFX_Decode : public IVideoReader {};

#endif // #define __FEI_UTILS_H__
