//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_DDI_H
#define __MFX_VPP_DDI_H

#include <vector>
#include <memory>
#include "mfxdefs.h"
#include "libmfx_core.h"
#include "mfx_platform_headers.h"

#if   defined(MFX_VA_WIN)
    #include "encoding_ddi.h"
#elif defined(MFX_VA_LINUX) || defined(MFX_VA_OSX)
    typedef unsigned int   UINT;

    typedef struct tagVPreP_StatusParams_V1_0
    {
        UINT StatusReportID;
        UINT Reserved1;

    } VPreP_StatusParams_V1_0;

    typedef struct tagFASTCOMP_QUERY_STATUS
    {
        UINT QueryStatusID;
        UINT Status;

        UINT Reserved0;
        UINT Reserved1;
        UINT Reserved2;
        UINT Reserved3;
        UINT Reserved4;

    } FASTCOMP_QUERY_STATUS;

    typedef enum _VPREP_ISTAB_MODE
    {
        ISTAB_MODE_NONE,
        ISTAB_MODE_BLACKEN,
        ISTAB_MODE_UPSCALE
    } VPREP_ISTAB_MODE;
#endif

namespace MfxHwVideoProcessing
{
    const mfxU32 g_TABLE_SUPPORTED_FOURCC [] =
    {
        MFX_FOURCC_NV12      ,
        MFX_FOURCC_YV12      ,
        MFX_FOURCC_NV16      ,
        MFX_FOURCC_YUY2      ,
        MFX_FOURCC_RGB3      ,
        MFX_FOURCC_RGB4      ,
        MFX_FOURCC_P8        ,
        MFX_FOURCC_P8_TEXTURE,
        MFX_FOURCC_P010      ,
        MFX_FOURCC_P210      ,
        MFX_FOURCC_BGR4      ,
        MFX_FOURCC_A2RGB10   ,
        MFX_FOURCC_ARGB16    ,
        MFX_FOURCC_R16       ,
        MFX_FOURCC_AYUV      ,
        MFX_FOURCC_AYUV_RGB4 ,
        MFX_FOURCC_UYVY
#if defined (PRE_SI_TARGET_PLATFORM_GEN11)
        , MFX_FOURCC_Y210
        , MFX_FOURCC_Y216
        , MFX_FOURCC_Y410
#endif
    };

    typedef enum mfxFormatSupport {
        MFX_FORMAT_SUPPORT_INPUT   = 1,
        MFX_FORMAT_SUPPORT_OUTPUT  = 2
    } mfxFormatSupport;

    struct RateRational
    {
        mfxU32  FrameRateExtN;
        mfxU32  FrameRateExtD;
    };

    struct CustomRateData
    {
        RateRational customRate;
        mfxU32       inputFramesOrFieldPerCycle;
        mfxU32       outputIndexCountPerCycle;
        mfxU32       bkwdRefCount;
        mfxU32       fwdRefCount;

        mfxU32       indexRateConversion;
    };

    struct FrcCaps
    {
        std::vector<CustomRateData> customRateData;
    };

    struct DstRect
    {
        mfxU32 DstX;
        mfxU32 DstY;
        mfxU32 DstW;
        mfxU32 DstH;
        mfxU16 LumaKeyEnable;
        mfxU16 LumaKeyMin;
        mfxU16 LumaKeyMax;
        mfxU16 GlobalAlphaEnable;
        mfxU16 GlobalAlpha;
        mfxU16 PixelAlphaEnable;
        mfxU16 IOPattern; //sys/video/opaque memory
        mfxU16 ChromaFormat;
        mfxU32 FourCC;
    };

#ifndef MFX_CAMERA_FEATURE_DISABLE
    typedef struct _CameraCaps
    {
        mfxU32 uBlackLevelCorrection;
        mfxU32 uHotPixelCheck;
        mfxU32 uWhiteBalance;
        mfxU32 uColorCorrectionMatrix;
        mfxU32 uGammaCorrection;
        mfxU32 uVignetteCorrection;
        mfxU32 u3DLUT;
    } CameraCaps;

    typedef struct _CameraBlackLevelParams
    {
        mfxU32 uR;
        mfxU32 uG0;
        mfxU32 uG1;
        mfxU32 uB;
    } CameraBlackLevelParams;

    typedef struct _CameraWhiteBalanceParams
    {
        mfxF32 fR;
        mfxF32 fG0;
        mfxF32 fG1;
        mfxF32 fB;
    } CameraWhiteBalanceParams;

    typedef struct _CameraHotPixelRemovalParams
    {
        mfxU32 uPixelThresholdDifference;
        mfxU32 uPixelCountThreshold;
    } CameraHotPixelRemovalParams;

    typedef struct _CameraCCM
    {
        mfxF32 CCM[3][3];
    } CameraCCMParams;


    typedef struct _CameraTCCParams
    {
        mfxU8 Red;
        mfxU8 Green;
        mfxU8 Blue;
        mfxU8 Cyan;
        mfxU8 Magenta;
        mfxU8 Yellow;
    } CameraTCCParams;

    typedef struct _CameraRGBToYUVParams
    {
        mfxU8 Red;
        mfxF32 PreOffset[3];
        mfxF32 Matrix[3][3];
        mfxF32 PostOffset[3];
    } CameraRGBToYUVParams;
    typedef struct _CameraLensCorrectionParams
    {
        mfxF32 a[3];
        mfxF32 b[3];
        mfxF32 c[3];
        mfxF32 d[3];
    } CameraLensCorrectionParams;

    typedef struct _CameraForwardGammaCorrectionSeg
    {
        mfxU16 PixelValue;
        mfxU16 RedChannelCorrectedValue;
        mfxU16 GreenChannelCorrectedValue;
        mfxU16 BlueChannelCorrectedValue;
    } CameraForwardGammaCorrectionSeg;

    typedef struct _CameraForwardGammaCorrectionParams
    {
        CameraForwardGammaCorrectionSeg Segment[64];
    } CameraForwardGammaCorrectionParams;

    typedef struct _LUT_ENTRY
    {
        USHORT R;
        USHORT G;
        USHORT B;
        USHORT Reserved;
    } LUT_ENTRY;

    const int LUT17_SEG = 17;
    const int LUT17_MUL = 32;
    const int LUT33_SEG = 33;
    const int LUT33_MUL = 64;
    const int LUT65_SEG = 65;
    const int LUT65_MUL = 128;
    typedef LUT_ENTRY LUT17[LUT17_SEG][LUT17_SEG][LUT17_MUL];
    typedef LUT_ENTRY LUT33[LUT33_SEG][LUT33_SEG][LUT33_MUL];
    typedef LUT_ENTRY LUT65[LUT65_SEG][LUT65_SEG][LUT65_MUL];

    typedef struct _Camera3DLUTParams
    {
        UINT  bActive;
        UINT  LUTSize;
        LUT_ENTRY *lut;
    } Camera3DLUTParams;

    typedef struct _CameraVignette_unsigned_8_8
    {
        USHORT      integer   : 8;
        USHORT      mantissa  : 8;
    } CameraVignette_unsigned_8_8;

  
    typedef struct _CameraVignetteCorrectionElem
    {
        CameraVignette_unsigned_8_8   R;
        CameraVignette_unsigned_8_8   G0;
        CameraVignette_unsigned_8_8   B;
        CameraVignette_unsigned_8_8   G1;
        USHORT                reserved;
    } CameraVignetteCorrectionElem;

    typedef struct _CameraVignetteCorrectionParams
    {
        UINT      bActive;
        UINT      Width;
        UINT      Height;
        UINT      Stride;
        CameraVignetteCorrectionElem *pCorrectionMap;
    } CameraVignetteCorrectionParams;
#endif // #ifndef MFX_CAMERA_FEATURE_DISABLE

    struct mfxVppCaps
    {
        mfxU32 uAdvancedDI;
        mfxU32 uSimpleDI;
        mfxU32 uInverseTC;
        mfxU32 uDenoiseFilter;
        mfxU32 uDetailFilter;
        mfxU32 uProcampFilter;
        mfxU32 uSceneChangeDetection;
#ifndef MFX_CAMERA_FEATURE_DISABLE
        CameraCaps cameraCaps;
#endif

        mfxU32 uFrameRateConversion;
        mfxU32 uDeinterlacing;
        mfxU32 uVideoSignalInfo;
        FrcCaps frcCaps;

        mfxU32 uIStabFilter;
        mfxU32 uVariance;

        mfxI32 iNumBackwardSamples;
        mfxI32 iNumForwardSamples;

        mfxU32 uMaxWidth;
        mfxU32 uMaxHeight;

        mfxU32 uFieldWeavingControl;

        mfxU32 uRotation;

        mfxU32 uScaling;

        mfxU32 uChromaSiting;

        std::map <mfxU32, mfxU32> mFormatSupport;

        mfxU32 uMirroring;

        mfxVppCaps()
            : uAdvancedDI(0)
            , uSimpleDI(0)
            , uInverseTC(0)
            , uDenoiseFilter(0)
            , uDetailFilter(0)
            , uProcampFilter(0)
            , uSceneChangeDetection(0)
            , uFrameRateConversion(0)
            , uDeinterlacing(0)
            , uVideoSignalInfo(0)
            , frcCaps()
            , uIStabFilter(0)
            , uVariance(0)
            , iNumBackwardSamples(0)
            , iNumForwardSamples(0)
            , uMaxWidth(0)
            , uMaxHeight(0)
            , uFieldWeavingControl(0)
            , uRotation(0)
            , uScaling(0)
            , uChromaSiting(0)
            , mFormatSupport()
            , uMirroring(0)
        {
#ifndef MFX_CAMERA_FEATURE_DISABLE
            memset(&cameraCaps, 0, sizeof(CameraCaps));
#endif
        };
    };


    typedef struct _mfxDrvSurface
    {
        mfxFrameInfo frameInfo;
        mfxHDLPair   hdl;

        mfxMemId     memId;
        bool         bExternal;

        mfxU64       startTimeStamp;
        mfxU64       endTimeStamp;

    } mfxDrvSurface;

    // Scene change detection
    typedef enum {
        VPP_NO_SCENE_CHANGE  = 0,
        VPP_SCENE_NEW        = 1,            // BOB display current field to generate output
        VPP_MORE_SCENE_CHANGE_DETECTED = 2 // BOB display only first field to avoid out of frame order
    } vppScene;

    class mfxExecuteParams
    {
        struct SignalInfo {
            bool   enabled;
            mfxU16 TransferMatrix;
            mfxU16 NominalRange;
        };

    public:
            mfxExecuteParams():
                targetSurface()
               ,targetTimeStamp()
               ,pRefSurfaces(0)
               ,refCount(0)
               ,bkwdRefCount(0)
               ,fwdRefCount(0)
               ,iDeinterlacingAlgorithm(0)
               ,bFMDEnable(false)
               ,bDenoiseAutoAdjust(false)
               ,denoiseFactor(0)
               ,denoiseFactorOriginal(0)
               ,bDetailAutoAdjust(false)
               ,detailFactor(0)
               ,detailFactorOriginal(0)
               ,iTargetInterlacingMode(0)
               ,bEnableProcAmp(false)
               ,Brightness(0)
               ,Contrast(0)
               ,Hue(0)
               ,Saturation(0)
               ,bSceneDetectionEnable(false)
               ,bVarianceEnable(false)
               ,bImgStabilizationEnable(false)
               ,istabMode(0)
               ,bFRCEnable(false)
               ,frcModeOrig(0)
               ,bComposite(false)
               ,dstRects(0)
               ,iBackgroundColor(0)
               ,statusReportID(0)
               ,bFieldWeaving(false)
               ,iFieldProcessingMode(0)
#ifndef MFX_CAMERA_FEATURE_DISABLE
               ,bCameraPipeEnabled(false)
               ,bCameraBlackLevelCorrection(false)
               ,CameraBlackLevel()
               ,bCameraWhiteBalaceCorrection(false)
               ,CameraWhiteBalance()
               ,bCameraHotPixelRemoval(false)
               ,CameraHotPixel()
               ,bCCM(false)
               ,CCMParams()
               ,bCameraTCC(false)
               ,CameraTCC()
               ,bCameraRGBtoYUV(false)
               ,CameraRGBToYUV()               
               ,bCameraGammaCorrection(false)
               ,CameraForwardGammaCorrection()
               ,bCameraVignetteCorrection(false)
               ,CameraVignetteCorrection()
               ,bCameraLensCorrection(false)
               ,CameraLensCorrection()
               ,bCamera3DLUT(false)
               ,Camera3DLUT()
#endif
               ,rotation(0)
               ,scalingMode(MFX_SCALING_MODE_DEFAULT)
#ifndef MFX_FUTURE_FEATURE_DISABLE
               ,chromaSiting(MFX_CHROMA_SITING_UNKNOWN)
#endif
               ,bEOS(false)
               ,mirroring(0)
               ,mirroringPosition(0)
               ,scene(VPP_NO_SCENE_CHANGE)
               ,bDeinterlace30i60p(false)
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
               ,gpuHangTrigger(false)
#endif
            {
                   memset(&targetSurface, 0, sizeof(mfxDrvSurface));
                   dstRects.clear();
                   memset(&customRateData, 0, sizeof(CustomRateData));
                   VideoSignalInfoIn.enabled         = false;
                   VideoSignalInfoIn.NominalRange    = MFX_NOMINALRANGE_16_235;
                   VideoSignalInfoIn.TransferMatrix  = MFX_TRANSFERMATRIX_BT601;
                   VideoSignalInfoOut.enabled        = false;
                   VideoSignalInfoOut.NominalRange   = MFX_NOMINALRANGE_16_235;
                   VideoSignalInfoOut.TransferMatrix = MFX_TRANSFERMATRIX_BT601;

                   VideoSignalInfo.clear();
                   VideoSignalInfo.assign(1, VideoSignalInfoIn);
            };

        //surfaces
        mfxDrvSurface  targetSurface;
        mfxU64         targetTimeStamp;

        mfxDrvSurface* pRefSurfaces;
        int            refCount; // refCount == bkwdRefCount + 1 + fwdRefCount

        int            bkwdRefCount; // filled from DdiTask
        int            fwdRefCount;  //

        // params
        mfxI32         iDeinterlacingAlgorithm; //0 - none, 1 - BOB, 2 - advanced (means reference need)
        bool           bFMDEnable;

        bool           bDenoiseAutoAdjust;
        mfxU16         denoiseFactor;
        mfxU16         denoiseFactorOriginal; // Original denoise factor provided by app.

        bool           bDetailAutoAdjust;
        mfxU16         detailFactor;
        mfxU16         detailFactorOriginal;  // Original detail factor provided by app.

        mfxU32         iTargetInterlacingMode;

        bool           bEnableProcAmp;
        mfxF64         Brightness;
        mfxF64         Contrast;
        mfxF64         Hue;
        mfxF64         Saturation;

        bool           bSceneDetectionEnable;

        bool           bVarianceEnable;
        bool           bImgStabilizationEnable;
        mfxU32         istabMode;

        bool           bFRCEnable;
        CustomRateData customRateData;
        mfxU16         frcModeOrig; // Original mode provided by app

        bool           bComposite;
        std::vector<DstRect> dstRects;
        mfxU64         iBackgroundColor;

        mfxU32         statusReportID;

        bool           bFieldWeaving;

        mfxU32         iFieldProcessingMode;

#ifndef MFX_CAMERA_FEATURE_DISABLE
        //  Camera Pipe specific params
        bool                     bCameraPipeEnabled;
        bool                     bCameraBlackLevelCorrection;
        CameraBlackLevelParams   CameraBlackLevel;
        bool                     bCameraWhiteBalaceCorrection;
        CameraWhiteBalanceParams CameraWhiteBalance;
        bool                     bCameraHotPixelRemoval;
        CameraHotPixelRemovalParams CameraHotPixel;
        bool                     bCCM;
        CameraCCMParams          CCMParams;
        bool bCameraTCC;
        CameraTCCParams CameraTCC;
        bool bCameraRGBtoYUV;
        CameraRGBToYUVParams CameraRGBToYUV;
        bool                     bCameraGammaCorrection;
        CameraForwardGammaCorrectionParams CameraForwardGammaCorrection;

        bool                     bCameraVignetteCorrection;
        CameraVignetteCorrectionParams     CameraVignetteCorrection;

        bool                     bCameraLensCorrection;
        CameraLensCorrectionParams         CameraLensCorrection;
        bool                     bCamera3DLUT;
        Camera3DLUTParams        Camera3DLUT;
#endif
        int         rotation;

        mfxU16      scalingMode;

        mfxU16      chromaSiting;

        bool        bEOS;

        SignalInfo VideoSignalInfoIn;   // Common video signal info set on Init
        SignalInfo VideoSignalInfoOut;  // Video signal info for output

        std::vector<SignalInfo> VideoSignalInfo; // Video signal info for each frame in a single run

        int        mirroring;
        int        mirroringPosition;

        vppScene    scene;     // Keep information about scene change
        bool        bDeinterlace30i60p;

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        bool       gpuHangTrigger;
#endif
    };

    class DriverVideoProcessing
    {
    public:

        virtual ~DriverVideoProcessing(void){}

        virtual mfxStatus CreateDevice(VideoCORE * core, mfxVideoParam* par, bool isTemporal = false) = 0;

        virtual mfxStatus ReconfigDevice(mfxU32 indx) = 0;

        virtual mfxStatus DestroyDevice( void ) = 0;

        virtual mfxStatus Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister) = 0;

        virtual mfxStatus QueryTaskStatus(mfxU32 idx) = 0;

        virtual mfxStatus Execute(mfxExecuteParams *pParams) = 0;

        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps ) = 0;

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<UINT> &variance) = 0;

    }; // DriverVideoProcessing

    DriverVideoProcessing* CreateVideoProcessing( VideoCORE* core );

}; // namespace


/*
 * Simple proxy for VPP device/caps create. It simplifies having a single
 * cached vpp processing device accessible thru VideoCORE
 */
class VPPHWResMng
{
public:
    VPPHWResMng(): m_ddi(0), m_caps() {};

    ~VPPHWResMng() { Close(); };

    mfxStatus Close(void){
        m_ddi.reset(0);
        return MFX_ERR_NONE;
    }

    MfxHwVideoProcessing::DriverVideoProcessing *GetDevice(void) const {
        return m_ddi.get();
    };

    mfxStatus SetDevice(MfxHwVideoProcessing::DriverVideoProcessing *ddi){
        MFX_CHECK_NULL_PTR1(ddi);
        MFX_CHECK_STS(Close());
        m_ddi.reset(ddi);
        return MFX_ERR_NONE;
    }

    MfxHwVideoProcessing::mfxVppCaps GetCaps(void) const {
        return m_caps;
    }

    mfxStatus  SetCaps(const MfxHwVideoProcessing::mfxVppCaps &caps){
        m_caps = caps;
        return MFX_ERR_NONE;
    }

    mfxStatus CreateDevice(VideoCORE * core);

    // Just to make ResMang easier to use with existing code of DriverVideoProcessing
    MfxHwVideoProcessing::DriverVideoProcessing *operator->() const {
        return m_ddi.get();
    }

private:
    VPPHWResMng(const VPPHWResMng &);
    VPPHWResMng &operator=(const VPPHWResMng &);

    std::auto_ptr<MfxHwVideoProcessing::DriverVideoProcessing> m_ddi;
    MfxHwVideoProcessing::mfxVppCaps                           m_caps;
};


#endif // __MFX_VPP_BASE_DDI_H
#endif // MFX_ENABLE_VPP
/* EOF */
