//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_aac_encode.h"
#include "mfx_common.h"
#if defined (MFX_ENABLE_AAC_AUDIO_ENCODE)

#include <math.h>
#include "mfx_common_int.h"
#include "mfx_thread_task.h"

class MFX_AAC_Encoder_Utility
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParam( mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus FillAudioParamByUMC(UMC::AACEncoderParams *in, mfxAudioParam *out);
};
struct ThreadAudioTaskInfo
{
    mfxBitstream           *out;
    mfxU32                 taskID; // for task ordering
};

AudioENCODEAAC::AudioENCODEAAC(AudioCORE *core, mfxStatus * sts)
: AudioENCODE()
, m_core(core)
, m_platform(MFX_PLATFORM_SOFTWARE)
, m_isInit(false)
{
    m_frame.Data = NULL;
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

AudioENCODEAAC::~AudioENCODEAAC(void)
{
    Close();
}

mfxStatus AudioENCODEAAC::Init(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
   
    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(par);


    if (CheckAudioParamEncoders(par) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_AUDIO_PARAM;

    m_vPar = *par;

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        m_pAACAudioEncoder.reset(new UMC::AACEncoder());
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
    // allocate memory
    //
    Ipp32u MaxLength = par->mfx.StreamInfo.NumChannels*sizeof(Ipp16s)*1024* 2; // 

    MFX_ZERO_MEM(m_frame);
    m_frame.MaxLength = MaxLength;
    m_frame.Data = new mfxU8[MaxLength];

    UMC::AACEncoderParams params;

    switch(par->mfx.OutputFormat)
    {
    case MFX_AUDIO_AAC_ADTS:
        params.outputFormat = UMC_AAC_ADTS;
        break;
    case MFX_AUDIO_AAC_ADIF:
        params.outputFormat = UMC_AAC_ADIF;
        break;
    case MFX_AUDIO_AAC_RAW:
        params.outputFormat = UMC_AAC_RAW;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    switch(par->mfx.StereoMode)  
    {
    case MFX_AUDIO_AAC_MONO:
        params.stereo_mode = UMC_AAC_MONO;
        params.m_info.channels = 1;
        break;
    case MFX_AUDIO_AAC_LR_STEREO:
        params.stereo_mode = UMC_AAC_LR_STEREO;
        params.m_info.channels = 2;
        break;
    case MFX_AUDIO_AAC_MS_STEREO:
        params.stereo_mode = UMC_AAC_MS_STEREO;
        params.m_info.channels = 2;
        break;
    case MFX_AUDIO_AAC_JOINT_STEREO:
        params.stereo_mode = UMC_AAC_JOINT_STEREO;
        params.m_info.channels = 2;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    switch(par->mfx.NoiseShapingModel)  
    {
    case MFX_AUDIO_AAC_NOISE_MODEL_SIMPLE:
        params.ns_mode = 0;
        break;
    case MFX_AUDIO_AAC_NOISE_MODEL_ADVANCED:
        params.ns_mode = 1;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    switch(par->mfx.CodecProfile)  
    {
    case MFX_PROFILE_AAC_HE:
        params.audioObjectType = AOT_AAC_LC;
        params.auxAudioObjectType = AOT_SBR;
        break;
    case MFX_PROFILE_AAC_LC:
        params.audioObjectType = AOT_AAC_LC;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    //    params.flag_SBR_support_lev = SBR_ENABLE;
    mInData.SetDataSize(0);
    mOutData.SetDataSize(0);

    params.m_info.sample_frequency = par->mfx.StreamInfo.SampleFrequency;
    if (par->mfx.StreamInfo.NumChannels == 1) {
        params.stereo_mode = UMC_AAC_MONO;
        params.m_info.channels = 1;
    } 
    //params.StreamInfo.channels = par->mfx.StreamInfo.Channels;
    params.m_info.bitrate = par->mfx.StreamInfo.Bitrate;
    params.lpMemoryAllocator = NULL;
  
    UMC::Status sts = m_pAACAudioEncoder->Init(&params);
    if(sts != UMC::UMC_OK)
    {
        return   MFX_ERR_UNDEFINED_BEHAVIOR;
    }
       
    m_isInit = true;
     
    return MFX_ERR_NONE;
}



mfxStatus AudioENCODEAAC::Reset(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    m_pAACAudioEncoder->Reset();

    return MFX_ERR_NONE;
}

mfxStatus AudioENCODEAAC::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit || !m_pAACAudioEncoder.get())
        return MFX_ERR_NOT_INITIALIZED;

    mOutData.Close();
    mInData.Close();
    m_pAACAudioEncoder->Close();

    m_isInit = false;
    return MFX_ERR_NONE;
}



mfxStatus AudioENCODEAAC::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
//    mfxStatus  MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);

    return MFX_AAC_Encoder_Utility::Query(core, in, out);
}

mfxStatus AudioENCODEAAC::GetAudioParam(mfxAudioParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    mfxStatus  MFXSts = MFX_ERR_NONE;
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    memcpy(&par->mfx, &m_vPar.mfx, sizeof(mfxInfoMFX));

    UMC::AACEncoderParams params;
    UMC::Status sts = m_pAACAudioEncoder->Init(&params);

    if(sts != UMC::UMC_OK)
    {
        return   MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    MFXSts = MFX_AAC_Encoder_Utility::FillAudioParamByUMC(&params, &m_vPar);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    MFXSts = MFX_AAC_Encoder_Utility::FillAudioParam(&m_vPar, par);
    if (MFXSts != MFX_ERR_NONE)
        return MFXSts;

    par->Protected = m_vPar.Protected;


    return MFX_ERR_NONE;
}


mfxStatus AudioENCODEAAC::QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);
    core;

    int upsample = 1;
    if(par->mfx.CodecProfile == MFX_PROFILE_AAC_HE) upsample = 2;
    request->SuggestedInputSize  = par->mfx.StreamInfo.NumChannels * sizeof(Ipp16s)*1024* upsample;
    request->SuggestedOutputSize = ((768*par->mfx.StreamInfo.NumChannels+9/* ADTS_HEADER */)*sizeof(Ipp8u)+3)&(~3);

    return MFX_ERR_NONE;
}


mfxStatus AudioENCODEAAC::EncodeFrameCheck(mfxAudioFrame *aFrame, mfxBitstream *buffer_out)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    buffer_out;
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    sts = aFrame ? CheckAudioFrame(aFrame) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

//    UMC::Status umcRes = UMC::UMC_OK;

    if (NULL != aFrame)
    {
        //sts = CheckAudioFrame(bs);
        //MFX_CHECK_STS(sts);

        sts = ConstructFrame(aFrame, &m_frame);

        if (MFX_ERR_NONE != sts)
        {
            return sts;
        }

        if (0 == buffer_out->MaxLength) {
            return sts;
        }

        mfxAudioAllocRequest audioAllocRequest;
        sts = QueryIOSize(m_core, &m_vPar, &audioAllocRequest);
        MFX_CHECK_STS(sts);

        if (buffer_out->MaxLength < audioAllocRequest.SuggestedOutputSize) {
            sts = MFX_ERR_NOT_ENOUGH_BUFFER;
        }
    }

    return sts;
}


mfxStatus AudioENCODEAAC::EncodeFrameCheck(mfxAudioFrame *aFrame,
                                           mfxBitstream *buffer_out,
                                           MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus mfxSts = EncodeFrameCheck(aFrame, buffer_out);

    if (MFX_ERR_NONE == mfxSts) // It can be useful to run threads right after first frame receive
    {
        ThreadAudioTaskInfo * info = new ThreadAudioTaskInfo();
        info->out = buffer_out;

        pEntryPoint->pRoutine = &AACENCODERoutine;
        pEntryPoint->pCompleteProc = &AACCompleteProc;
        pEntryPoint->pState = this;
        pEntryPoint->requiredNumThreads = 1; // need only one thread
        pEntryPoint->pParam = info;
    }

    return mfxSts;
}

mfxStatus AudioENCODEAAC::AACENCODERoutine(void *pState, void *pParam,
                                          mfxU32 threadNumber, mfxU32 callNumber)
{
    callNumber;
    threadNumber;
    AudioENCODEAAC &obj = *((AudioENCODEAAC *) pState);
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX::AutoTimer timer("DecodeFrame");

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        ThreadAudioTaskInfo *pTask = (ThreadAudioTaskInfo *) pParam;

        obj.mInData.SetBufferPointer((Ipp8u *)obj.m_frame.Data, obj.m_frame.DataLength);
        obj.mInData.SetDataSize(obj.m_frame.DataLength);
        obj.mOutData.SetBufferPointer( static_cast<Ipp8u *>(pTask->out->Data +pTask->out->DataOffset + pTask->out->DataLength), pTask->out->MaxLength - (pTask->out->DataOffset + pTask->out->DataLength));
        obj.mOutData.MoveDataPointer(0);
        obj.mOutData.SetDataSize(0);

        UMC::Status sts = obj.m_pAACAudioEncoder.get()->GetFrame(&obj.mInData, &obj.mOutData);
        if(sts == UMC::UMC_OK)
        {
            // set data size 0 to the input buffer 
            obj.m_frame.DataLength = 0;

            // set out buffer size;
            //if(pTask->out->MaxLength)  AR need to implement a check of output buffer size
            //pTask->out->DataOffset = 0;
            pTask->out->DataLength += (mfxU32)obj.mOutData.GetDataSize();
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }

    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return mfxRes;

} // mfxStatus AudioENCODEAAC::AACECODERoutine(void *pState, void *pParam,

mfxStatus AudioENCODEAAC::AACCompleteProc(void *pState, void *pParam,
                                          mfxStatus taskRes)
{
    taskRes;
    pParam;
    AudioENCODEAAC &obj = *((AudioENCODEAAC *) pState);

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

//    return MFX_ERR_NONE;
}


mfxStatus AudioENCODEAAC::EncodeFrame(mfxAudioFrame *aFrame, mfxBitstream *buffer_out)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    buffer_out;
    mfxStatus sts;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;
    if(aFrame)
    {
        sts = CheckAudioFrame(aFrame);
        if (sts != MFX_ERR_NONE)
            return sts;
    }
    else
    {
        return MFX_ERR_MORE_DATA;
    }
//    UMC::Status umcRes = UMC::UMC_OK;


    sts = ConstructFrame(aFrame, &m_frame);

    if (MFX_ERR_NONE != sts)
    {
        return sts;
    }

    return sts;
}

// protected methods
mfxStatus AudioENCODEAAC::CopyBitstream(mfxAudioFrame& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    ippsCopy_8u(ptr, bs.Data, bytes);
    bs.DataLength = bytes;
    return MFX_ERR_NONE;
}

void AudioENCODEAAC::MoveBitstreamData(mfxAudioFrame& bs, mfxU32 offset)
{
    ippsMove_8u(bs.Data + offset, bs.Data, bs.DataLength - offset);
    bs.DataLength -= offset;
} 

mfxStatus AudioENCODEAAC::ConstructFrame(mfxAudioFrame *in, mfxAudioFrame *out)
{
    mfxStatus sts = MFX_ERR_NONE;
   
    MFX_CHECK_NULL_PTR2(in, out);

    MFX_CHECK_NULL_PTR2(in->Data, out->Data);


    int upSample = 1;
    if(m_vPar.mfx.CodecProfile == MFX_PROFILE_AAC_HE) upSample = 2;
    Ipp32s FrameSize  = m_vPar.mfx.StreamInfo.NumChannels * sizeof(Ipp16s)*1024* upSample;

    if(FrameSize > (Ipp32s)in->DataLength) 
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        sts = CopyBitstream(*out, in->Data, FrameSize);
        if(sts != MFX_ERR_NONE) 
        {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
        MoveBitstreamData(*in, (mfxU32)(FrameSize));
       
    }

    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Encoder_Utility::FillAudioParam( mfxAudioParam *in, mfxAudioParam *out)
{
    out->AsyncDepth = in->AsyncDepth;
    out->Protected = in->Protected;
    out->mfx.CodecId = out->mfx.CodecId;
    out->mfx.CodecLevel = in->mfx.CodecLevel;
    out->mfx.CodecProfile = in->mfx.CodecProfile;

    memcpy(&(out->mfx.StreamInfo), &(in->mfx.StreamInfo), sizeof(mfxAudioStreamInfo));

    out->mfx.FlagPSSupportLev = in->mfx.FlagPSSupportLev;
    out->mfx.FlagSBRSupportLev = in->mfx.FlagSBRSupportLev;
    out->mfx.Layer = in->mfx.Layer;
    out->mfx.ModeDecodeHEAACprofile = in->mfx.ModeDecodeHEAACprofile;
    out->mfx.ModeDwnsmplHEAACprofile = in->mfx.ModeDwnsmplHEAACprofile;
    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Encoder_Utility::FillAudioParamByUMC(UMC::AACEncoderParams *in, mfxAudioParam *out)
{
    out->mfx.StreamInfo.BitPerSample = (mfxU16)in->m_info.bitPerSample;
    out->mfx.StreamInfo.Bitrate = (mfxU16)in->m_info.bitrate;
    out->mfx.StreamInfo.NumChannels = (mfxU16)in->m_info.channels;
    out->mfx.StreamInfo.SampleFrequency = (mfxU16)in->m_info.sample_frequency;
    return MFX_ERR_NONE;
}

mfxStatus MFX_AAC_Encoder_Utility::Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxAudioParam in1;
        memcpy(&in1, in, sizeof(mfxAudioParam));
        return Query(core, &in1, out);
    }

    memset(&out->mfx, 0, sizeof(mfxAudioInfoMFX));

    if (in)
    {
        // save bitrate from input params
        out->mfx.StreamInfo.Bitrate = in->mfx.StreamInfo.Bitrate;
        
        // save sample frequency from input params
        out->mfx.StreamInfo.SampleFrequency = in->mfx.StreamInfo.SampleFrequency;

        if (in->mfx.CodecId == MFX_CODEC_AAC)
            out->mfx.CodecId = in->mfx.CodecId;


        switch(in->mfx.CodecProfile)
        {
        case MFX_PROFILE_AAC_LC :
        case MFX_PROFILE_AAC_HE :
            out->mfx.CodecProfile = in->mfx.CodecProfile;
            break;
        default:
            out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
        };

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        }

        mfxU16 outputFormat = in->mfx.OutputFormat;
        if (outputFormat == MFX_AUDIO_AAC_ADTS || 
            outputFormat == MFX_AUDIO_AAC_RAW || 
            outputFormat == MFX_AUDIO_AAC_ADIF 
            )
        {
            out->mfx.OutputFormat = in->mfx.OutputFormat;
        }
        else
        {
            if(outputFormat == 0)
            {
                out->mfx.OutputFormat = MFX_AUDIO_AAC_RAW;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        mfxU16 stereoMode = in->mfx.StereoMode;
        if (stereoMode == MFX_AUDIO_AAC_MONO || 
            stereoMode == MFX_AUDIO_AAC_LR_STEREO ||
            stereoMode == MFX_AUDIO_AAC_MS_STEREO ||
            stereoMode == MFX_AUDIO_AAC_JOINT_STEREO 
            )
        {
            //num channels not specified
            switch (in->mfx.StreamInfo.NumChannels)
            {
                case 0 : {
                    if (stereoMode == MFX_AUDIO_AAC_MONO) {
                        out->mfx.StreamInfo.NumChannels = 1;
                    }
                    else {
                        out->mfx.StreamInfo.NumChannels = 2;
                    }
                    break ;
                }
                case 1: {
                    out->mfx.StreamInfo.NumChannels = 1;
                    out->mfx.StereoMode = MFX_AUDIO_AAC_MONO;
                    break;
                }
                case 2: {
                    out->mfx.StreamInfo.NumChannels = 2;
                    //TODO: looks query shouldn't do this, setting default strereo mode
                    if (stereoMode == MFX_AUDIO_AAC_MONO) {
                        out->mfx.StereoMode = MFX_AUDIO_AAC_JOINT_STEREO;
                    }
                }
            } 
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        mfxU32 noiseShapingModel = in->mfx.NoiseShapingModel;
        if (noiseShapingModel == MFX_AUDIO_AAC_NOISE_MODEL_SIMPLE || 
            noiseShapingModel == MFX_AUDIO_AAC_NOISE_MODEL_ADVANCED 
            )
        {
            out->mfx.NoiseShapingModel = in->mfx.NoiseShapingModel;
        }
        else
        {
            if(noiseShapingModel  == 0)
            {
                out->mfx.NoiseShapingModel = MFX_AUDIO_AAC_PS_ENABLE_UR;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }
        }
        if (in->mfx.StreamInfo.BitPerSample != 16 && in->mfx.StreamInfo.BitPerSample != 0) {
            sts = MFX_ERR_UNSUPPORTED;
        } else {
            out->mfx.StreamInfo.BitPerSample = 16;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_AAC;
        out->mfx.CodecProfile = 0;
        out->mfx.CodecLevel = 0;
        out->AsyncDepth = 1;
    }        

    return sts;
}

#endif // MFX_ENABLE_AAC_AUDIO_ENCODE
