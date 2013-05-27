/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFXUTILS_H__
#define __MFXUTILS_H__

#include "umc_structures.h"
#include "mfx_trace.h"
#include "mfx_timing.h"

#ifndef MFX_DEBUG_TRACE
#define MFX_STS_TRACE(sts) sts
#else
template <typename T>
static inline T mfx_print_err(T sts, char *file, int line, char *func)
{
    if (sts)
    {
        printf("%s: %d: %s: Error = %d\n", file, line, func, sts);
    }
    return sts;
}
#define MFX_STS_TRACE(sts) mfx_print_err(sts, __FILE__, __LINE__, __FUNCTION__)
#endif

#define MFX_SUCCEEDED(sts)      (MFX_STS_TRACE(sts) == MFX_ERR_NONE)
#define MFX_FAILED(sts)         (MFX_STS_TRACE(sts) != MFX_ERR_NONE)
#define MFX_RETURN(sts)         { return MFX_STS_TRACE(sts); }
#define MFX_CHECK(EXPR, ERR)    { if (!(EXPR)) MFX_RETURN(ERR); }

#define MFX_CHECK_STS(sts)              MFX_CHECK(MFX_SUCCEEDED(sts), sts)
#define MFX_SAFE_CALL(FUNC)             { mfxStatus _sts = FUNC; MFX_CHECK_STS(_sts); }
#define MFX_CHECK_NULL_PTR1(pointer)    MFX_CHECK(pointer, MFX_ERR_NULL_PTR)
#define MFX_CHECK_NULL_PTR2(p1, p2)     { MFX_CHECK(p1, MFX_ERR_NULL_PTR); MFX_CHECK(p2, MFX_ERR_NULL_PTR); }
#define MFX_CHECK_NULL_PTR3(p1, p2, p3) { MFX_CHECK(p1, MFX_ERR_NULL_PTR); MFX_CHECK(p2, MFX_ERR_NULL_PTR); MFX_CHECK(p3, MFX_ERR_NULL_PTR); }
#define MFX_CHECK_STS_ALLOC(pointer)    MFX_CHECK(pointer, MFX_ERR_MEMORY_ALLOC)
#define MFX_CHECK_COND(cond)            MFX_CHECK(cond, MFX_ERR_UNSUPPORTED)
#define MFX_CHECK_INIT(InitFlag)        MFX_CHECK(InitFlag, MFX_ERR_MORE_DATA)

#define MFX_CHECK_UMC_ALLOC(err) if (err != true) {return MFX_ERR_MEMORY_ALLOC;}
#define MFX_CHECK_EXBUF_INDEX(index) if (index == -1) {return MFX_ERR_MEMORY_ALLOC;}

static const mfxU32 MFX_TIME_STAMP_FREQUENCY = 90000; // will go to mfxdefs.h
static const mfxU64 MFX_TIME_STAMP_INVALID = (mfxU64)-1; // will go to mfxdefs.h

#define MFX_CHECK_UMC_STS(err) if (err != UMC::UMC_OK) {return ConvertStatusUmc2Mfx(err);}

inline
mfxStatus ConvertStatusUmc2Mfx(UMC::Status umcStatus)
{
    switch (umcStatus)
    {
    case UMC::UMC_OK: return MFX_ERR_NONE;
    case UMC::UMC_ERR_NULL_PTR: return MFX_ERR_NULL_PTR;
    case UMC::UMC_ERR_UNSUPPORTED: return MFX_ERR_UNSUPPORTED;
    case UMC::UMC_ERR_ALLOC: return MFX_ERR_MEMORY_ALLOC;
    case UMC::UMC_ERR_LOCK: return MFX_ERR_LOCK_MEMORY;
    case UMC::UMC_ERR_NOT_ENOUGH_BUFFER: return MFX_ERR_NOT_ENOUGH_BUFFER;
    case UMC::UMC_ERR_NOT_ENOUGH_DATA: return MFX_ERR_MORE_DATA;
    default: return MFX_ERR_ABORTED; // need general error code here
    }
}

inline
mfxF64 GetUmcTimeStamp(mfxU64 ts)
{
    return ts == MFX_TIME_STAMP_INVALID ? -1.0 : ts / (mfxF64)MFX_TIME_STAMP_FREQUENCY;
}

inline
mfxU64 GetMfxTimeStamp(mfxF64 ts)
{
    return ts < 0.0 ? MFX_TIME_STAMP_INVALID : (mfxU64)(ts * MFX_TIME_STAMP_FREQUENCY + .5);
}


#define MFX_ZERO_MEM(_X)    memset(&_X, 0, sizeof(_X))

#ifndef SAFE_DELETE
#define SAFE_DELETE(PTR)    { if (PTR) { delete PTR; PTR = NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(PTR)   { if (PTR) { PTR->Release(); PTR = NULL; } }
#endif

#define IS_PROTECTION_PAVP_ANY(val) (MFX_PROTECTION_PAVP == (val) || MFX_PROTECTION_GPUCP_PAVP == (val))
#define IS_PROTECTION_ANY(val) (IS_PROTECTION_PAVP_ANY(val) || MFX_PROTECTION_GPUCP_AES128_CTR == (val))
#define IS_PROTECTION_GPUCP_ANY(val) (MFX_PROTECTION_GPUCP_PAVP == (val) || MFX_PROTECTION_GPUCP_AES128_CTR == (val))

//#undef  SUCCEEDED
//#define SUCCEEDED(hr)   (MFX_STS_TRACE(hr) >= 0)
//#undef  FAILED
//#define FAILED(hr)      (MFX_STS_TRACE(hr) < 0)

#endif // __MFXUTILS_H__
