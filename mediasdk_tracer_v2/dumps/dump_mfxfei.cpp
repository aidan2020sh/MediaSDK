#include "dump.h"
#include "../loggers/log.h"


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPreEncCtrl &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(Qp);
    DUMP_FIELD(LenSP);
    DUMP_FIELD(MaxLenSP);
    DUMP_FIELD(SubMBPartMask);
    DUMP_FIELD(SubPelMode);
    DUMP_FIELD(InterSAD);
    DUMP_FIELD(IntraSAD);
    DUMP_FIELD(AdaptiveSearch);
    DUMP_FIELD(MVPredictor);
    DUMP_FIELD(MBQp);
    DUMP_FIELD(FTEnable);
    DUMP_FIELD(RefWidth);
    DUMP_FIELD(RefHeight);
    DUMP_FIELD(SearchWindow);
    DUMP_FIELD(DisableMVOutput);
    DUMP_FIELD(DisableStatisticsOutput);
    DUMP_FIELD(PictureType);
    DUMP_FIELD_RESERVED(reserved);

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPreEncMVPredictors &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += "{ L0: {" + ToString(_struct.MB[i].MV[0].x) + "," + ToString(_struct.MB[i].MV[0].y) 
                + "}, L1: {" + ToString(_struct.MB[i].MV[1].x) + "," + ToString(_struct.MB[i].MV[1].y)
                + "}}\n";
        }
        str += "}\n";
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPreEncMV &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += "{\n";
            for (int j = 0; j < GET_ARRAY_SIZE(_struct.MB[i].MV); j++)
            {
                str += "{ L0: {" + ToString(_struct.MB[i].MV[j][0].x) + "," + ToString(_struct.MB[i].MV[j][0].y)
                    + "}, L1: {" + ToString(_struct.MB[i].MV[j][1].x) + "," + ToString(_struct.MB[i].MV[j][1].y)
                    + "}}, ";
            }
            str += "}\n";
        }
        str += "}\n";
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPreEncMBStat &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved0);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            std::string prefix = structName + ".MB[" + ToString(i) + "]";
            str += prefix + ".Inter[0].BestDistortion=" + ToString(_struct.MB[i].Inter[0].BestDistortion) + "\n";
            str += prefix + ".Inter[0].Mode=" + ToString(_struct.MB[i].Inter[0].Mode) + "\n";
            str += prefix + ".Inter[1].BestDistortion=" + ToString(_struct.MB[i].Inter[1].BestDistortion) + "\n";
            str += prefix + ".Inter[1].Mode=" + ToString(_struct.MB[i].Inter[1].Mode) + "\n";

            str += prefix + ".BestIntraDistortion=" + ToString(_struct.MB[i].BestIntraDistortion) + "\n";
            str += prefix + ".IntraMode=" + ToString(_struct.MB[i].IntraMode) + "\n";

            str += prefix + ".NumOfNonZeroCoef=" + ToString(_struct.MB[i].NumOfNonZeroCoef) + "\n";
            str += prefix + ".reserved1=" + ToString(_struct.MB[i].reserved1) + "\n";

            str += prefix + ".SumOfCoef=" + ToString(_struct.MB[i].SumOfCoef) + "\n";

            str += prefix + ".Variance=" + ToString(_struct.MB[i].Variance) + "\n";
            str += prefix + ".PixelAverage=" + ToString(_struct.MB[i].PixelAverage) + "\n";
        }
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncFrameCtrl &_struct)
{
    std::string str;

    DUMP_FIELD(MaxLenSP);
    DUMP_FIELD(LenSP);
    DUMP_FIELD(SubMBPartMask);
    DUMP_FIELD(IntraPartMask);
    DUMP_FIELD(MultiPredL0);
    DUMP_FIELD(MultiPredL1);
    DUMP_FIELD(SubPelMode);
    DUMP_FIELD(InterSAD);
    DUMP_FIELD(IntraSAD);
    DUMP_FIELD(DistortionType);
    DUMP_FIELD(RepartitionCheckEnable);
    DUMP_FIELD(AdaptiveSearch);
    DUMP_FIELD(MVPredictor);
    DUMP_FIELD(NumMVPredictors);
    DUMP_FIELD(PerMBQp);
    DUMP_FIELD(PerMBInput);
    DUMP_FIELD(MBSizeCtrl);
    DUMP_FIELD(RefWidth);
    DUMP_FIELD(RefHeight);
    DUMP_FIELD(SearchWindow);

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMVPredictors &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str +=  structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += dump("", _struct.MB[i]) + ",\n";
        }
        str += "}\n";
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMVPredictors::mfxMB &_struct)
{
    std::string str;

    for (int i = 0; i < GET_ARRAY_SIZE(_struct.RefIdx); i++)
    {
        str += structName + ".RefIdx[" + ToString(i) + "].RefL0=" + ToString(_struct.RefIdx[i].RefL0) + "\n";
        str += structName + ".RefIdx[" + ToString(i) + "].RefL1=" + ToString(_struct.RefIdx[i].RefL1) + "\n";
    }

    DUMP_FIELD(reserved);

    // mfxI16Pair MV[4][2]; /* first index is predictor number, second is 0 for L0 and 1 for L1 */
    
    str += structName + ".MV[]={\n";
    for (int i = 0; i < GET_ARRAY_SIZE(_struct.MV); i++)
    {
        str += "{ L0: {" + ToString(_struct.MV[i][0].x) + "," + ToString(_struct.MV[i][0].y)
            + "}, L1: {" + ToString(_struct.MV[i][1].x) + "," + ToString(_struct.MV[i][1].y)
            + "}}, ";
    }
    str += "}\n";

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncQP &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumQPAlloc); /* size of allocated memory in number of QPs value*/

    if (_struct.QP && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".QP[]=" + dump_reserved_array(_struct.QP, _struct.NumQPAlloc) + "\n";
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMBCtrl &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += dump("", _struct.MB[i]) + ",\n";
        }
        str += "}\n";
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMBCtrl::mfxMB &_struct)
{
    std::string str;

    DUMP_FIELD(ForceToIntra);
    DUMP_FIELD(ForceToSkip);
    DUMP_FIELD(reserved1);

    DUMP_FIELD(reserved2);
    DUMP_FIELD(reserved3);

    DUMP_FIELD(reserved4);
    DUMP_FIELD(TargetSizeInWord);
    DUMP_FIELD(MaxSizeInWord);

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMV &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += "{\n";
            for (int j = 0; j < GET_ARRAY_SIZE(_struct.MB[i].MV); j++)
            {
                str += "{ L0: {" + ToString(_struct.MB[i].MV[j][0].x) + "," + ToString(_struct.MB[i].MV[j][0].y)
                    + "}, L1: {" + ToString(_struct.MB[i].MV[j][1].x) + "," + ToString(_struct.MB[i].MV[j][1].y)
                    + "}}, ";
            }
            str += "}\n";
        }
        str += "}\n";
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMBStat &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += dump("", _struct.MB[i]) + ",\n";
        }
        str += "}\n";
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiEncMBStat::mfxMB &_struct)
{
    std::string str;

    DUMP_FIELD_RESERVED(InterDistortion);
    DUMP_FIELD(BestInterDistortion);
    DUMP_FIELD(BestIntraDistortion);

    return str;
}



std::string DumpContext::dump(const std::string structName, const mfxFeiPakMBCtrl &_struct)
{
    std::string str;

    DUMP_FIELD_RESERVED(reserved0);
    DUMP_FIELD(InterMbMode);
    DUMP_FIELD(MBSkipFlag);
    DUMP_FIELD(Reserved00);
    DUMP_FIELD(IntraMbMode);
    DUMP_FIELD(Reserved01);
    DUMP_FIELD(FieldMbPolarityFlag);
    DUMP_FIELD(MbType);
    DUMP_FIELD(IntraMbFlag);
    DUMP_FIELD(FieldMbFlag);
    DUMP_FIELD(Transform8x8Flag);
    DUMP_FIELD(Reserved02);
    DUMP_FIELD(DcBlockCodedCrFlag);
    DUMP_FIELD(DcBlockCodedCbFlag);
    DUMP_FIELD(DcBlockCodedYFlag);
    DUMP_FIELD(Reserved03);
    DUMP_FIELD(HorzOrigin);
    DUMP_FIELD(VertOrigin);
    DUMP_FIELD(CbpY);
    DUMP_FIELD(CbpCb);
    DUMP_FIELD(CbpCr);
    DUMP_FIELD(QpPrimeY);
    DUMP_FIELD(Reserved30);
    DUMP_FIELD(MbSkipConvDisable);
    DUMP_FIELD(IsLastMB);
    DUMP_FIELD(EnableCoefficientClamp);
    DUMP_FIELD(Direct8x8Pattern);

    if (_struct.IntraMbMode)
    {
        //dword 7,8
        str += structName + ".IntraMB.LumaIntraPredModes[]=" + DUMP_RESERVED_ARRAY(_struct.IntraMB.LumaIntraPredModes) + "\n";
        //dword 9
        str += structName + ".IntraMB.MbIntraStruct=" + ToString(_struct.IntraMB.MbIntraStruct) + "\n";
        str += structName + ".IntraMB.Reserved60=" + ToString(_struct.IntraMB.Reserved60) + "\n";
    }

    if (_struct.InterMbMode)
    {
        //dword 7
        str += structName + ".InterMB.SubMbShapes=" + ToString(_struct.InterMB.SubMbShapes) + "\n";
        str += structName + ".InterMB.SubMbPredModes=" + ToString(_struct.InterMB.SubMbPredModes) + "\n";
        str += structName + ".InterMB.Reserved40=" + ToString(_struct.InterMB.Reserved40) + "\n";
        //dword 8, 9
        for (int i = 0; i < GET_ARRAY_SIZE(_struct.InterMB.RefIdx); i++)
        {
            for (int j = 0; j < GET_ARRAY_SIZE(_struct.InterMB.RefIdx[0]); j++)
            {
                str += structName + ".InterMB.RefIdx[" + ToString(i) + "][" + ToString(j) + "]=" + ToString(_struct.InterMB.RefIdx[i][j]) + "\n";
            }
        }
    }

    //dword 10
    DUMP_FIELD(Reserved70);
    DUMP_FIELD(TargetSizeInWord);
    DUMP_FIELD(MaxSizeInWord);

    DUMP_FIELD_RESERVED(reserved2);
    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPakMBCtrl &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD_RESERVED(reserved);
    DUMP_FIELD(NumMBAlloc);
    if (_struct.MB && Log::GetLogLevel() == LOG_LEVEL_FULL)
    {
        str += structName + ".MB[]={\n";
        for (int i = 0; i < _struct.NumMBAlloc; i++)
        {
            str += dump("", _struct.MB[i]) + ",\n";
        }
        str += "}\n";
    }

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiParam &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(Func);
    DUMP_FIELD_RESERVED(reserved);    

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxPAKInput &_struct)
{
    std::string str;

    DUMP_FIELD_RESERVED(reserved);
    if (_struct.InSurface)
    {
        str += dump(structName + ".InSurface", *_struct.InSurface) + "\n";
    }
    else
    {
        str += structName + ".InSurface=NULL\n";
    }
    DUMP_FIELD(NumFrameL0);
    for (int i = 0; i < _struct.NumFrameL0; i++)
    {
        if (_struct.L0Surface[i])
        {
            str += dump(structName + ".L0Surface[" + ToString(i) + "]", *_struct.L0Surface[i]) + "\n";
        }
        else
        {
            str += structName + ".L0Surface[" + ToString(i) + "]=NULL\n";
        }
    }
    DUMP_FIELD(NumFrameL1);
    for (int i = 0; i < _struct.NumFrameL1; i++)
    {
        if (_struct.L1Surface[i])
        {
            str += dump(structName + ".L1Surface[" + ToString(i) + "]", *_struct.L1Surface[i]) + "\n";
        }
        else
        {
            str += structName + ".L1Surface[" + ToString(i) + "]=NULL\n";
        }
    }

    str += dump_mfxExtParams(structName, _struct) + "\n";

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxPAKOutput &_struct)
{
    std::string str;

    if (_struct.Bs)
    {
        str += dump(structName + ".Bs", *_struct.Bs) + "\n";
    }
    else
    {
        str += structName + ".Bs=NULL\n";
    }

    if (_struct.OutSurface)
    {
        str += dump(structName + ".OutSurface", *_struct.OutSurface) + "\n";
    }
    else
    {
        str += structName + ".OutSurface=NULL\n";
    }

    str += dump_mfxExtParams(structName, _struct) + "\n";

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiSPS &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(Pack);
    DUMP_FIELD(SPSId);
    DUMP_FIELD(Profile);
    DUMP_FIELD(Level);
    DUMP_FIELD(NumRefFrame);
    DUMP_FIELD(WidthInMBs);
    DUMP_FIELD(HeightInMBs);
    DUMP_FIELD(ChromaFormatIdc);
    DUMP_FIELD(FrameMBsOnlyFlag);
    DUMP_FIELD(MBAdaptiveFrameFieldFlag);
    DUMP_FIELD(Direct8x8InferenceFlag);
    DUMP_FIELD(Log2MaxFrameNum);
    DUMP_FIELD(PicOrderCntType);
    DUMP_FIELD(Log2MaxPicOrderCntLsb);
    DUMP_FIELD(DeltaPicOrderAlwaysZeroFlag);

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiPPS &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(Pack);
    DUMP_FIELD(SPSId);
    DUMP_FIELD(PPSId);
    DUMP_FIELD(FrameNum);
    DUMP_FIELD(PicInitQP);
    DUMP_FIELD(NumRefIdxL0Active);
    DUMP_FIELD(NumRefIdxL1Active);
    DUMP_FIELD(ChromaQPIndexOffset);
    DUMP_FIELD(SecondChromaQPIndexOffset);
    DUMP_FIELD(IDRPicFlag);
    DUMP_FIELD(ReferencePicFlag);
    DUMP_FIELD(EntropyCodingModeFlag);
    DUMP_FIELD(ConstrainedIntraPredFlag);
    DUMP_FIELD(Transform8x8ModeFlag);

    return str;
}


std::string DumpContext::dump(const std::string structName, const mfxExtFeiSliceHeader &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";

    DUMP_FIELD(NumSliceAlloc);
    DUMP_FIELD(NumSlice);
    DUMP_FIELD(Pack);

    if (_struct.Slice)
    {
        str += structName + ".Slice[]={\n";
        for (int i = 0; i < _struct.NumSlice; i++)
        {
            str += dump("", _struct.Slice[i]) + ",\n";
        }
        str += "}\n";
    }

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtFeiSliceHeader::mfxSlice &_struct)
{
    std::string str;

    DUMP_FIELD(MBAaddress);
    DUMP_FIELD(NumMBs);
    DUMP_FIELD(SliceType);
    DUMP_FIELD(PPSId);
    DUMP_FIELD(IdrPicId);
    DUMP_FIELD(CabacInitIdc);
    DUMP_FIELD(SliceQPDelta);
    DUMP_FIELD(DisableDeblockingFilterIdc);
    DUMP_FIELD(SliceAlphaC0OffsetDiv2);
    DUMP_FIELD(SliceBetaOffsetDiv2);

    for (int i = 0; i < GET_ARRAY_SIZE(_struct.RefL0); i++)
    {
        str += structName + ".RefL0[" + ToString(i) + "].PictureType=" + ToString(_struct.RefL0[i].PictureType) + "\n";
        str += structName + ".RefL0[" + ToString(i) + "].Index=" + ToString(_struct.RefL0[i].Index) + "\n";
    }

    for (int i = 0; i < GET_ARRAY_SIZE(_struct.RefL1); i++)
    {
        str += structName + ".RefL1[" + ToString(i) + "].PictureType=" + ToString(_struct.RefL1[i].PictureType) + "\n";
        str += structName + ".RefL1[" + ToString(i) + "].Index=" + ToString(_struct.RefL1[i].Index) + "\n";
    }

    return str;
}

