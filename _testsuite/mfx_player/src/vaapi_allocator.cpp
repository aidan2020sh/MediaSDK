/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_SUPPORT)

#include <stdio.h>
#include <assert.h>

#include "vaapi_allocator.h"

enum {
    MFX_FOURCC_VP8_NV12    = MFX_MAKEFOURCC('V','P','8','N'),
    MFX_FOURCC_VP8_MBDATA  = MFX_MAKEFOURCC('V','P','8','M'),
    MFX_FOURCC_VP8_SEGMAP  = MFX_MAKEFOURCC('V','P','8','S'),
};

unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;

    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

unsigned int ConvertVP8FourccToMfxFourcc(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_VP8_NV12:
    case MFX_FOURCC_VP8_MBDATA:
        return MFX_FOURCC_NV12;
    case MFX_FOURCC_VP8_SEGMAP:
        return MFX_FOURCC_P8;

    default:
        return fourcc;
    }
}

vaapiFrameAllocator::vaapiFrameAllocator()
    : m_dpy(0),
    m_libva(new MfxLoader::VA_Proxy),
    m_bAdaptivePlayback(false)
{
}

vaapiFrameAllocator::~vaapiFrameAllocator()
{
    Close();
    delete m_libva;
}

mfxStatus vaapiFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    vaapiAllocatorParams* p_vaapiParams = dynamic_cast<vaapiAllocatorParams*>(pParams);

    if ((NULL == p_vaapiParams) || (NULL == p_vaapiParams->m_dpy))
        return MFX_ERR_NOT_INITIALIZED;

    m_dpy = p_vaapiParams->m_dpy;
    m_bAdaptivePlayback =p_vaapiParams->bAdaptivePlayback;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus vaapiFrameAllocator::Close()
{
    return BaseFrameAllocator::Close();
}

mfxStatus vaapiFrameAllocator::AllocImpl(mfxFrameSurface1 * surf)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res  = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    mfxU32 fourcc = surf->Info.FourCC;

    // VP8 hybrid driver has weird requirements for allocation of surfaces/buffers for VP8 encoding
    // to comply with them additional logic is required to support regular and VP8 hybrid allocation pathes
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(fourcc);
    va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
                       (VA_FOURCC_YV12 != va_fourcc) &&
                       (VA_FOURCC_YUY2 != va_fourcc) &&
                       (VA_FOURCC_ARGB != va_fourcc) &&
                       (VA_FOURCC_ABGR != va_fourcc) &&
                       (VA_FOURCC_P208 != va_fourcc) &&
                       (VA_FOURCC_P010 != va_fourcc) &&
                       (VA_FOURCC_AYUV != va_fourcc)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if( VA_FOURCC_P208 != va_fourcc)
    {
        VASurfaceID surfaces[1];
        VASurfaceAttrib attrib[2];
        vaapiMemId *vaapiMid = (vaapiMemId *)surf->Data.MemId;
        surfaces[0] = *vaapiMid->m_surface;
        m_libva->vaDestroySurfaces(m_dpy, surfaces, 1);

        unsigned int format;
        int attrCnt = 0;

        attrib[attrCnt].type          = VASurfaceAttribPixelFormat;
        attrib[attrCnt].flags         = VA_SURFACE_ATTRIB_SETTABLE;
        attrib[attrCnt].value.type    = VAGenericValueTypeInteger;
        attrib[attrCnt++].value.value.i = va_fourcc;
        format               = va_fourcc;

        if ((fourcc == MFX_FOURCC_VP8_NV12) ||
            ((MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET & surf->Data.MemType) 
            && ((fourcc == MFX_FOURCC_RGB4) || (fourcc == MFX_FOURCC_BGR4))))
        {
/*
 *  special configuration for NV12 surf allocation for VP8 hybrid encoder and
 *  RGB32 for JPEG is required
 */
            attrib[attrCnt].type            = (VASurfaceAttribType)VASurfaceAttribUsageHint;
            attrib[attrCnt].flags           = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type      = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
        }
        else if (fourcc == MFX_FOURCC_VP8_MBDATA)
        {
            // special configuration for MB data surf allocation for VP8 hybrid encoder is required
            attrib[0].value.value.i = VA_FOURCC_P208;
            format               = VA_FOURCC_P208;
        }
        else if (va_fourcc == VA_FOURCC_NV12)
        {
            format = VA_RT_FORMAT_YUV420;
        }

        va_res = m_libva->vaCreateSurfaces(m_dpy,
                                format,
                                surf->Info.Width, surf->Info.Height,
                                surfaces,
                                1,
                                &attrib[0], attrCnt);

        *vaapiMid->m_surface = surfaces[0];
        mfx_res = va_to_mfx_status(va_res);
    }

    return mfx_res;
}

mfxStatus vaapiFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res  = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    vaapiMemId *vaapi_mids = NULL, *vaapi_mid = NULL;
    mfxMemId* mids = NULL;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 surfaces_num = request->NumFrameSuggested, numAllocated = 0, i = 0;
    bool bCreateSrfSucceeded = false;

    memset(response, 0, sizeof(mfxFrameAllocResponse));

    // VP8 hybrid driver has weird requirements for allocation of surfaces/buffers for VP8 encoding
    // to comply with them additional logic is required to support regular and VP8 hybrid allocation pathes
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(fourcc);
    va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
                       (VA_FOURCC_YV12 != va_fourcc) &&
                       (VA_FOURCC_YUY2 != va_fourcc) &&
                       (VA_FOURCC_ARGB != va_fourcc) &&
                       (VA_FOURCC_ABGR != va_fourcc) &&
                       (VA_FOURCC_P208 != va_fourcc) &&
                       (VA_FOURCC_P010 != va_fourcc) &&
                       (VA_FOURCC_AYUV != va_fourcc)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (!surfaces_num)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        surfaces = (VASurfaceID*)calloc(surfaces_num, sizeof(VASurfaceID));
        vaapi_mids = (vaapiMemId*)calloc(surfaces_num, sizeof(vaapiMemId));
        mids = (mfxMemId*)calloc(surfaces_num, sizeof(mfxMemId));
        if ((NULL == surfaces) || (NULL == vaapi_mids) || (NULL == mids)) mfx_res = MFX_ERR_MEMORY_ALLOC;
    }

    if ( m_bAdaptivePlayback )
    {
        for (i = 0; i < surfaces_num; ++i)
        {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            surfaces[i] = (VASurfaceID)VA_INVALID_ID;
            vaapi_mid->m_surface = &surfaces[i];
            mids[i] = vaapi_mid;
        }
        m_Width  = request->Info.Width;
        m_Height = request->Info.Height;
        response->mids = mids;
        response->NumFrameActual = surfaces_num;
        return MFX_ERR_NONE;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        if( VA_FOURCC_P208 != va_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib attrib[2];
            int attrCnt = 0;

            attrib[attrCnt].type          = VASurfaceAttribPixelFormat;
            attrib[attrCnt].flags         = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type    = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = va_fourcc;
            format               = va_fourcc;

            if ((fourcc == MFX_FOURCC_VP8_NV12) ||
                ((MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET & request->Type) 
                && ((fourcc == MFX_FOURCC_RGB4) || (fourcc == MFX_FOURCC_BGR4))))
            {
/*
 *  special configuration for NV12 surf allocation for VP8 hybrid encoder and
 *  RGB32 for JPEG is required
 */
                attrib[attrCnt].type            = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib[attrCnt].flags           = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[attrCnt].value.type      = VAGenericValueTypeInteger;
                attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib[0].value.value.i = VA_FOURCC_P208;
                format               = VA_FOURCC_P208;
            }
            else if (va_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                                    format,
                                    request->Info.Width, request->Info.Height,
                                    surfaces,
                                    surfaces_num,
                                    &attrib[0], attrCnt);
            mfx_res = va_to_mfx_status(va_res);
            bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        }
        else
        {
            VAContextID context_id = request->AllocId;
            int codedbuf_size;

            int width32 = 32 * ((request->Info.Width + 31) >> 5);
            int height32 = 32 * ((request->Info.Height + 31) >> 5);

            VABufferType codedbuf_type;
            if (fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = request->Info.Width * request->Info.Height;
                codedbuf_type = (VABufferType)VAEncMacroblockMapBufferType;
            }
            else
            {
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_type = VAEncCodedBufferType;
            }

            for (numAllocated = 0; numAllocated < surfaces_num; numAllocated++)
            {
                VABufferID coded_buf;

                va_res = m_libva->vaCreateBuffer(m_dpy,
                                      context_id,
                                      codedbuf_type,
                                      codedbuf_size,
                                      1,
                                      NULL,
                                      &coded_buf);
                mfx_res = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != mfx_res) break;
                surfaces[numAllocated] = coded_buf;
            }
        }

    }
    if (MFX_ERR_NONE == mfx_res)
    {
        for (i = 0; i < surfaces_num; ++i)
        {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            vaapi_mid->m_surface = &(surfaces[i]);
            mids[i] = vaapi_mid;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        response->mids = mids;
        response->NumFrameActual = surfaces_num;
    }
    else // i.e. MFX_ERR_NONE != mfx_res
    {
        response->mids = NULL;
        response->NumFrameActual = 0;
        if (VA_FOURCC_P208 != va_fourcc
            || fourcc == MFX_FOURCC_VP8_MBDATA )
        {
            if (bCreateSrfSucceeded) m_libva->vaDestroySurfaces(m_dpy, surfaces, surfaces_num);
        }
        else
        {
            for (i = 0; i < numAllocated; i++)
                m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
        }
        if (mids)
        {
            free(mids);
            mids = NULL;
        }
        if (vaapi_mids) { free(vaapi_mids); vaapi_mids = NULL; }
        if (surfaces) { free(surfaces); surfaces = NULL; }
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    vaapiMemId *vaapi_mids = NULL;
    VASurfaceID* surfaces = NULL;
    mfxU32 i = 0;
    bool isBitstreamMemory=false;

    if (!response) return MFX_ERR_NULL_PTR;

    if (response->mids)
    {
        vaapi_mids = (vaapiMemId*)(response->mids[0]);
        mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc);
        isBitstreamMemory = (MFX_FOURCC_P8 == mfx_fourcc)?true:false;
        surfaces = vaapi_mids->m_surface;
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc) m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
            else if (vaapi_mids[i].m_sys_buffer) free(vaapi_mids[i].m_sys_buffer);
        }
        free(vaapi_mids);
        free(response->mids);
        response->mids = NULL;

        if (!isBitstreamMemory) m_libva->vaDestroySurfaces(m_dpy, surfaces, response->NumFrameActual);
        free(surfaces);
    }
    response->NumFrameActual = 0;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res  = VA_STATUS_SUCCESS;
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;
    mfxU8* pBuffer = 0;
    VASurfaceAttrib attrib;
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if ( (VASurfaceID)VA_INVALID_ID == *(vaapi_mid->m_surface))
    {
        if( VA_FOURCC_P208 != vaapi_mid->m_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib *pAttrib = &attrib;

            attrib.type          = VASurfaceAttribPixelFormat;
            attrib.flags         = VA_SURFACE_ATTRIB_SETTABLE;
            attrib.value.type    = VAGenericValueTypeInteger;
            attrib.value.value.i = vaapi_mid->m_fourcc;
            format               = vaapi_mid->m_fourcc;

            if (mfx_fourcc == MFX_FOURCC_VP8_NV12)
            {
                // special configuration for NV12 surf allocation for VP8 hybrid encoder is required
                attrib.type          = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (mfx_fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib.value.value.i = VA_FOURCC_P208;
                format               = VA_FOURCC_P208;
            }
            else if (vaapi_mid->m_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }

#if defined(ANDROID)
            // It seems that VA implementation on android(W49) doesn't accept "attrib" parameter (ERR_UNKNOWN on
            // anything exept NULL). According to QuerySurfaceAttributes only NV12 is supported.
            if (vaapi_mid->m_fourcc != VA_FOURCC_NV12) return MFX_ERR_UNSUPPORTED;
            pAttrib = 0;
#endif

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                                    format,
                                    m_Width, m_Height,
                                    vaapi_mid->m_surface,
                                    1,
                                    pAttrib, pAttrib ? 1 : 0);
            mfx_res = va_to_mfx_status(va_res);
        }
        else
        {
            int codedbuf_size;

            int width32 = 32 * ((m_Width + 31) >> 5);
            int height32 = 32 * ((m_Height + 31) >> 5);

            VABufferType codedbuf_type;
            if (mfx_fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = m_Width * m_Height;
                codedbuf_type = (VABufferType)VAEncMacroblockMapBufferType;
            }
            else
            {
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_type = VAEncCodedBufferType;
            }
            VABufferID coded_buf;

            va_res = m_libva->vaCreateBuffer(m_dpy,
                                    VA_INVALID_ID,
                                    codedbuf_type,
                                    codedbuf_size,
                                    1,
                                    NULL,
                                    &coded_buf);
            mfx_res = va_to_mfx_status(va_res);
            //vaapi_mid->m_surface = coded_buf;
        }
        return MFX_ERR_NONE;
    }

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        VACodedBufferSegment *coded_buffer_segment;
        if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void **)(&pBuffer));
        else
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void **)(&coded_buffer_segment));
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE == mfx_res)
        {
            if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
                ptr->Y = pBuffer;
            else
                ptr->Y = (mfxU8*)coded_buffer_segment->buf;

        }
    }
    else   // Image processing
    {
        va_res = m_libva->vaDeriveImage(m_dpy, *(vaapi_mid->m_surface), &(vaapi_mid->m_image));
        mfx_res = va_to_mfx_status(va_res);

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = m_libva->vaMapBuffer(m_dpy, vaapi_mid->m_image.buf, (void **)&pBuffer);
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            switch (vaapi_mid->m_image.format.fourcc)
            {
            case VA_FOURCC_NV12:
                if (mfx_fourcc == MFX_FOURCC_NV12)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YV12:
                if (mfx_fourcc == MFX_FOURCC_YV12)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[2];
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YUY2:
                if (mfx_fourcc == MFX_FOURCC_YUY2)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_ARGB:
                if (mfx_fourcc == MFX_FOURCC_RGB4)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_ABGR:
                if (mfx_fourcc == MFX_FOURCC_BGR4)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->R + 1;
                    ptr->B = ptr->R + 2;
                    ptr->A = ptr->R + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_P208:
                if (mfx_fourcc == MFX_FOURCC_NV12)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_P010:
                if (mfx_fourcc == MFX_FOURCC_P010)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 2;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_AYUV:
                if (mfx_fourcc == MFX_FOURCC_AYUV)
                {
                    ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
                    ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->V + 1;
                    ptr->Y = ptr->V + 2;
                    ptr->A = ptr->V + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            default:
                mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            }
        }
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        m_libva->vaUnmapBuffer(m_dpy, *(vaapi_mid->m_surface));
    }
    else  // Image processing
    {
        m_libva->vaUnmapBuffer(m_dpy, vaapi_mid->m_image.buf);
        m_libva->vaDestroyImage(m_dpy, vaapi_mid->m_image.image_id);

        if (NULL != ptr)
        {
            ptr->PitchLow  = 0;
            ptr->PitchHigh = 0;
            ptr->Y     = NULL;
            ptr->U     = NULL;
            ptr->V     = NULL;
            ptr->A     = NULL;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!handle || !vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    *handle = vaapi_mid->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

#endif // #if defined(LIBVA_SUPPORT)
