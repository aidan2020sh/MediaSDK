/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#include "mfx_samples_config.h"

#include "pipeline_manager.h"
#include <iostream>
#include <string>
#ifdef UNICODE
    #include <clocale>
    #include <locale>
    #pragma warning(disable: 4996)
#endif

#ifdef MFX_D3D11_SUPPORT
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace {
    msdk_string cvt_string2msdk(const std::string & lhs)
    {
    #ifndef UNICODE
        return lhs;
    #else
        std::locale const loc("");
        std::vector<wchar_t> buffer(lhs.size() + 1);
        std::use_facet<std::ctype<wchar_t> >(loc).widen(&lhs.at(0), &lhs.at(0) + lhs.size(), &buffer[0]);
        return msdk_string(&buffer[0], &buffer[lhs.size()]);
    #endif
    }
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int nParams, TCHAR ** sparams)
#else
int main(int nParams, char ** sparams)
#endif
{
    try
    {
        msdk_stringstream ss;
        for (int i = 1; i < nParams; i++) {
            ss<<sparams[i];
            if (i + 1 != nParams)
                ss<<MSDK_CHAR(' ');
        }
        PipelineFactory factory;
        std::auto_ptr<CmdLineParser> parser(factory.CreateCmdLineParser());
        if (nParams == 1 || !parser->Parse(ss.str())) {
            parser->PrintHelp(msdk_cout);
            return 1;
        }

        PipelineManager mgr(factory);
        mgr.Build(*parser);
        mgr.Run();

        return 0;
    }
    catch (KnownException & /*e*/) {
        //TRACEs should be generated for that exception
        MSDK_TRACE_ERROR(MSDK_STRING("Exiting..."));
    }
    catch (std::exception & e) {
        MSDK_TRACE_ERROR(MSDK_STRING("exception: ") << cvt_string2msdk(e.what()));
    }
    catch(...) {
        MSDK_TRACE_ERROR(MSDK_STRING("Unknown exception "));
    }
    return 1;
}