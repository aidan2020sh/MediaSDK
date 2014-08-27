#include "ts_trace.h"
#include <iomanip>

#define STRUCT_BODY(name, fields)\
        *this  << #name << " {\n";                 \
        inc_offset();                              \
        fields;                                    \
        dec_offset();                              \
        *this << m_off << "}";                     \

#define STRUCT(name, fields)                       \
    tsAutoTrace& tsAutoTrace::operator<< (const name& p) \
    {                                              \
        STRUCT_BODY(name, fields)                  \
        return *this;                              \
    }                                              \
    tsAutoTrace& tsAutoTrace::operator<< (const name* p)\
    {                                              \
        if(p) return *this << (void*)p << " &(" << (name&)*p << ")";\
        return *this << (void*)p;                              \
    }
#define FIELD_T_N(type, _name, _str_name)                           \
    {                                                               \
        mfxU32 n = sizeof(p._name) / sizeof(type);                  \
        if(n > 1) {                                                 \
            *this << m_off << _str_name << "[" << n << "] = {";     \
            type* pp = (type*)&p._name; inc_offset();               \
            for(mfxU32 i = 0; i < n; i ++) {*this << pp[i] << ", ";}\
            dec_offset();*this << "}\n";                            \
        }else{*this << m_off << _str_name << " = " << (type&)p._name << "\n";}\
    }
#define FIELD_T(type, _name) FIELD_T_N(type, _name, #_name)
#define FIELD_S(type, _name) FIELD_T(type, _name)
#define FIELD_A(_num, _name)                                    \
        FIELD_T(mfxU16, _num )                                  \
        if(p._num )                                             \
        {                                                       \
            *this << m_off << #_name " = " << (void*)p._name << " &(\n"; \
            inc_offset();                                       \
            for(mfxU32 i = 0; i < p._num; i ++)                 \
                { *this << m_off << p._name[i] << ", "; }       \
            dec_offset(); *this << "\n" << m_off << ")\n";      \
        }                                                       \
        else { *this << m_off << #_name << " = " << (void*)p._name << "\n"; }

#include "ts_struct_decl.h"

//#undef STRUCT
//#undef FIELD_T
//#undef FIELD_S

/////////////////////////////////////////////////////////////////////////////////////////

tsTrace& tsTrace::operator<<(const mfxExtBuffer& p)
{
    if(!m_interpret_ext_buf)
    {
        STRUCT_BODY(mfxExtBuffer,
            FIELD_T(mfx4CC, BufferId)
            FIELD_T(mfxU32, BufferSz)
        )
        return *this;
    }

    m_interpret_ext_buf = false;

    switch(p.BufferId)
    {
#define EXTBUF(type, id) case id: *this << *(type*)&p; break;
#include "ts_ext_buffers_decl.h"
#undef EXTBUF
    default: *this << p; break;
    }
    m_interpret_ext_buf = true;

    return *this;
}


enum
{
    ENCODER = 1,
    DECODER = 2,
    VPP     = 3
};

tsTrace& tsTrace::operator<<(const mfxVideoParam& p)
{
    m_flags = (!!(p.IOPattern & 0x0F) | 2 * !!(p.IOPattern & 0xF0));

    STRUCT_BODY(mfxVideoParam,
        if(m_flags != VPP || !m_flags)
        {
            FIELD_S(mfxInfoMFX, mfx)
        }
        if(m_flags == VPP || !m_flags)
        {
            FIELD_S(mfxInfoVPP, vpp)
        } 
        FIELD_T(mfxU16        , AsyncDepth  )
        FIELD_T(mfxU16        , Protected   )
        if(!p.IOPattern)
            FIELD_T(mfxU16        , IOPattern   )
        else
        { 
            *this << m_off << "IOPattern = " << p.IOPattern << " = ";
            if(p.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY  ) *this << "IN_VIDEO|";
            if(p.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY ) *this << "IN_SYSTEM|";
            if(p.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY ) *this << "IN_OPAQUE|";
            if(p.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY ) *this << "OUT_VIDEO|";
            if(p.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) *this << "OUT_SYSTEM|";
            if(p.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) *this << "OUT_OPAQUE";
            *this << "\n";
        }
        FIELD_T(mfxU16        , NumExtParam )
        if(p.NumExtParam && p.ExtParam)
        {
            *this << m_off << "ExtParam = " << p.ExtParam << "&(\n";
            inc_offset();
            for(mfxU32 i = 0; i < p.NumExtParam; i ++) 
            {
                *this << m_off << p.ExtParam[i] << ", ";
            }
            dec_offset();
            *this << "\n" << m_off << ")\n";
        }
        else 
        {
            FIELD_T(mfxExtBuffer**, ExtParam)
        }
    )

    m_flags = 0;

    return *this;
}

tsTrace& tsTrace::operator<<(const mfxInfoMFX& p)
{
    STRUCT_BODY(mfxInfoMFX,
        FIELD_T(mfxU16, BRCParamMultiplier)
        FIELD_S(mfxFrameInfo, FrameInfo)
        FIELD_T(mfx4CC, CodecId           )
        FIELD_T(mfxU16, CodecProfile      )
        FIELD_T(mfxU16, CodecLevel        )
        FIELD_T(mfxU16, NumThread         )

        if(m_flags == ENCODER)
        {
            if(p.CodecId == MFX_CODEC_JPEG)
            {
                FIELD_T(mfxU16, Interleaved    )
                FIELD_T(mfxU16, Quality        )
                FIELD_T(mfxU16, RestartInterval)
            }
            else
            {
                int f = false;
                FIELD_T(mfxU16, TargetUsage      )
                FIELD_T(mfxU16, GopPicSize       )
                FIELD_T(mfxU16, GopRefDist       )
                FIELD_T(mfxU16, GopOptFlag       )
                FIELD_T(mfxU16, IdrInterval      )
                FIELD_T(mfxU16, RateControlMethod)
                FIELD_T(mfxU16, BufferSizeInKB   )

                if(!p.RateControlMethod || p.RateControlMethod == MFX_RATECONTROL_CQP)
                {
                    FIELD_T(mfxU16, QPI              )
                    FIELD_T(mfxU16, QPP              )
                    FIELD_T(mfxU16, QPB              )
                    f++;
                }
                    
                if(!p.RateControlMethod || p.RateControlMethod == MFX_RATECONTROL_AVBR)
                {
                    FIELD_T(mfxU16, Accuracy         )
                    FIELD_T(mfxU16, TargetKbps       )
                    FIELD_T(mfxU16, Convergence      )
                    f++;
                }
                    
                if(!p.RateControlMethod || p.RateControlMethod == MFX_RATECONTROL_ICQ)
                {
                    FIELD_T(mfxU16, ICQQuality      )
                    f++;
                }
                if(!p.RateControlMethod || !f)
                {
                    FIELD_T(mfxU16, InitialDelayInKB )
                    FIELD_T(mfxU16, TargetKbps       )
                    FIELD_T(mfxU16, MaxKbps          )
                }

                FIELD_T(mfxU16, NumSlice         )
                FIELD_T(mfxU16, NumRefFrame      )
                FIELD_T(mfxU16, EncodedOrder     )
            }
        }
        else 
        {
            if(p.CodecId == MFX_CODEC_JPEG)
            {
                FIELD_T(mfxU16, JPEGChromaFormat )
                FIELD_T(mfxU16, Rotation         )
                FIELD_T(mfxU16, JPEGColorFormat  )
                FIELD_T(mfxU16, InterleavedDec   )
            }
            else
            {
                FIELD_T(mfxU16, DecodedOrder       )
                FIELD_T(mfxU16, ExtendedPicStruct  )
                FIELD_T(mfxU16, TimeStampCalc      )
                FIELD_T(mfxU16, SliceGroupsPresent )
            }
        }
    )
        
    return *this;
}

#define ENUM(p, fields) switch(p){fields default: *this << "<unknown>"; break;} std::hex(*this); *this << "(0x" << (mfxU32)p  << ")"; std::dec(*this);
#define FIELD_E(f) case f: *this << #f; break;

tsTrace& tsTrace::operator<<(mfxStatus& p)
{
    ENUM(p,
        FIELD_E(MFX_ERR_NONE)
        FIELD_E(MFX_ERR_UNKNOWN)
        FIELD_E(MFX_ERR_NULL_PTR)
        FIELD_E(MFX_ERR_UNSUPPORTED)
        FIELD_E(MFX_ERR_MEMORY_ALLOC)
        FIELD_E(MFX_ERR_NOT_ENOUGH_BUFFER)
        FIELD_E(MFX_ERR_INVALID_HANDLE)
        FIELD_E(MFX_ERR_LOCK_MEMORY)
        FIELD_E(MFX_ERR_NOT_INITIALIZED)
        FIELD_E(MFX_ERR_NOT_FOUND)
        FIELD_E(MFX_ERR_MORE_DATA)
        FIELD_E(MFX_ERR_MORE_SURFACE)
        FIELD_E(MFX_ERR_ABORTED)
        FIELD_E(MFX_ERR_DEVICE_LOST)
        FIELD_E(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        FIELD_E(MFX_ERR_INVALID_VIDEO_PARAM)
        FIELD_E(MFX_ERR_UNDEFINED_BEHAVIOR)
        FIELD_E(MFX_ERR_DEVICE_FAILED)
        FIELD_E(MFX_ERR_MORE_BITSTREAM)
        FIELD_E(MFX_WRN_IN_EXECUTION)
        FIELD_E(MFX_WRN_DEVICE_BUSY)
        FIELD_E(MFX_WRN_VIDEO_PARAM_CHANGED)
        FIELD_E(MFX_WRN_PARTIAL_ACCELERATION)
        FIELD_E(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        FIELD_E(MFX_WRN_VALUE_NOT_CHANGED)
        FIELD_E(MFX_WRN_OUT_OF_RANGE)
        FIELD_E(MFX_WRN_FILTER_SKIPPED)
        FIELD_E(MFX_TASK_WORKING)
        FIELD_E(MFX_TASK_BUSY)
    )

    return *this;
}


rawdata hexstr(const void* d, unsigned int s)
{
    rawdata r = {(unsigned char*)d, s};
    return r;
}

std::ostream &operator << (std::ostream &os, rawdata p){
    char f = os.fill();
    os.fill('0');
    os << std::hex;
    for(unsigned int i = 0; i < p.size; i++)
        os << std::setw(2) << (unsigned int)(p.data[i]);
    os << std::dec;
    os.fill(f);
    return os;
}

tsTrace& tsTrace::operator<<(const mfxPluginUID& p)
{
    *this << hexstr(p.Data, sizeof(p.Data));
    return *this;
}


tsTrace& tsTrace::operator<<(const mfxFrameData& p)
{
    STRUCT_BODY(mfxFrameData,
        FIELD_T(mfxU16        , NumExtParam )
        if(p.NumExtParam && p.ExtParam)
        {
            *this << m_off << "ExtParam = " << p.ExtParam << "&(\n";
            inc_offset();
            for(mfxU32 i = 0; i < p.NumExtParam; i ++) 
            {
                *this << m_off << p.ExtParam[i] << ", ";
            }
            dec_offset();
            *this << "\n" << m_off << ")\n";
        }
        else 
        {
            FIELD_T(mfxExtBuffer**, ExtParam)
        }
        FIELD_T(mfxU16  , PitchHigh  )
        FIELD_T(mfxU64  , TimeStamp  )
        FIELD_T(mfxU32  , FrameOrder )
        FIELD_T(mfxU16  , Locked     )
        FIELD_T(mfxU16  , Pitch      )
        FIELD_T(mfxU16  , PitchLow   )
        FIELD_T_N(mfxU8 * , Y, "Y/Y16/R")
        FIELD_T_N(mfxU8 * , U, "U/UV/G/U16/VU/CbCr/CrCb/Cb")
        FIELD_T_N(mfxU8 * , V, "V/B/V16/Cr")
        FIELD_T(mfxU8 * , A          )
        FIELD_T(mfxMemId,  MemId     )
        FIELD_T(mfxU16  , Corrupted  )
        FIELD_T(mfxU16  , DataFlag   )
    )
        
    return *this;
}

tsTrace& tsTrace::operator<<(const mfxExtEncoderROI& p)
{
    STRUCT_BODY(mfxExtEncoderROI,
        FIELD_S(mfxExtBuffer, Header)
        FIELD_T(mfxU16  , NumROI  )
        for(mfxU32 i = 0; i < p.NumROI; ++i)
        {
            FIELD_S(mfxExtEncoderROI_Entry, ROI[i])
        }
    )

    return *this;
}

tsTrace& tsTrace::operator<<(const mfxExtCamGammaCorrection& p)
{
    STRUCT_BODY(mfxExtCamGammaCorrection,
        FIELD_S(mfxExtBuffer, Header     )
        FIELD_T(mfxU16      , Mode       )
        FIELD_T(mfxU16      , reserved1  )
        FIELD_T(mfxF64      , GammaValue )
        FIELD_T(mfxU16      , NumPoints  )
        for(mfxU32 i = 0; i < p.NumPoints; ++i)
        {
            FIELD_T(mfxU16, GammaPoint[i])
            FIELD_T(mfxU16, GammaCorrected[i])
        }
    )

    return *this;
}
tsTrace& tsTrace::operator<<(const mfxExtCamVignetteCorrection& p)
{
    STRUCT_BODY(mfxExtCamVignetteCorrection,
        FIELD_S(mfxExtBuffer,                 Header       )
        FIELD_T(mfxU32,                       Width        )
        FIELD_T(mfxU32,                       Height       )
        FIELD_T(mfxU32,                       Pitch        )
        FIELD_T(mfxU32,                       MaskPrecision)
        if(p.CorrectionMap)
        {
            FIELD_S(mfxVignetteCorrectionParams*, CorrectionMap)
        }
    )

    return *this;
}

tsTrace& tsTrace::operator<<(const mfxEncodeCtrl& p)
{
    STRUCT_BODY(mfxEncodeCtrl,
        FIELD_S(mfxExtBuffer  , Header     )
        FIELD_T(mfxU16        , SkipFrame  )
        FIELD_T(mfxU16        , QP         )
        FIELD_T(mfxU16        , FrameType  )

        FIELD_A(NumExtParam, ExtParam)
        FIELD_A(NumPayload,  Payload)
    )

    return *this;
}

tsTrace& tsTrace::operator<<(const mfxExtAVCRefLists& p)
{
    STRUCT_BODY(mfxExtAVCRefLists,
        FIELD_S(mfxExtBuffer, Header          )
        FIELD_A(NumRefIdxL0Active, RefPicList0)
        FIELD_A(NumRefIdxL1Active, RefPicList1)
    )
    return *this;
}
