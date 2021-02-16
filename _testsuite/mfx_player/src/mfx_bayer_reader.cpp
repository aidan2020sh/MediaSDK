/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014-2020 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef MFX_ONEVPL 

#include "mfx_bayer_reader.h"

BayerVideoReader::BayerVideoReader(sStreamInfo *pParams)
{
    m_bInited  = false;
    m_bInfoSet = false;
    m_sInfo = {};
    m_BayerType = (mfxCamBayerFormat)0;

    if (NULL != pParams)
    {
        m_bInfoSet = true;
        m_sInfo = *pParams;
        m_BayerType = (mfxCamBayerFormat)pParams->videoType;
    }
    m_FileNum = 0;
    m_fSrc     = 0;
    m_pRawData = nullptr;
    m_FileFrameHeight = 0;
    m_FileFramePitchBytes = 0;
    m_RawWidthU32 = 0;
}


#define IN_HOR_OFFSET 0

mfxStatus BayerVideoReader::Init(const vm_char *strFileName)
{
    Close();

    vm_string_strcpy_s(m_FileNameBase, (sizeof(m_FileNameBase)/sizeof(vm_char)), strFileName);
    m_FileNum = 0;

    m_bInited = true;

    return MFX_ERR_NONE;

}


void BayerVideoReader::Close()
{
    if (m_fSrc != 0)
    {
        vm_file_close(m_fSrc);
        m_fSrc = 0;
    }

    if (m_pRawData)
    {
        delete[] m_pRawData;
        m_pRawData = 0;
    }
}


mfxStatus BayerVideoReader::ReadNextFrame(mfxBitstream2 &bs)
{
    CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
    bs.DataOffset = 0;
    bs.TimeStamp = MFX_TIME_STAMP_INVALID;

    const vm_char* pExt;

    switch (m_BayerType) {
    case MFX_CAM_BAYER_GRBG:
        pExt = VM_STRING("gr16");
        break;
    case MFX_CAM_BAYER_GBRG:
        pExt = VM_STRING("gb16");
        break;
    case MFX_CAM_BAYER_BGGR:
        pExt = VM_STRING("bg16");
        break;
    case MFX_CAM_BAYER_RGGB:
    default:
        pExt = VM_STRING("rg16");
        break;
    }

    /*
     * The following file name structure used: <base_name><fileNameIndex>.<extension> ("%s%08d.%s")
     * <base_name> length is up to the size of m_FileNameBase (MAX_PATH).
     * <fileNameIndex> length is from 8 to 11 characters.
     * <extension> length is 4 characters.
     * Hence, the max length of the file name, including the terminating null-character, is (MAX_PATH + 16).
     */
    const int fileNameIndex = m_FileNum;
    vm_char fname[sizeof(m_FileNameBase) + 16];

    vm_string_snprintf(fname, sizeof(fname) - 1, VM_STRING("%s%08d.%s"), m_FileNameBase, fileNameIndex, pExt);

    Ipp64u file_size;
    Ipp32u file_attr;
    vm_file_getinfo(fname, &file_size, &file_attr);

    m_fSrc = vm_file_open(fname, VM_STRING("rb"));
    if ( ! m_fSrc )
    {
        if (bs.DataFlag & MFX_BITSTREAM_EOS)
            return MFX_ERR_UNKNOWN;
        bs.DataFlag |= MFX_BITSTREAM_EOS;
        return MFX_ERR_MORE_DATA;
    }

    if ( (mfxU32)file_size > bs.MaxLength){
        // Extend bitsream to fit size of the read frame
        mfxU32 nNewLen = (1024 * 1024) * ( (file_size ) / (1024 * 1024) + (((file_size) % (1024 * 1024)) ? 1 : 0));
        mfxU8 * p;
        p = new mfxU8[nNewLen], MFX_ERR_MEMORY_ALLOC;

        bs.MaxLength = nNewLen;
        memcpy(p, bs.Data + bs.DataOffset, bs.DataLength);
        delete [] bs.Data;
        bs.Data = p;
        bs.DataOffset = 0;
    }


    if (!vm_file_read(bs.Data, 1, file_size, m_fSrc))
    {
        return MFX_ERR_UNKNOWN;
    }
    bs.DataLength = file_size;
    m_FileNum++;

    vm_file_close(m_fSrc);

    return MFX_ERR_NONE;
}

#endif // #ifndef MFX_ONEVPL
