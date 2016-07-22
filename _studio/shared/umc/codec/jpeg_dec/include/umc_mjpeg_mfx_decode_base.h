/* ////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_BASE_H
#define __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_BASE_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include <memory>

#include "umc_structures.h"
#include "umc_video_decoder.h"
#include "umc_media_data_ex.h"
#include "umc_frame_data.h"
#include "umc_frame_allocator.h"
#include "jpegdec_base.h"

#include "mfxvideo++int.h"

namespace UMC
{

typedef struct
{
    ChromaType colorFormat;
    size_t UOffset;
    size_t VOffset;
} ConvertInfo;

class MJPEGVideoDecoderBaseMFX
{
public:
    // Default constructor
    MJPEGVideoDecoderBaseMFX(void);

    // Destructor
    virtual ~MJPEGVideoDecoderBaseMFX(void);

    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams* init);

    // Reset decoder to initial state
    virtual Status Reset(void);

    // Close decoding & free all allocated resources
    virtual Status Close(void);

    ChromaType GetChromaType();

    virtual Status GetFrame(UMC::MediaDataEx *, UMC::FrameData** , const mfxU32  ) { return MFX_ERR_NONE; };

    virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

    Status FillQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables);

    Status FillHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables);

    Status DecodeHeader(MediaData* in);

    Status FillVideoParam(mfxVideoParam *par, bool full);

    // Skip extra data at the begiging of stream
    Status FindStartOfImage(MediaData * in);

    // All memory sizes should come in size_t type
    Status _GetFrameInfo(const Ipp8u* pBitStream, size_t nSize);

    Status SetRotation(Ipp16u rotation);

protected:

    JCOLOR GetColorType();
    virtual void AdjustFrameSize(IppiSize & size);
    // Allocate the destination frame
    virtual Status AllocateFrame() { return MFX_ERR_NONE; };

    bool                    m_IsInit;
    bool                    m_interleaved;
    bool                    m_interleavedScan;
    VideoDecoderParams      m_DecoderParams;

    FrameData               m_frameData;
    JCOLOR                  m_color;
    Ipp16u                  m_rotation;

    IppiSize                m_frameDims;
    int                     m_frameSampling;

    // JPEG decoders allocated
    std::auto_ptr<CJPEGDecoderBase> m_decoder;
    CJPEGDecoderBase * m_decBase;

    FrameAllocator *        m_frameAllocator;
};

inline mfxU16 GetMFXChromaFormat(ChromaType type)
{
    mfxU16 chromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
    switch(type)
    {
    case CHROMA_TYPE_YUV400:
        chromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
        break;
    case CHROMA_TYPE_YUV420:
        chromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case CHROMA_TYPE_YUV422H_2Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422H;
        break;
    case CHROMA_TYPE_YUV444:
    case CHROMA_TYPE_RGB:
        chromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    case CHROMA_TYPE_YUV411:
        chromaFormat = MFX_CHROMAFORMAT_YUV411;
        break;
    case CHROMA_TYPE_YUV422V_2Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422V;
        break;
    case CHROMA_TYPE_YUV422H_4Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422H;
        break;
    case CHROMA_TYPE_YUV422V_4Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422V;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return chromaFormat;
}

inline mfxU16 GetMFXColorFormat(JCOLOR color)
{
    mfxU16 colorFormat = MFX_JPEG_COLORFORMAT_UNKNOWN;
    switch(color)
    {
    case JC_UNKNOWN:
        colorFormat = MFX_JPEG_COLORFORMAT_UNKNOWN;
        break;
    case JC_GRAY:
        colorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
        break;
    case JC_YCBCR:
        colorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
        break;
    case JC_RGB:
        colorFormat = MFX_JPEG_COLORFORMAT_RGB;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return colorFormat;
}

inline JCOLOR GetUMCColorType(Ipp16u chromaFormat, Ipp16u colorFormat)
{
    JCOLOR color = JC_UNKNOWN;

    switch(colorFormat)
    {
    case MFX_JPEG_COLORFORMAT_UNKNOWN:
        color = JC_UNKNOWN;
        break;
    case MFX_JPEG_COLORFORMAT_YCbCr:
        if(chromaFormat == MFX_CHROMAFORMAT_MONOCHROME)
        {
            color = JC_GRAY;
        }
        else
        {
            color = JC_YCBCR;
        }
        break;
    case MFX_JPEG_COLORFORMAT_RGB:
        color = JC_RGB;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return color;
}

} // end namespace UMC

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif //__UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_BASE_H
