/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_factory_default.h"
#include "mfx_renders.h"
#include "mfx_vpp.h"
#include "mfx_allocators_generic.h"
#include "mfx_bitstream_converters.h"
#include "mfx_file_generic.h"
#include "mfx_encode.h"
#include "mfx_video_session.h"
#include "mfx_null_file_writer.h"
#include "mfx_separate_file.h"
#include "mfx_crc_writer.h"
#include "mfx_svc_vpp.h"

MFXPipelineFactory :: MFXPipelineFactory ()
{
    m_sharedRandom = mfx_make_shared<IPPBasedRandom>();
}

IVideoSession     * MFXPipelineFactory ::CreateVideoSession( IPipelineObjectDesc * pParams)
{
    switch (PipelineObjectDescPtr<>(pParams)->Type)
    {
        case VIDEO_SESSION_NATIVE:
        {
            return new MFXVideoSessionImpl();
        }

    default:
        return NULL;
    }
}

IYUVSource      * MFXPipelineFactory ::CreateDecode( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IYUVSource> create(pParams);

    switch (create->Type)
    {
        case DECODER_MFX_NATIVE:
        {
            return new MFXDecoder(create->session);
        }

    default:
        return NULL;
    }
}

IMFXVideoVPP    * MFXPipelineFactory ::CreateVPP( const IPipelineObjectDesc & pParams)
{
    PipelineObjectDescPtr<IMFXVideoVPP> create(pParams);
    
    switch (create->Type)
    {
        case VPP_MFX_NATIVE:
        {
            return new MFXVideoVPPImpl(create->session);
        }
        case VPP_SVC : 
        {
            return new SVCedVpp(create->m_pObject);
        }
    default:
        return NULL;
    }
}

IVideoEncode   * MFXPipelineFactory :: CreateVideoEncode ( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IVideoEncode> create(pParams);

    switch (create->Type)
    {
        case ENCODER_MFX_NATIVE : 
        {
            return new MFXVideoEncode(create->session);
        }
        default:
            return NULL;
    }
}

IMFXVideoRender * MFXPipelineFactory ::CreateRender( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IMFXVideoRender> create (pParams);

    switch (create->Type)
    {
        //TODO:encoder will be created in transcoder lib
        //case RENDER_MFX_ENCODER:
        //{
        //    return new MFXEncodeWRAPPER(create->m_refParams, create->m_status);
        //}
        case RENDER_FWR :
        {
            return NULL;//new MFXFileWriteRender(create->m_core, create->m_status);
        }
    default:
        return NULL;
    }
}

IAllocatorFactory * MFXPipelineFactory::CreateAllocatorFactory( IPipelineObjectDesc * /*pParams*/)
{
    return new DefaultAllocatorFactory();
}

IBitstreamConverterFactory * MFXPipelineFactory::CreateBitstreamCVTFactory( IPipelineObjectDesc * /*pParams*/)
{
    //registering converters defined somewhere
    std::auto_ptr<BitstreamConverterFactory>  fac(new BitstreamConverterFactory());

    fac->Register(new BSConvert<MFX_FOURCC_NV12, MFX_FOURCC_NV12>());
    fac->Register(new BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_NV12>());
    fac->Register(new BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3>());
    fac->Register(new BSConvert<MFX_FOURCC_YUY2, MFX_FOURCC_YUY2>());

    return fac.release();
}

IFile * MFXPipelineFactory::CreatePARReader(IPipelineObjectDesc * /*pParams*/)
{
    return new GenericFile();
}

mfx_shared_ptr<IRandom> MFXPipelineFactory::CreateRandomizer()
{
    return m_sharedRandom;
}

IFile * MFXPipelineFactory::CreateFileWriter( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IFile> create (pParams);

    switch (create->Type)
    {
        case FILE_NULL :
        {
            return new NullFileWriter();
        }
        case FILE_GENERIC :
        {
            return new GenericFile();
        }
        case FILE_SEPARATE :
        {
            return new SeparateFilesWriter(create->m_pObject);
        }
        case FILE_CRC :
        {
            return new CRCFileWriter(create->sCrcFile, create->m_pObject);
        }
    default:
        return NULL;
    }   
}