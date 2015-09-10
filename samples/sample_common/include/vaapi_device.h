/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT) || defined(LIBVA_WAYLAND_SUPPORT)

#include "hw_device.h"
#include "vaapi_utils_drm.h"
#include "vaapi_utils_x11.h"
#if defined(LIBVA_ANDROID_SUPPORT)
#include "vaapi_utils_android.h"
#endif

CHWDevice* CreateVAAPIDevice(int type = MFX_LIBVA_DRM);

#if defined(LIBVA_DRM_SUPPORT)
/** VAAPI DRM implementation. */
class CVAAPIDeviceDRM : public CHWDevice
{
public:
    CVAAPIDeviceDRM(int type);
    virtual ~CVAAPIDeviceDRM(void);

    virtual mfxStatus Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum);
    virtual mfxStatus Reset(void) { return MFX_ERR_NONE; }
    virtual void Close(void) { }

    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl)
    {
        if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
        {
            *pHdl = m_DRMLibVA.GetVADisplay();

            return MFX_ERR_NONE;
        }
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc);
    virtual void      UpdateTitle(double fps) { }

    inline drmRenderer* getRenderer() { return m_rndr; }
protected:
    DRMLibVA m_DRMLibVA;
    drmRenderer * m_rndr;
};

#endif

#if defined(LIBVA_X11_SUPPORT)

/** VAAPI X11 implementation. */
class CVAAPIDeviceX11 : public CHWDevice
{
public:
    CVAAPIDeviceX11(){m_window = NULL;}
    virtual ~CVAAPIDeviceX11(void);

    virtual mfxStatus Init(
            mfxHDL hWindow,
            mfxU16 nViews,
            mfxU32 nAdapterNum);
    virtual mfxStatus Reset(void);
    virtual void Close(void);

    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);

    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc);
    virtual void      UpdateTitle(double fps) { }

protected:
    mfxHDL m_window;
    X11LibVA m_X11LibVA;
};

#endif

#if defined(LIBVA_WAYLAND_SUPPORT)

class Wayland;

class CVAAPIDeviceWayland : public CHWDevice
{
public:
    CVAAPIDeviceWayland(){}
    virtual ~CVAAPIDeviceWayland(void);

    virtual mfxStatus Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum);
    virtual mfxStatus Reset(void) { return MFX_ERR_NONE; }
    virtual void Close(void);

    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl)
    {
        if((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
        {
            *pHdl = m_DRMLibVA.GetVADisplay();
            return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
    }
    virtual void SetRenderWinPosSize(mfxU32 x, mfxU32 y, mfxU32 w, mfxU32 h);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc);
    virtual void UpdateTitle(double fps) { }

protected:
    DRMLibVA m_DRMLibVA;
    MfxLoader::VA_WaylandClientProxy  m_WaylandClient;
    Wayland *m_Wayland;
};

#endif

#if defined(LIBVA_ANDROID_SUPPORT)

/** VAAPI Android implementation. */
class CVAAPIDeviceAndroid : public CHWDevice
{
public:
    CVAAPIDeviceAndroid(void) {};
    virtual ~CVAAPIDeviceAndroid(void)  { Close();}

    virtual mfxStatus Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum) { return MFX_ERR_NONE;}
    virtual mfxStatus Reset(void) { return MFX_ERR_NONE; }
    virtual void Close(void) { }

    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl)
    {
        if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
        {
            *pHdl = m_AndroidLibVA.GetVADisplay();

            return MFX_ERR_NONE;
        }

        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc) { return MFX_ERR_NONE; }
    virtual void      UpdateTitle(double fps) { }

protected:
    AndroidLibVA m_AndroidLibVA;
};
#endif
#endif //#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT)