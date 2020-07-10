/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

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

\* ****************************************************************************** */

#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxENCInput &encIn)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encIn.reserved) + "\n";
    str += structName + ".InSurface=" + ToString(encIn.InSurface) + "\n";
    str += structName + ".NumFrameL0=" + ToString(encIn.NumFrameL0) + "\n";
    str += structName + ".L0Surface=" + ToString(encIn.L0Surface) + "\n";
    str += structName + ".NumFrameL1=" + ToString(encIn.NumFrameL1) + "\n";
    str += structName + ".L1Surface=" + ToString(encIn.L1Surface) + "\n";
    str += dump_mfxExtParams(structName, encIn);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxENCOutput &encOut)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encOut.reserved) + "\n";
    str += dump_mfxExtParams(structName, encOut);
    return str;
}