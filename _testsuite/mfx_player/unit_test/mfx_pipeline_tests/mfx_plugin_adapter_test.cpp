/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"

    TEST_FIXTURE(Fixture, Submit)
    {
        //const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task
        mfxThreadTask task ;
        internalAPI.Submit(internalAPI.pthis, (mfxHDL*)NULL, 0, (mfxHDL*)NULL, 0, &task);
        CHECK(m._Submit.WasCalled());
    }
    TEST_FIXTURE(Fixture, Execute)
    {
        internalAPI.Execute(internalAPI.pthis, mfxThreadTask(), 0, 0);
        CHECK(m._Execute.WasCalled());
    }
    TEST_FIXTURE(Fixture, FreeResources)
    {
        internalAPI.FreeResources(internalAPI.pthis, mfxThreadTask(), MFX_ERR_NONE);
        CHECK(m._FreeResources.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, Query)
    {
        mfxVideoParam vpar;
        internalAPI.Video->Query(internalAPI.pthis, &vpar, &vpar);
        CHECK(m._Query.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, Reset)
    {
        mfxVideoParam vpar;
        internalAPI.Video->Reset(internalAPI.pthis, &vpar);
        CHECK(m._Reset.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, GetVideoParam)
    {
        mfxVideoParam par;
        internalAPI.Video->GetVideoParam(internalAPI.pthis, &par);
        CHECK(m._GetVideoParam.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, DecodeHeader)
    {
        mfxBitstream bs;
        mfxVideoParam par;
        internalAPI.Video->DecodeHeader(internalAPI.pthis, &bs, &par);
        CHECK(m._DecodeHeader.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, GetPayload)
    {
        mfxU64 ts;
        mfxPayload payload;
        internalAPI.Video->GetPayload(internalAPI.pthis, &ts, &payload);
        CHECK(m._GetPayload.WasCalled());
    }
    TEST_FIXTURE(FixtureDec, DecodeFrameSubmit)
    {
        mfxBitstream bs;
        mfxFrameSurface1 surface_work;
        mfxFrameSurface1 *surface_out;
        mfxThreadTask task;
        internalAPI.Video->DecodeFrameSubmit(internalAPI.pthis, &bs,&surface_work,&surface_out,&task);
        CHECK(m._DecodeFrameSubmit.WasCalled());
    }

    TEST_FIXTURE(FixtureEnc, EncodeFrameSubmit)
    {
        mfxBitstream bs;
        mfxFrameSurface1 surface;
        mfxThreadTask task;
        mfxEncodeCtrl ctrl;
        internalAPI.Video->EncodeFrameSubmit(internalAPI.pthis, &ctrl, &surface,&bs,&task);
        CHECK(m._EncodeFrameSubmit.WasCalled());
    }
}
