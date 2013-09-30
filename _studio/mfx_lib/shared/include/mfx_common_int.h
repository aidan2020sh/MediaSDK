/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2013 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __MFX_COMMON_INT_H__
#define __MFX_COMMON_INT_H__

#include <vector>
#include <memory>
#include <errno.h>
#include "mfx_common.h"
#include "mfxaudio.h"

#if defined(ANDROID)
typedef int error_t;
#endif

mfxStatus CheckFrameInfoCommon(mfxFrameInfo  *info, mfxU32 codecId);
mfxStatus CheckFrameInfoEncoders(mfxFrameInfo  *info);
mfxStatus CheckFrameInfoCodecs(mfxFrameInfo  *info, mfxU32 codecId = MFX_CODEC_AVC);

mfxStatus CheckVideoParamEncoders(mfxVideoParam *in, bool IsExternalFrameAllocator, eMFXHWType type);
mfxStatus CheckVideoParamDecoders(mfxVideoParam *in, bool IsExternalFrameAllocator, eMFXHWType type);

mfxStatus CheckAudioParamEncoders(mfxAudioParam *in);
mfxStatus CheckAudioParamCommon(mfxAudioParam *in);
mfxStatus CheckAudioParamDecoders(mfxAudioParam *in);

mfxStatus CheckBitstream(const mfxBitstream *bs);
mfxStatus CheckAudioFrame(const mfxAudioFrame *aFrame);
mfxStatus CheckEncryptedBitstream(const mfxBitstream *bs);
mfxStatus CheckFrameData(const mfxFrameSurface1 *surface);

mfxStatus CheckDecodersExtendedBuffers(mfxVideoParam* par);

mfxExtBuffer* GetExtendedBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id);
mfxExtBuffer* GetExtendedBufferInternal(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id);

class ExtendedBuffer
{
public:

    ExtendedBuffer();
    virtual ~ExtendedBuffer();

    template<typename T> void AddTypedBuffer(mfxU32 id)
    {
        if (GetBufferByIdInternal(id))
            return;

        mfxExtBuffer * buffer = (mfxExtBuffer *)(new mfxU8[sizeof(T)]);
        memset(buffer, 0, sizeof(T));
        buffer->BufferSz = sizeof(T);
        buffer->BufferId = id;
        AddBufferInternal(buffer);
    }

    void AddBuffer(mfxExtBuffer * buffer);

    size_t GetCount() const;

    template<typename T> T * GetBufferById(mfxU32 id)
    {
        return (T*)GetBufferByIdInternal(id);
    }

    template<typename T> T * GetBufferByPosition(mfxU32 pos)
    {
        return (T*)GetBufferByPositionInternal(pos);
    }

    mfxExtBuffer ** GetBuffers();

private:

    void AddBufferInternal(mfxExtBuffer * buffer);

    mfxExtBuffer * GetBufferByIdInternal(mfxU32 id);

    mfxExtBuffer * GetBufferByPositionInternal(mfxU32 pos);

    void Release();

    typedef std::vector<mfxExtBuffer *>  BuffersList;
    BuffersList m_buffers;
};

class mfxVideoParamWrapper : public mfxVideoParam
{
public:

    mfxVideoParamWrapper();

    mfxVideoParamWrapper(const mfxVideoParam & par);

    virtual ~mfxVideoParamWrapper();

    mfxVideoParamWrapper & operator = (const mfxVideoParam & par);

    mfxVideoParamWrapper & operator = (const mfxVideoParamWrapper & par);

    bool CreateExtendedBuffer(mfxU32 bufferId);

    template<typename T> T * GetExtendedBuffer(mfxU32 id)
    {
        T * extBuf = m_buffers.GetBufferById<T>(id);

        if (!extBuf)
        {
            m_buffers.AddTypedBuffer<T>(id);
            extBuf = m_buffers.GetBufferById<T>(id);
            if (!extBuf)
                throw 1;
        }

        return extBuf;
    }

private:

    ExtendedBuffer m_buffers;
    mfxU8* m_mvcSequenceBuffer;

    void CopyVideoParam(const mfxVideoParam & par);

    // Deny copy constructor
    mfxVideoParamWrapper(const mfxVideoParamWrapper &);
};

#pragma warning(disable: 4127)
template<typename T1, typename T2>
mfxU32 memcpy_s(T1* pDst, T2* pSrc)
{
    if (sizeof(T1) != sizeof(T2))
        return 0;

    ippsCopy_8u((Ipp8u*)pSrc, (Ipp8u*)pDst, sizeof(T1));
    return sizeof(T1);
}

#if !defined(_WIN32) && !defined(_WIN64)
#if defined(__GNUC__)
inline
#else
__attribute__((always_inline))
#endif
error_t memcpy_s(void* pDst, size_t nDstSize, const void* pSrc, size_t nCount)
{
    if (pDst && pSrc && (nDstSize >= nCount))
    {
        ippsCopy_8u((Ipp8u*)pSrc, (Ipp8u*)pDst, nCount);
        return 0;
    }
    if (!pDst) return EINVAL;
    if (!pSrc)
    {
        ippsZero_8u((Ipp8u*)pDst, nDstSize);
        return EINVAL;
    }
    // only remainnig option: nDstSize < nCount
    ippsZero_8u((Ipp8u*)pDst, nDstSize);
    return ERANGE;
}

#endif

#endif
