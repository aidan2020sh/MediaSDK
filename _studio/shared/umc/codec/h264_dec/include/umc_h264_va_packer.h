//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_VA_PACKER_H
#define __UMC_H264_VA_PACKER_H

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_mvc_ddi.h"
#include "umc_svc_ddi.h"
#endif

#if defined(_WIN32) || defined(_WIN64)
#pragma warning( disable : 4100 ) // ignore unreferenced parameters in virtual method declaration
#endif

namespace UMC_H264_DECODER
{
    struct H264PicParamSet;
    struct H264ScalingPicParams;
}

namespace UMC
{

class H264DecoderFrame;
class H264Slice;
class H264DecoderFrameInfo;
struct ReferenceFlags;
class TaskSupplier;

class Packer
{

public:

    Packer(VideoAccelerator * va, TaskSupplier * supplier);
    virtual ~Packer();

    virtual Status GetStatusReport(void * pStatusReport, size_t size) = 0;
    virtual Status SyncTask(H264DecoderFrame*, void * error);
    virtual Status QueryTaskStatus(int32_t index, void * status, void * error);
    virtual Status QueryStreamOut(H264DecoderFrame*);

    VideoAccelerator * GetVideoAccelerator() { return m_va; }

    virtual void PackAU(const H264DecoderFrame*, int32_t isTop) = 0;

    virtual void BeginFrame(H264DecoderFrame*, int32_t field) = 0;
    virtual void EndFrame() = 0;

    virtual void Reset() {}

    static Packer * CreatePacker(VideoAccelerator * va, TaskSupplier* supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice) = 0;

    virtual int32_t PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField) = 0;

    virtual void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling) = 0;

protected:

    VideoAccelerator *m_va;
    TaskSupplier     *m_supplier;
};

#ifdef UMC_VA_DXVA

class PackerDXVA2
    : public Packer
{

public:

    PackerDXVA2(VideoAccelerator * va, TaskSupplier * supplier);

    Status GetStatusReport(void * pStatusReport, size_t size);
    virtual Status SyncTask(H264DecoderFrame*, void * error);

    void PackAU(const H264DecoderFrame*, int32_t isTop);

    void BeginFrame(H264DecoderFrame*, int32_t field);
    void EndFrame();

private:

    void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling);

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    int32_t PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField);

protected:

    void AddReferenceFrame(DXVA_PicParams_H264 * pPicParams_H264, int32_t &pos, H264DecoderFrame * pFrame, int32_t reference);
    DXVA_PicEntry_H264 GetFrameIndex(const H264DecoderFrame * frame);

    void PackSliceGroups(DXVA_PicParams_H264 * pPicParams_H264, H264DecoderFrame * frame);
    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice, DXVA_PicParams_H264* pPicParams_H264);
    int32_t PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField, DXVA_Slice_H264_Long* pDXVA_Slice_H264);

    void SendPAVPStructure(int32_t numSlicesOfPrevField, H264Slice *pSlice);

    void CheckData(); //check correctness of encrypted data

    virtual void PackAU(H264DecoderFrameInfo * sliceInfo, int32_t firstSlice, int32_t count);

    void PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_PicParams_H264_MVC* pMVCPicParams_H264);
    void PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_Intel_PicParams_MVC* pMVCPicParams_H264);

    //pointer to the beginning of offset for next slice in HW buffer
    uint8_t * m_pBuf;
    uint32_t  m_statusReportFeedbackCounter;

    DXVA_PicParams_H264* m_picParams;
};

class PackerDXVA2_Widevine
    : public PackerDXVA2
{

public:

    PackerDXVA2_Widevine(VideoAccelerator * va, TaskSupplier * supplier);
    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

private:

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice, DXVA_PicParams_H264* pPicParams_H264);
    void PackAU(H264DecoderFrameInfo * sliceInfo, int32_t firstSlice, int32_t count);
};

#endif // UMC_VA_DXVA


#ifdef UMC_VA_LINUX

class PackerVA
    : public Packer
{
public:
    PackerVA(VideoAccelerator * va, TaskSupplier * supplier);

    Status GetStatusReport(void * pStatusReport, size_t size);
    Status QueryStreamOut(H264DecoderFrame*);

    void PackAU(const H264DecoderFrame*, int32_t isTop);

    void BeginFrame(H264DecoderFrame*, int32_t field);
    void EndFrame();

protected:

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    void FillFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
        int32_t field, int32_t reference, int32_t defaultIndex);

    int32_t FillRefFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
        ReferenceFlags flags, bool isField, int32_t defaultIndex);

    void FillFrameAsInvalid(VAPictureH264 * pic);

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    void PackProcessingInfo(H264DecoderFrameInfo * sliceInfo);
#endif

    enum
    {
        VA_FRAME_INDEX_INVALID = 0x7f
    };

protected:

    void CreateSliceParamBuffer(H264DecoderFrameInfo * pSliceInfo);
    void CreateSliceDataBuffer(H264DecoderFrameInfo * pSliceInfo);

    int32_t PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField);

    void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling);

private:

    bool                       m_enableStreamOut;
};

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
class PackerVA_PAVP : public PackerVA
{

public:

    PackerVA_PAVP(VideoAccelerator * va, TaskSupplier * supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    virtual void CreateSliceDataBuffer(H264DecoderFrameInfo * pSliceInfo);

    virtual int32_t PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField);

protected:

    void PackPavpParams();
};

class PackerVA_Widevine
    : public PackerVA
{

public:

    PackerVA_Widevine(VideoAccelerator * va, TaskSupplier * supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);
    virtual void PackAU(const H264DecoderFrame *pCurrentFrame, int32_t isTop);
};
#endif // #if !defined(MFX_PROTECTED_FEATURE_DISABLE)

#endif // UMC_VA_LINUX

} // namespace UMC

#endif /* __UMC_H264_VA_PACKER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
