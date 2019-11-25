/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT)

#include "vaapi_device.h"

#if defined(LIBVA_X11_SUPPORT)

#include <va/va_x11.h>
#include <X11/Xlib.h>
#include "vaapi_allocator.h"

#undef Status

#define VAAPI_GET_X_DISPLAY(_display) (Display*)(_display)
#define VAAPI_GET_X_WINDOW(_window) (Window*)(_window)


CVAAPIDeviceX11::~CVAAPIDeviceX11(void)
{
    Close();
}

mfxStatus CVAAPIDeviceX11::Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    Window* window = NULL;

    if (nViews)
    {
        if (MFX_ERR_NONE == mfx_res)
        {
            m_window = window = (Window*)malloc(sizeof(Window));
            if (!m_window) mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
            MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
            mfxU32 screen_number = DefaultScreen(display);

            *window = x11lib.XCreateSimpleWindow(display,
                                        RootWindow(display, screen_number),
                                        0,
                                        0,
                                        100,
                                        100,
                                        0,
                                        0,
                                        WhitePixel(display, screen_number));
            if (!(*window)) mfx_res = MFX_ERR_UNKNOWN;
            else
            {
                x11lib.XMapWindow(display, *window);
                x11lib.XSync(display, False);
            }
        }
    }
    return mfx_res;
}

void CVAAPIDeviceX11::Close(void)
{
    if (m_window)
    {
        Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
        Window* window = VAAPI_GET_X_WINDOW(m_window);

        MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
        x11lib.XDestroyWindow(display, *window);

        free(m_window);
        m_window = NULL;
    }
}

mfxStatus CVAAPIDeviceX11::Reset(void)
{
    return MFX_ERR_NONE;
}

mfxStatus CVAAPIDeviceX11::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
    {
        *pHdl = m_X11LibVA.GetVADisplay();

        return MFX_ERR_NONE;
    }

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CVAAPIDeviceX11::SetHandle(mfxHandleType type, mfxHDL hdl)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CVAAPIDeviceX11::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*pmfxAlloc*/)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus mfx_res = MFX_ERR_NONE;
    vaapiMemId * memId = NULL;
    VASurfaceID surface;
    Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
    Window* window = VAAPI_GET_X_WINDOW(m_window);

    if(!window || !(*window)) mfx_res = MFX_ERR_NOT_INITIALIZED;
    // should MFX_ERR_NONE be returned below considuring situation as EOS?
    if ((MFX_ERR_NONE == mfx_res) && NULL == pSurface) mfx_res = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == mfx_res)
    {
        memId = (vaapiMemId*)(pSurface->Data.MemId);
        if (!memId || !memId->m_surface) mfx_res = MFX_ERR_NULL_PTR;
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        surface = *memId->m_surface;

        MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
        x11lib.XResizeWindow(display,
                      *window,
                      pSurface->Info.CropW, pSurface->Info.CropH);

        MfxLoader::VA_X11Proxy & vax11lib = m_X11LibVA.GetVAX11();
        vax11lib.vaPutSurface(m_X11LibVA.GetVADisplay(),
                        surface,
                        *window,
                        pSurface->Info.CropX,
                        pSurface->Info.CropY,
                        pSurface->Info.CropX + pSurface->Info.CropW,
                        pSurface->Info.CropY + pSurface->Info.CropH,
                        pSurface->Info.CropX,
                        pSurface->Info.CropY,
                        pSurface->Info.CropX + pSurface->Info.CropW,
                        pSurface->Info.CropY + pSurface->Info.CropH,
                        NULL,
                        0,
                        VA_FRAME_PICTURE);

        mfx_res = va_to_mfx_status(va_res);
        x11lib.XSync(display, False);
    }
    return mfx_res;
}

#endif

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT)


CHWDevice* CreateVAAPIDevice(std::string devicePath, int type)
{
   CHWDevice * device = nullptr;
    switch (type)
    {
    case MFX_LIBVA_DRM:
#if defined(LIBVA_DRM_SUPPORT)
        try
        {
            device = new CVAAPIDeviceDRM(devicePath);
        }
        catch (std::exception&)
        {
            device = nullptr;
        }
#endif
        break;
    case MFX_LIBVA_X11:
#if defined(LIBVA_X11_SUPPORT)
        try
        {
            device = new CVAAPIDeviceX11;
        }
        catch (std::exception&)
        {
            device = nullptr;
        }
#endif
        break;
    case MFX_LIBVA_AUTO:
#if defined(LIBVA_X11_SUPPORT)
        try
        {
            device = new CVAAPIDeviceX11;
        }
        catch (std::exception&)
        {
            device = nullptr;
        }
#endif
#if defined(LIBVA_DRM_SUPPORT)
        if (!device)
        {
            try
            {
                device = new CVAAPIDeviceDRM(devicePath);
            }
            catch (std::exception&)
            {
                device = nullptr;
            }
        }
#endif
        break;
    } // switch(type)
    return device;
}

#elif defined(LIBVA_ANDROID_SUPPORT)

CHWDevice* CreateVAAPIDevice(const std::string&, int)
{
    return new CVAAPIDeviceAndroid();
}

#endif
#endif //#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT)
