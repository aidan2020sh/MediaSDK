/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"
#include "mfx_pipeline_utils.h"
#include "ippdc.h"
#include "ippcp.h"
#include "vm_file.h"

//wrapping to file writer render provided crc calc+file writing
class CRCFileWriter
    : public InterfaceProxy<IFile>
{
    typedef InterfaceProxy<IFile> base;
    IMPLEMENT_CLONE(CRCFileWriter);
    mfxU32  m_crc32;
    tstring m_crcFile;
    int m_frameIndex = 0;
public:
    CRCFileWriter(const tstring &sCrcFile, std::unique_ptr<IFile> &&pTargetFile)
        : base(std::move(pTargetFile))
        , m_crc32()
        , m_crcFile(sCrcFile)
    {
        int size;
        ippsMD5GetSize(&size);
        ctx = (IppsMD5State *)malloc(size);
        if ( ctx )
            ippsMD5Init(ctx);
    }
    ~CRCFileWriter()
    {
        if (ctx)
        {
            FillMD5Result(ctx);
            PrintInfo(VM_STRING("MD5"), md5_result);
            WriteMd5ToFile(md5_result);
        }
        PrintInfo(VM_STRING("CRC"), VM_STRING("%08X"), m_crc32 );
        WriteCrcToFile();

        if (ctx)
            free(ctx);
    }
    virtual mfxStatus Write(mfxU8* p, mfxU32 size)
    {
        if (NULL != p)
        {
            ippsCRC32_8u(p, size, &m_crc32);
            if (ctx) ippsMD5Update(p, size, ctx);
        }
        return base::Write(p, size);
    }
    virtual mfxStatus Write(mfxBitstream * pBs)
    {
        if (NULL != pBs)
        {
            ippsCRC32_8u(pBs->DataOffset + pBs->Data, pBs->DataLength, &m_crc32);
            if (ctx) ippsMD5Update(pBs->DataOffset + pBs->Data, pBs->DataLength, ctx);
        }
        return base::Write(pBs);
    }

    virtual void FrameBoundary()
    {
        if (ctx)
        {
            int size;
            ippsMD5GetSize(&size);
            IppsMD5State *copyCtx = (IppsMD5State *)malloc(size);
            ippsMD5Duplicate(ctx, copyCtx);
            FillMD5Result(copyCtx);
            free(copyCtx);

            vm_char name[128];

            vm_string_sprintf_s(name, VM_STRING("\n#%d-md5"), m_frameIndex++);
            PrintInfo(name,  md5_result);
            // Flush stdout every frame to get the latest results
            // in case of crash or exception.
            fflush(stdout);
        }
    }

protected:
    void FillMD5Result(IppsMD5State *md5Ctx)
    {
        if (md5Ctx)
        {
            mfxU8 md5_digest[MD5_DIGEST_LENGTH];
            ippsMD5Final(md5_digest, md5Ctx);

            for (int i = 0; i < 16; i++)
            {
                vm_string_sprintf(&md5_result[i * 2], VM_STRING("%02x"), md5_digest[i]);
            }
            md5_result[32] = 0;
        }
    }
    CRCFileWriter(const CRCFileWriter &that)
        : base((const base&)that)
        , m_crc32()
    {
        int size;
        ippsMD5GetSize(&size);
        ctx = (IppsMD5State *)malloc(size);
        if ( ctx )
            ippsMD5Init(ctx);
    }
    mfxStatus WriteCrcToFile()
    {
        if (!m_crcFile.empty())
        {
            vm_file *f ;
            MFX_CHECK_VM_FOPEN(f, m_crcFile.c_str(), VM_STRING("wb"));
        
            if (!vm_file_write(&m_crc32, 1, sizeof(m_crc32), f))
                return MFX_ERR_UNKNOWN;
            vm_file_fclose(f);
        }
        return MFX_ERR_NONE;
    }
    tstring Md5FileName() {
        tstring md5File;
        if (m_crcFile.size() > 4) {
            tstring fileExtension = tstring(m_crcFile.end() - 4, m_crcFile.end());
            if (fileExtension.compare(tstring(VM_STRING(".crc"))) == 0) {
                md5File = tstring(m_crcFile.begin(), (m_crcFile.end() - 4)) + VM_STRING(".md5");
            }
        }
        return md5File;
    }
    mfxStatus WriteMd5ToFile(vm_char md5[33]) {
        tstring md5File = Md5FileName();
        if (!md5File.empty()) {
            vm_file *f;
            MFX_CHECK_VM_FOPEN(f, md5File.c_str(), VM_STRING("wb"));
            if (vm_file_fprintf(f, VM_STRING("%s"), md5) == 0)
                return MFX_ERR_UNKNOWN;
            vm_file_fclose(f);
        }
        return MFX_ERR_NONE;
    }
    IppsMD5State *ctx;
    vm_char md5_result[33];
};
