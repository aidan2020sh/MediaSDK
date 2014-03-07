//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#include "utest_pch.h"
#include "itransform.h"
#include "transform_adec.h"
#include "exceptions.h"
#include "mock_mfxaudio++.h"

class TransformTestADEC: public ::testing::Test{
public:

    MockPipelineFactory mockFactory;
    MFXAudioSession session;
    MockSample& mockSample;
    MockSample& mockSample2;
    MockSample& mockSampleEOS;
    MockSample& mockSampleToReturn;
    MockMFXAudioDECODE& mockDecoder;
    MockSamplePool &mockPool;
    mfxBitstream bitstream;
    mfxAudioFrame aFrame;
    mfxAudioAllocRequest allocRequest;

    std::auto_ptr<ITransform> pTransform;

    mfxBitstream decHeaderBs;
    mfxAudioParam decHeaderParam;

    Sequence sDFA;

    TransformTestADEC()
        : allocRequest()
        , mockSample(*new MockSample())
        , mockSample2(*new MockSample())
        , mockSampleEOS(*new MockSample())
        , mockDecoder(*new MockMFXAudioDECODE)
        , mockPool(*new MockSamplePool)
        , mockSampleToReturn(*new MockSample()) {
        EXPECT_CALL(mockFactory, CreateAudioDecoder(_)).WillOnce(Return(&mockDecoder));
        EXPECT_CALL(mockFactory, CreateSamplePool(_)).WillOnce(Return(&mockPool));
        pTransform.reset(new Transform<MFXAudioDECODE>(mockFactory, session, 10000));
    }

    void LetQueryReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, Query(_, _)).WillOnce(Return(e));
    }
    void LetDecodeHeaderReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, DecodeHeader(_, _)).WillOnce(DoAll(Invoke(this, &TransformTestADEC::DecodeHeaderSaveParams), Return(e)));
        EXPECT_CALL(mockSample, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    }
    void LetDecoderInitReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, Init(_)).WillOnce(Return(e));
    }
    void LetQueryIOsizeReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, QueryIOSize(_,_)).WillOnce(DoAll(SetArgPointee<1>(allocRequest), Return(e)));
    }
    void LetInitReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, Init(_)).WillOnce(Return(e));
    }
    void LetDecodeFrameAsyncReturn(mfxStatus e){
        EXPECT_CALL(mockDecoder, DecodeFrameAsync(_,_,_)).InSequence(sDFA).WillOnce(Return(e));
        EXPECT_CALL(mockSample, GetAudioFrame()).WillRepeatedly(ReturnRef(aFrame));
    }
    void SetupMetaData(MockSample & smpl, bool sts) {
        EXPECT_CALL(smpl, HasMetaData(META_EOS)).WillOnce(Return(sts));
    }
    void DecodeHeaderSaveParams(mfxBitstream *bs, mfxAudioParam *par) {
        if (bs) decHeaderBs = *bs;
        if (par) decHeaderParam = *par;
    }

};

#if 0
TEST_F(TransformTestADEC, GetSample_returns_false_after_construction){
    SamplePtr pSample;
    EXPECT_FALSE(pTransform->GetSample(pSample));
}

TEST_F(TransformTestADEC, Configure_with_video_param_throw_exception){
    mfxVideoParam vParam = {0};
    EXPECT_THROW(pTransform->Configure(vParam, NULL), IncompatibleParamTypeError);
}

TEST_F(TransformTestADEC, DecodeHeader_fail_throw_exception){
    LetDecodeHeaderReturn(MFX_ERR_NOT_INITIALIZED);
    SetupMetaData(mockSample, false);
    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)), DecodeHeaderError);
}

TEST_F(TransformTestADEC, DecodeHeader_set_valid_codecid){
    mfxAudioParam aParam = {0};
    aParam.mfx.CodecId = MFX_CODEC_AAC;

    LetDecodeHeaderReturn(MFX_ERR_NOT_INITIALIZED);

    pTransform->Configure(aParam, NULL);
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)), DecodeHeaderError);
    EXPECT_EQ((mfxU32)MFX_CODEC_AAC, decHeaderParam.mfx.CodecId);
}

TEST_F(TransformTestADEC, DecodeQuery_fail_throw_exception){
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_UNSUPPORTED);
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)), DecodeQueryError);
}

TEST_F(TransformTestADEC, QueryIOsize_fail_throw_exception) {
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_ABORTED);
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)), DecodeQueryIOSizeError);
}

TEST_F(TransformTestADEC, DecodeInit_fail_throw_exception) {
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    LetInitReturn(MFX_ERR_NOT_INITIALIZED);
    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)), DecodeInitError);
}

TEST_F(TransformTestADEC, Initialization_completed) {
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    LetInitReturn(MFX_ERR_NONE);
    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));
    SetupMetaData(mockSample, false);

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)));
}

TEST_F(TransformTestADEC, DecodeFrameAsyncError_result_in_excetion) {
    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_NONE);
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    LetInitReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);

    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(22));
    EXPECT_CALL(mockPool, FindFreeSample()).WillOnce(ReturnRef(mockSample));
    SetupMetaData(mockSample, false);

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)));
    LetDecodeFrameAsyncReturn(MFX_ERR_UNKNOWN);
    EXPECT_THROW(pTransform->GetSample(outSample), DecodeFrameAsyncError);
}


TEST_F(TransformTestADEC, normal_operation_and_bitstream_alloc_check) {
    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_NONE);
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    LetInitReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(22));
    EXPECT_CALL(mockPool, FindFreeSample()).WillOnce(ReturnRef(mockSample));
    EXPECT_CALL(mockPool, LockSample(_)).WillOnce(Return(&mockSampleToReturn));
    SetupMetaData(mockSample, false);

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)));
    LetDecodeFrameAsyncReturn(MFX_ERR_NONE);
    ASSERT_TRUE(pTransform->GetSample(outSample));
}


TEST_F(TransformTestADEC, EOS_CASE) {
    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    Sequence s1;

    allocRequest.SuggestedOutputSize = 100;
    LetQueryIOsizeReturn(MFX_ERR_NONE);
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    LetDecodeHeaderReturn(MFX_ERR_NONE);
    LetQueryReturn(MFX_ERR_NONE);
    LetInitReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    EXPECT_CALL(mockPool, FindFreeSample()).InSequence(s1).WillOnce(ReturnRef(mockSample));
    EXPECT_CALL(mockPool, LockSample(_)).WillOnce(Return(&mockSampleToReturn));

    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(22));
    SetupMetaData(mockSample, false);

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSample)));
    LetDecodeFrameAsyncReturn(MFX_ERR_NONE);
    ASSERT_TRUE(pTransform->GetSample(outSample));
    ASSERT_FALSE(pTransform->GetSample(outSample));

    //setupEOS
    SetupMetaData(mockSampleEOS, true);
    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr((ISample*)&mockSampleEOS)));

    EXPECT_CALL(mockSample2, GetAudioFrame()).WillRepeatedly(ReturnRef(aFrame));
    EXPECT_CALL(mockDecoder, DecodeFrameAsync(0,_,_)).InSequence(sDFA).WillOnce(Return(MFX_ERR_MORE_DATA));
    EXPECT_CALL(mockPool, FindFreeSample()).InSequence(s1).WillOnce(ReturnRef(mockSample2));
    ASSERT_FALSE(pTransform->GetSample(outSample));

    delete &mockSample2;
}
#endif