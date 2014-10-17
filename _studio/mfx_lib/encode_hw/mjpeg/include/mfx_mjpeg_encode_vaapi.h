/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_MJPEG_ENCODE_VAAPI_H__
#define __MFX_MJPEG_ENCODE_VAAPI_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include <vector>
#include <assert.h>
#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_jpeg.h>
#include "umc_mutex.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mjpeg_encode_hw_utils.h"
#include "mfx_mjpeg_encode_interface.h"

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

namespace MfxHwMJpegEncode
{
    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;
        mfxU32 size; // valid only if Surface ID == VA_INVALID_SURFACE (skipped frames)
    } ExtVASurface;

    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus RegisterBitstreamBuffer(
            mfxFrameAllocResponse & response);

        virtual
        mfxStatus Execute(DdiTask &task, mfxHDL surface);

        virtual
        mfxStatus QueryBitstreamBufferInfo(
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            JpegEncCaps & caps);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task);

        virtual
        mfxStatus UpdateBitstream(
            mfxMemId    MemId,
            DdiTask   & task);

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(VAAPIEncoder const &);              // no implementation
        VAAPIEncoder & operator =(VAAPIEncoder const &); // no implementation
        mfxStatus DestroyBuffers();

        VideoCORE       * m_core;
        mfxU32            m_width;
        mfxU32            m_height;
        JpegEncCaps       m_caps;
        VADisplay         m_vaDisplay;
        VAContextID       m_vaContextEncode;
        VAConfigID        m_vaConfig;

        UMC::Mutex        m_guard;
        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_bsQueue;

        //std::vector<VABufferID>  m_qmBufferIds;
        //std::vector<VABufferID>  m_htBufferIds;
        //std::vector<VABufferID>  m_scanBufferIds;
        VABufferID  m_qmBufferId;
        VABufferID  m_htBufferId;
        VABufferID  m_scanBufferId;
        VABufferID  m_ppsBufferId;
    };

}; // namespace

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)
#endif // __MFX_MJPEG_ENCODE_VAAPI_H__
