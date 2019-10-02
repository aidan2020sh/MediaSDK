/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_SUPPORT)

#include <stdio.h>
#include <assert.h>

#include "vaapi_allocator.h"
#include "vaapi_utils.h"

unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_UYVY:
        return VA_FOURCC_UYVY;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
#if (MFX_VERSION >= 1028)
    case MFX_FOURCC_RGBP:
        return VA_FOURCC_RGBP;
#endif
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;

    case MFX_FOURCC_A2RGB10:
        return VA_FOURCC_ARGB;  // rt format will be VA_RT_FORMAT_RGB32_10BPP

    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        return VA_FOURCC_Y210;
    case MFX_FOURCC_Y410:
        return VA_FOURCC_Y410;
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
        return VA_FOURCC_P016;
    case MFX_FOURCC_Y216:
        return VA_FOURCC_Y216;
    case MFX_FOURCC_Y416:
        return VA_FOURCC_Y416;
#endif
    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

vaapiFrameAllocator::vaapiFrameAllocator()
    : m_dpy(0)
{
}

vaapiFrameAllocator::~vaapiFrameAllocator()
{
    Close();
}

mfxStatus vaapiFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    vaapiAllocatorParams* p_vaapiParams = dynamic_cast<vaapiAllocatorParams*>(pParams);

    if ((NULL == p_vaapiParams) || (NULL == p_vaapiParams->m_dpy))
        return MFX_ERR_NOT_INITIALIZED;

    m_dpy = p_vaapiParams->m_dpy;

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

mfxStatus vaapiFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res  = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    VASurfaceAttrib attrib;
    vaapiMemId *vaapi_mids = NULL, *vaapi_mid = NULL;
    mfxMemId* mids = NULL;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 surfaces_num = request->NumFrameSuggested, numAllocated = 0, i = 0;
    bool bCreateSrfSucceeded = false;

    memset(response, 0, sizeof(mfxFrameAllocResponse));

    va_fourcc = ConvertMfxFourccToVAFormat(fourcc);
    if (!va_fourcc)
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
    if (MFX_ERR_NONE == mfx_res)
    {
        if( VA_FOURCC_P208 != va_fourcc )
        {
            unsigned int format;
            attrib.type = VASurfaceAttribPixelFormat;
            attrib.value.type = VAGenericValueTypeInteger;
            attrib.value.value.i = va_fourcc;
            format = va_fourcc;
            attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
            if (fourcc == VA_FOURCC_UYVY)
            {
                format = VA_RT_FORMAT_YUV422;
            }
            else if (va_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }
            else if (fourcc == MFX_FOURCC_A2RGB10)
            {
                format = VA_RT_FORMAT_RGB32_10BPP;
            }
#if (MFX_VERSION >= 1028)
            else if (fourcc == MFX_FOURCC_RGBP)
            {
                format = VA_RT_FORMAT_RGBP;
            }
#endif

            va_res = m_libva.vaCreateSurfaces(m_dpy,
                                    format,
                                    request->Info.Width, request->Info.Height,
                                    surfaces,
                                    surfaces_num,
                                    &attrib, 1);
            mfx_res = va_to_mfx_status(va_res);
            bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        }
        else
        {
            VAContextID context_id = request->AllocId;
#if defined(ANDROID)
            int codedbuf_size = 
                static_cast<int>((request->Info.Width * request->Info.Height) * 400LL / (16 * 16)); //from libva spec
#else
            int width32 = 32 * ((request->Info.Width + 31) >> 5);
            int height32 = 32 * ((request->Info.Height + 31) >> 5);
            int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec
            //codedbuf_size = 0x1000 * ((codedbuf_size + 0xfff) >> 12);  // align to page size
#endif

            for (numAllocated = 0; numAllocated < surfaces_num; numAllocated++)
            {
                VABufferID coded_buf;

                va_res = m_libva.vaCreateBuffer(m_dpy,
                                      context_id,
                                      VAEncCodedBufferType,
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
        if (VA_FOURCC_P208 != va_fourcc)
        {
            if (bCreateSrfSucceeded) m_libva.vaDestroySurfaces(m_dpy, surfaces, surfaces_num);
        }
        else
        {
            for (i = 0; i < numAllocated; i++)
                m_libva.vaDestroyBuffer(m_dpy, surfaces[i]);
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
        isBitstreamMemory = (MFX_FOURCC_P8 == vaapi_mids->m_fourcc)?true:false;
        surfaces = vaapi_mids->m_surface;
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc) m_libva.vaDestroyBuffer(m_dpy, surfaces[i]);
            else if (vaapi_mids[i].m_sys_buffer) free(vaapi_mids[i].m_sys_buffer);
        }
        free(vaapi_mids);
        free(response->mids);
        response->mids = NULL;

        if (!isBitstreamMemory) m_libva.vaDestroySurfaces(m_dpy, surfaces, response->NumFrameActual);
        free(surfaces);
    }
    response->NumFrameActual = 0;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res = VA_STATUS_SUCCESS;
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;
    mfxU8* pBuffer = 0;
    mfxU32 mfx_fourcc = vaapi_mid->m_fourcc;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == vaapi_mid->m_fourcc)   // bitstream processing
    {
        VACodedBufferSegment *coded_buffer_segment;
        va_res = m_libva.vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void **)(&coded_buffer_segment));
        mfx_res = va_to_mfx_status(va_res);
        ptr->Y = (mfxU8*)coded_buffer_segment->buf; // !!! bug
    }
    else   // Image processing
    {
        va_res = m_libva.vaDeriveImage(m_dpy, *(vaapi_mid->m_surface), &(vaapi_mid->m_image));
        mfx_res = va_to_mfx_status(va_res);

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = m_libva.vaMapBuffer(m_dpy, vaapi_mid->m_image.buf, (void **)&pBuffer);
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            switch (vaapi_mid->m_image.format.fourcc)
            {
            case VA_FOURCC_NV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                }
                break;
            case VA_FOURCC_YV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[2];
                }
                break;
            case VA_FOURCC_YUY2:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                }
                break;
            case VA_FOURCC_UYVY:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->Y = ptr->U + 1;
                    ptr->V = ptr->U + 2;
                }
                break;
            case VA_FOURCC_ARGB:
                if (mfx_fourcc == MFX_FOURCC_RGB4)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                }
                else if (mfx_fourcc == MFX_FOURCC_A2RGB10)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B;
                    ptr->R = ptr->B;
                    ptr->A = ptr->B;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#if (MFX_VERSION >= 1028)
            case VA_FOURCC_RGBP:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[2];
                }
                break;
#endif
            case VA_FOURCC_ABGR:
                if (mfx_fourcc == MFX_FOURCC_BGR4)
                {
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->R + 1;
                    ptr->B = ptr->R + 2;
                    ptr->A = ptr->R + 3;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_P016:
#endif
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16 *) (pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = (mfxU16 *) (pBuffer + vaapi_mid->m_image.offsets[1]);
                    ptr->V16 = ptr->U16 + 1;
                }
                break;
            case VA_FOURCC_AYUV:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->V + 1;
                    ptr->Y = ptr->V + 2;
                    ptr->A = ptr->V + 3;
                }
                break;
#if (MFX_VERSION >= 1027)
            case VA_FOURCC_Y210:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_Y216:
#endif
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16 *) (pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = ptr->Y16 + 1;
                    ptr->V16 = ptr->Y16 + 3;
                }
                break;
            case VA_FOURCC_Y410:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y410 = (mfxY410 *)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y = 0;
                    ptr->V = 0;
                    ptr->A = 0;
                }
                break;
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_Y416:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->U16 = (mfxU16 *) (pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y16 = ptr->U16 + 1;
                    ptr->V16 = ptr->Y16 + 1;
                    ptr->A   = (mfxU8 *)(ptr->V16 + 1);
                }
                break;
#endif
            default:
                return MFX_ERR_LOCK_MEMORY;
            }
        }

        ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
        ptr->PitchLow  = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == vaapi_mid->m_fourcc)   // bitstream processing
    {
        m_libva.vaUnmapBuffer(m_dpy, *(vaapi_mid->m_surface));
    }
    else  // Image processing
    {
        m_libva.vaUnmapBuffer(m_dpy, vaapi_mid->m_image.buf);
        m_libva.vaDestroyImage(m_dpy, vaapi_mid->m_image.image_id);

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
