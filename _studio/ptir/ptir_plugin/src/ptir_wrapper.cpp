/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_wrapper.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"

mfxStatus MFX_PTIR_Plugin::PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out)
{

    if(!b_firstFrameProceed)
    {
        uiStart = 1;
        memcpy(frmBuffer[0]->ucMem, frmBuffer[uiSupBuf]->ucMem, frmBuffer[uiSupBuf]->uiSize);
        frmBuffer[0]->frmProperties.tindex = uiStart + 1;
        sadCalc_I420_frame(frmBuffer[0],frmBuffer[0]);
        Rs_measurement(frmBuffer[0]);
        Artifacts_Detection_frame(frmBuffer, 0);
        frmBuffer[0]->frmProperties.processed = FALSE;

        frmBuffer[0]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[0]->plaY.ucStats.ucSAD[1] = 99.999;

        uiCur = 1;
        uiBufferCount = 1;
        patternFound = FALSE;
        //bFullFrameRate = FALSE;
        uiDispatch = 0;
        mainPattern.blendedCount = 0.0;
        mainPattern.uiIFlush = 0;
        mainPattern.uiPFlush = 0;

        dBaseTime = (1 / dFrameRate) * 1000;
        dDeIntTime = dBaseTime / 2;
        frmBuffer[0]->frmProperties.timestamp = dTimeStamp;
        dBaseTimeSw = dBaseTime;
        b_firstFrameProceed = true;

        uiFrame = uiStart;

        return MFX_ERR_MORE_DATA;
    }

    //for (uiFrame = uiStart, uiFrameOut = 0, uiCur = 1; uiFrame < uiCount; ++uiFrame)
    //{
        if(surf_in)
        {
            uiNext = uiCur - 1;

            uiProgress = 0;
            uiTimer = 0;

            if (uiFrame < uiStart + uiCount)
            {
                ////QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Loading
                //----------------------------------------
                //ferror = ReadFile(hIn, pucIO, uiSize, &uiBytesRead, NULL);

                ////QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Picture Format Conversion
                //----------------------------------------
                //Convert_to_I420(pucIO, frmBuffer[uiSupBuf], pcFormat, dFrameRate);

                memcpy(frmBuffer[uiCur]->ucMem, frmBuffer[uiSupBuf]->ucMem, frmBuffer[uiSupBuf]->uiSize);
                frmBuffer[uiCur]->frmProperties.tindex = uiFrame + 1;
                sadCalc_I420_frame(frmBuffer[uiCur], frmBuffer[uiNext]);
                Rs_measurement(frmBuffer[uiCur]);
                frmBuffer[uiCur]->frmProperties.processed = TRUE;
                dSAD = frmBuffer[uiCur]->plaY.ucStats.ucSAD;
                dRs = frmBuffer[uiCur]->plaY.ucStats.ucRs;
    //#if PRINTX == 2
    //            printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", uiFrame + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
    //#endif
                if ((uiCur == BUFMINSIZE - 1) || (uiFrame == (uiCount - 1)))
                {
                    Analyze_Buffer_Stats(frmBuffer, &mainPattern, &uiDispatch, &bisInterlaced);
                    if(mainPattern.ucPatternFound && !bisInterlaced)
                    {
                        if(mainPattern.ucPatternType == 0)
                            dOutBaseTime = dBaseTime;
                        else
                            dOutBaseTime = (dBaseTime * 5 / 4);

                        for(mfxU32 i = 0; i < min(uiDispatch,uiCur + 1); i++)
                        {
                            if(!frmBuffer[i]->frmProperties.drop)
                            {
                                CheckGenFrame(frmBuffer, i, mainPattern.ucPatternType);
                                Prepare_frame_for_queue(&frmIn,frmBuffer[i], uiWidth, uiHeight);
                                memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[i]->plaY.ucStats.ucRs,sizeof(double) * 10);

                                //Timestamp
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dOutBaseTime;
                                frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                                FrameQueue_Add(&fqIn, frmIn);
                            }
                            else
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;
                        }
                        Rotate_Buffer_borders(frmBuffer, uiDispatch);
                        uiCur -= min(uiDispatch,uiCur);
                        dBaseTimeSw = dOutBaseTime;
                    }
                    else
                    {
                        for(mfxU32 i = 0; i < min(uiDispatch,uiCur + 1); i++)
                        {
                            FillBaseLinesIYUV(frmBuffer[0], frmBuffer[BUFMINSIZE], FALSE, FALSE);
                            if(!frmBuffer[0]->frmProperties.drop)
                            {
                                if(!bisInterlaced && !(mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1)))
                                {
                                    if(frmBuffer[0]->frmProperties.interlaced)
                                    {
                                        //if(!frmBuffer[0]->frmProperties.processed)
                                        //    DeinterlaceBilinearFilter(frmBuffer, 0 , 0);
                                        //else
                                            DeinterlaceMedianFilter(frmBuffer, 0, 0);
                                        //Second field
                                        DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);
                                        Prepare_frame_for_queue(&frmIn,frmBuffer[0], uiWidth, uiHeight);
                                    }
                                    else
                                        Prepare_frame_for_queue(&frmIn,frmBuffer[0], uiWidth, uiHeight);
                                    memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[0]->plaY.ucStats.ucRs,sizeof(double) * 10);

                                    //Timestamp
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dBaseTimeSw;
                                    frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                    Rotate_Buffer(frmBuffer);
                                }
                                else
                                {
                                    //if(!frmBuffer[0]->frmProperties.processed)
                                    //    DeinterlaceBilinearFilter(frmBuffer, 0 , 0);
                                    //else
                                        DeinterlaceMedianFilter(frmBuffer, 0, 0);
                                    //Second field
                                    DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);

                                    Prepare_frame_for_queue(&frmIn,frmBuffer[0], uiWidth, uiHeight); // Go to input frame rate
                                    memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[0]->plaY.ucStats.ucRs,sizeof(double) * 10);

                                    //Timestamp
                                    frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dDeIntTime;
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                                    frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                    if (bFullFrameRate && bisInterlaced)
                                    {
                                        Prepare_frame_for_queue(&frmIn,frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
                                        memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs,sizeof(double) * 10);

                                        //Timestamp
                                        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                                        FrameQueue_Add(&fqIn, frmIn);
                                    }
                                    Rotate_Buffer_deinterlaced(frmBuffer);
                                }
                                dBaseTimeSw = dBaseTime;
                            }
                            uiCur--;
                        }
                    }

                }
                uiCur++;
            }
            else
            {
                //QueryPerformanceCounter(&liTime[uiTimer++]);
                //QueryPerformanceCounter(&liTime[uiTimer++]);
            }
        }

    if(surf_out)
    {
        frmIn = NULL;
        //QueryPerformanceCounter(&liTime[uiTimer++]);
        if(fqIn.iCount >= 1 && !mainPattern.ucPatternFound)
        {
            uiFrameCount = fqIn.iCount;
            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                //----------------------------------------
                // Saving Test Output
                //----------------------------------------
                if (frmIn)
                {
                    ferror = FALSE;
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                    //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    memcpy(surf_out->Data.Y, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize);
                    Frame_Release(frmIn);
                    free(frmIn);

                    if(0 != fqIn.iCount)
                        return MFX_ERR_MORE_SURFACE;
                    else
                        return MFX_ERR_NONE;

                    uiFrameOut++;
                }
            }
        }
        else if(mainPattern.ucPatternFound)
        {
            mainPattern.ucPatternFound = FALSE;
            uiFrameCount = fqIn.iCount;

            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                ferror = FALSE;
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                    //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    memcpy(surf_out->Data.Y, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize);
                    Frame_Release(frmIn);
                    free(frmIn);

                    if(0 != fqIn.iCount)
                        return MFX_ERR_MORE_SURFACE;
                    else
                        return MFX_ERR_NONE;

                    uiFrameOut++;
                }
            }
        }
        //QueryPerformanceCounter(&liTime[uiTimer++]);

        //// Update Time Counters
        //for (i = 0; i < uiTimer - 1; ++i)
        //    dTime[i] += (double)(liTime[i+1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;
    //}

        if(uiCur && !surf_in)
        {
            for(mfxU32 i = 0; i < uiCur; i++)
            {
                if (!frmBuffer[i]->frmProperties.drop /*&& (frmBuffer[i]->frmProperties.tindex > uiLastFrameNumber)*/)
                {
                    if (bisInterlaced)
                    {
                        FillBaseLinesIYUV(frmBuffer[i], frmBuffer[BUFMINSIZE], FALSE, FALSE);
                        DeinterlaceMedianFilter(frmBuffer, i, 0);
                        //Second field
                        DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);

                        Prepare_frame_for_queue(&frmIn, frmBuffer[i], uiWidth, uiHeight); // Go to input frame rate
                        memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //Timestamp
                        frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[i]->frmProperties.tindex;
                        frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dDeIntTime;
                        frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                        frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                        if (bFullFrameRate)
                        {
                            Prepare_frame_for_queue(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
                            memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

                            //Timestamp
                            frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                            FrameQueue_Add(&fqIn, frmIn);
                        }
                    }
                    else
                    {
                        CheckGenFrame(frmBuffer, i, mainPattern.ucPatternType);
                        Prepare_frame_for_queue(&frmIn, frmBuffer[i], uiWidth, uiHeight);
                        memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //timestamp
                        frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + (dBaseTime * 5 / 4);
                        frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                    }
                }
            }

            uiCur = 0;

            uiFrameCount = fqIn.iCount;
            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                ferror = FALSE;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                        //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                        //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                        memcpy(surf_out->Data.Y, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize);
                        Frame_Release(frmIn);
                        free(frmIn);

                        if(0 != fqIn.iCount)
                            return MFX_ERR_MORE_SURFACE;
                        else
                            return MFX_ERR_NONE;

                    Frame_Release(frmIn);
                    free(frmIn);

                    uiFrameOut++;
                }
            }
        }
    }
    return MFX_ERR_MORE_DATA;
}

mfxStatus MFX_PTIR_Plugin::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;

    if(NULL != surface_in && NULL != surface_out)
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.CropW;
        mfxU16& h = work420_surface.Info.CropH;
        work420_surface.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        surf_out.Info.FourCC = MFX_FOURCC_I420;

        surf_out.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;


        mfxSts = PTIR_ProcessFrame( &work420_surface, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);

        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_MORE_SURFACE == mfxSts)
            bMoreOutFrames = true;
        else
            bMoreOutFrames = false;

        return mfxSts;
    }
    else if(NULL != surface_in && NULL == surface_out)
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.CropW;
        mfxU16& h = work420_surface.Info.CropH;
        work420_surface.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;

        mfxSts = PTIR_ProcessFrame( &work420_surface, 0 );

        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }

        return mfxSts;
    }
    else if(0 == fqIn.iCount && 0 == uiCur)
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_out->Info), sizeof(mfxFrameInfo));
        mfxU16& w = surf_out.Info.CropW;
        mfxU16& h = surf_out.Info.CropH;
        surf_out.Info.FourCC = MFX_FOURCC_I420;

        surf_out.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_out->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = true;
        }

        mfxSts = PTIR_ProcessFrame( 0, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);
        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_MORE_SURFACE == mfxSts)
            bMoreOutFrames = true;
        else
            bMoreOutFrames = false;
        return mfxSts;
    }

    return mfxSts;
}