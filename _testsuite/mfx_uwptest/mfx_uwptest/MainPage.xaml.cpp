﻿//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace mfx_uwptest;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

wchar_t libNames[5][32] =
#ifndef _WIN64
{
    L"libmfxhw32.dll",
    L"mfxplugin32_hw.dll",
    L"intel_gfx_api-x86.dll",
    L"igfx11cmrt32.dll",
    L"mfxstub32.dll"
};
#else
{
    L"libmfxhw64.dll",
    L"mfxplugin64_hw.dll",
    L"intel_gfx_api-x64.dll",
    L"igfx11cmrt64.dll",
    L"mfxstub64.dll"
};
#endif

void VerifyMediaSDKDecoder(progress_reporter<String^> reporter, bool hevc = false);
void DoMediaSDKDecode(progress_reporter<String^> reporter, IRandomAccessStream^ outputStream, Array<unsigned char>^ inputStream, bool hevc = false);

//Contains command line arguments
extern Platform::String^ RunArgs;
//Flags from command line
bool DumpToFile = false,
     CloseImm = false,
     UseHEVC = false;
String^ DestinationFolder;

MainPage::MainPage()
{
	InitializeComponent();
}

void mfx_uwptest::MainPage::LogMsg(Object ^ message)
{
    Dispatcher->RunAsync(
        Windows::UI::Core::CoreDispatcherPriority::High,
        ref new Windows::UI::Core::DispatchedHandler([&, message]()
    {
        String^ line = message->ToString();
        Log->Text += message->ToString() + L"\r\n";
        /* Trying to resize TextBlock to the actual size of content */
        Log->Height = Log->ActualHeight + 200;
    })
    );
}

void mfx_uwptest::MainPage::LogMsg(wchar_t *message)
{
    LogMsg(ref new String(message));
}

bool mfx_uwptest::MainPage::MfxProbeLib(Platform::String ^ name)
{
    HMODULE lib = NULL;
    DWORD errCode = S_OK;
    wchar_t strErrCode[64] = {};

    LogMsg(L"--------------------------------------------------------------");
    LogMsg(L"Probing library " + name);
    try
    {
        lib = LoadPackagedLibrary(name->Begin(), 0);
    }
    catch (...)
    {
        LogMsg(L"Exception while LoadPackagedLibrary called");
        lib = NULL;
    }
    if (lib)
    {
        LogMsg("LoadPackagedLibrary: OK");
    }
    else
    {
        LogMsg("LoadPackagedLibrary: FAIL");
        errCode = GetLastError();
        swprintf_s(strErrCode, L"GetLastError(): 0x%X (%d)", errCode, errCode);
        LogMsg(ref new String(strErrCode));
        return false;
    }
    try
    {
        LogMsg(FreeLibrary(lib) ? L"FreeLibrary: OK" : L"FreeLibrary: FAIL");
    }
    catch (...)
    {
        LogMsg(L"Exception while FreeLibrary called");
    }
    LogMsg(L"==============================================================");
    return true;
}

#include "mfxdefs.h"
#include "mfxsession.h"

void mfx_uwptest::MainPage::MfxMediaSDKProbe(void)
{
    size_t strSize = 256;
    wchar_t *str = new wchar_t[strSize];
    mfxVersion msdkVersion = {};
    mfxSession msdkSession = NULL;
    mfxStatus sts = MFX_ERR_NONE;

    MSDKVersion->Text = L"";
    LogMsg(L"--------------------------------------------------------------");
    LogMsg(L"MediaSDK probe");

    swprintf_s(str, strSize, L"Compiled MediaSDK version: %d.%d\r\n", MFX_VERSION_MAJOR, MFX_VERSION_MINOR);
    MSDKVersion->Text += ref new String(str);
    LogMsg(str);

    msdkVersion.Major = 1;
    msdkVersion.Minor = 0;
    swprintf_s(str, strSize, L"Queried MediaSDK version: %d.%d\r\n", msdkVersion.Major, msdkVersion.Minor);
    MSDKVersion->Text += ref new String(str);
    LogMsg(str);
    try
    {
        sts = MFXInit(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11, &msdkVersion, &msdkSession);
    }
    catch (...)
    {
        LogMsg("Exception while MFXInit called");
        sts = MFX_ERR_UNKNOWN;
    }
    if (str == NULL)
    {
        LogMsg("Possible heap corruption while MFXInit called");
        goto cleanup;
    }
    if (sts == MFX_ERR_NONE)
    {
        swprintf_s(str, strSize, L"MFXInit(): OK");
    }
    else
    {
        swprintf_s(str, strSize, L"MFXInit(): FAIL\r\nSTATUS: 0x%X (%d)", static_cast<int>(sts), static_cast<int>(sts));
    }
    LogMsg(str);

    if (sts == MFX_ERR_NONE)
    {
        try
        {
            sts = MFXQueryVersion(msdkSession, &msdkVersion);
        }
        catch (...)
        {
            LogMsg("Exception while MFXQueryVersion called");
            sts = MFX_ERR_UNKNOWN;
        }

        swprintf_s(str, strSize, L"Loaded MediaSDK version: %d.%d\r\n", msdkVersion.Major, msdkVersion.Minor);
        MSDKVersion->Text += ref new String(str);
        LogMsg(str);

        try
        {
            MFXClose(msdkSession);
        }
        catch (...)
        {
            LogMsg("Exception while MFXClose called");
        }
    }
    else
    {
        MSDKVersion->Text += L"MSDK init failed\r\n";
        LogMsg(L"MSDK init failed\r\n");
    }

cleanup:
    delete[] str;
    LogMsg(L"==============================================================");
}

void mfx_uwptest::MainPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Application::Current->Exit();
}

void mfx_uwptest::MainPage::Grid_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Array<String^>^ strNames = ref new Array<String^>(sizeof(libNames) / sizeof(libNames[0]));
    Log->Text = L"";
    LogMsg(L"Available command line usage");
    LogMsg(L"mfx_uwptest [/dump] [/hevcd] [/close] [/output Destination\\Path]");
    LogMsg(L"/dump - dump log to Documents\\mfx_uwptest.mfxresult");
    LogMsg(L"/close - close immediately after work is completed");
    LogMsg(L"/hevcd - run HEVC decode instead of AVC (by default)");
    LogMsg(L"/output - put results to the destination folder (Documents by default)");
    LogMsg(L"==============================================================");

    for (int i = 0; i < sizeof(libNames) / sizeof(libNames[0]); ++i)
    {
        strNames[i] = ref new String(libNames[i]);
    }

    for (int i = 0; i < sizeof(libNames) / sizeof(libNames[0]); ++i)
    {
        bool probe = MfxProbeLib(strNames[i]);
        switch (i)
        {
        case 0: //libmfxhw*.dll
            CbLibmfx->IsChecked = probe;
            break;
        case 1: //mfxplugin_hw*.dll
            CbMfxPlugin->IsChecked = probe;
            break;
        case 2: //intel_gfx_api*.dll
            CbIntelApi->IsChecked = probe;
            break;
        case 3: //igfx11cmrt*.dll
            CbMDF->IsChecked = probe;
            break;
        case 4: //mfxstub*.dll
            CbStub->IsChecked = probe;
            break;
        }
    }

    if (CbLibmfx->IsChecked)
    {
        try
        {
            MfxMediaSDKProbe();
        }
        catch (...)
        {
            LogMsg("Something wrong happened while MediaSDK probe...");
        }
    }

#pragma warning(disable:4996)
    wchar_t delim[] = L" ";
    wchar_t *pwc = NULL;
    //Debug
    //RunArgs = L"/dump /output d:\\results";
    if (!RunArgs->IsEmpty())
    {
        pwc = wcstok((wchar_t*)RunArgs->Begin(), &delim[0], 0);
    }

    while (pwc != NULL)
    {
        String^ part = ref new String(pwc);
        LogMsg((ref new String(L"Command line argument found: \"")) + part + L"\"");
        if(part == L"/dump")
            DumpToFile = true;
        if(part == L"/close")
            CloseImm = true;
        if(part == L"/hevcd")
            UseHEVC = true;
        if (part == L"/output")
        {
            pwc = wcstok(NULL, &delim[0], 0);
            if (pwc != NULL)
            {
                DestinationFolder = ref new String(pwc);
            }
        }
        pwc = wcstok(NULL, &delim[0], 0);
    }

    if (CloseImm)
    {
        LogMsg(L"Immediately close choosed");
    }


    try
    {
        MfxCheckRobustness(UseHEVC);
    }
    catch (...)
    {
        LogMsg("Something wrong happened while MediaSDK checking robustness...");
    }
}


#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"

#define EX_HANDLER(expr) \
try \
{ \
    expr \
} \
catch(const std::exception &e) \
{ \
    size_t exStrSize = 256; \
    wchar_t *exStr = new wchar_t[exStrSize]; \
    swprintf_s(exStr, exStrSize, L"%s, line %d thrown std::exception %S", L#expr, __LINE__, e.what()); \
    reporter.report(ref new String(exStr)); \
} \
catch(...) \
{ \
    size_t exStrSize = 256; \
    wchar_t *exStr = new wchar_t[exStrSize]; \
    swprintf_s(exStr, exStrSize, L"%s thrown unknown exception", L#expr); \
    reporter.report(ref new String(exStr)); \
} \

#define MFX_CHECK(sts) \
if(sts != MFX_ERR_NONE) \
{ \
    reporter.report("DECODE: ERROR"); \
    return; \
} \


void VerifyMediaSDKDecoder(progress_reporter<String^> reporter, bool hevc)
{
    String^ sourcePath;
    String^ inFileName = nullptr;
    String^ outFileName = nullptr;

    if (hevc)
    {
        inFileName = L"input.h265";
        outFileName = L"mfx_h265_decode_nv12.mfxresult";
        reporter.report(L"Decoding HEVC");
    }
    else
    {
        inFileName = L"input.h264";
        outFileName = L"mfx_h264_decode_nv12.mfxresult";
        reporter.report(L"Decoding AVC");
    }

    reporter.report(ref new String(L"Output folder: ") + (DestinationFolder->IsEmpty() ? L"Documents" : DestinationFolder));

    sourcePath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path + L"\\Assets";
    reporter.report(ref new String(L"Application folder: ") + sourcePath);

    IRandomAccessStream^ outputStream;
    StorageFolder^ outputDir;
    Array<unsigned char>^ bytes;

    /* Opening output file */
    try
    {
        outputStream = create_task(DestinationFolder->IsEmpty() ? KnownFolders::GetFolderForUserAsync(nullptr, KnownFolderId::DocumentsLibrary) : StorageFolder::GetFolderFromPathAsync(DestinationFolder))
            .then([outFileName](StorageFolder^ folder)
        {
            return create_task(folder->CreateFileAsync(outFileName, CreationCollisionOption::ReplaceExisting));
        })
            .then([](StorageFile^ file)
        {
            return create_task(file->OpenAsync(FileAccessMode::ReadWrite));
        })
            .then([](IRandomAccessStream^ stream)
        {
            return stream;
        })
            .get();
    }
    catch (...)
    {
        reporter.report("Cannot open output file. Check file for existence. Probably application doesn't have FileSystem access rights.");
        return;
    }

    try
    {
        bytes = create_task(StorageFolder::GetFolderFromPathAsync(sourcePath))
            .then([inFileName](StorageFolder^ folder)
        {
            return create_task(folder->GetFileAsync(inFileName));
        })
            .then([](StorageFile^ file)
        {
            return create_task(file->OpenAsync(FileAccessMode::Read));
        })
            .then([reporter](IRandomAccessStream^ stream)
        {
            DataReader^ dataReader = ref new DataReader(stream);
            unsigned int sSize = static_cast<unsigned int>(stream->Size);

            return create_task(dataReader->LoadAsync(sSize))
                .then([dataReader, reporter](unsigned int bytesLoaded)
            {
                Array<unsigned char>^ bytes = nullptr;
                try
                {
                    bytes = ref new Array<unsigned char>(bytesLoaded);
                }
                catch (...)
                {
                    reporter.report(L"Cannot allocate memory for input file.");
                    return bytes;
                }
                dataReader->ReadBytes(bytes);
                return bytes;
            });
        }).get();
    }
    catch (...)
    {
        reporter.report("Cannot open input file.");
        return;
    }

    DoMediaSDKDecode(reporter, outputStream, bytes, hevc);
}

void DoMediaSDKDecode(progress_reporter<String^> reporter, IRandomAccessStream^ outputStream, Array<unsigned char>^ inputStream, bool hevc)
{
    /* Global definitions */
    mfxStatus sts = MFX_ERR_NONE;
    mfxBitstream inputBS = { 0 }, outputBS = { 0 };
    mfxVideoParam vParams = { 0 };
    mfxFrameAllocRequest allocRequest = { 0 };
    std::vector<mfxFrameSurface1> frames(0);
    mfxFrameSurface1
        *displayOrderFrame = NULL,
        *freeFrame = NULL;
    std::vector<std::shared_ptr<mfxU8>> framesMemory(0);
    mfxSyncPoint syncPoint = NULL;

    /* MediaSDK Initialization */
    mfxVersion minVersion = { 0, 1 };
    mfxSession session = { 0 };

    /* For making messages */
    size_t strSize = 256;
    std::shared_ptr<wchar_t> strKeeper(new wchar_t[strSize], std::default_delete<wchar_t[]>());
    wchar_t *str = &(*strKeeper);

    /* For writing an output bitstream */
    DataWriter^ dataWriter = ref new DataWriter(outputStream);
    std::vector<mfxU8> streamData(0);

    /* MediaSDK initialization */
    EX_HANDLER(sts = MFXInit(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11, &minVersion, &session););
    swprintf_s(str, strSize, L"MFXInit(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11) returned status: %X", sts);
    reporter.report(ref new String(str));
    MFX_CHECK(sts);

    if (hevc)
    {
        EX_HANDLER(sts = MFXVideoUSER_Load(session, &MFX_PLUGINID_HEVCD_HW, 0););
        swprintf_s(str, strSize, L"MFXVideoUSER_Load(MFX_PLUGINID_HEVCD_HW) returned status: %X", sts);
        reporter.report(ref new String(str));
        if (sts != MFX_ERR_NONE)
        {
            reporter.report(ref new String(L"Possible HEVC is placed in libmfxhw, trying to use it without plugin loading"));
        }
    }
    if (hevc)
    {
        vParams.mfx.CodecId = MFX_CODEC_HEVC;
    }
    else
    {
        vParams.mfx.CodecId = MFX_CODEC_AVC;
    }

    /* Read input file */
    inputBS.MaxLength = inputBS.DataLength = 0;
    try
    {
        streamData.resize(inputStream->Length);
        inputBS.Data = streamData.data();
    }
    catch (...)
    {
        reporter.report(L"Couldn't allocate memory for input mfxBitstream->Data");
        MFX_CHECK(MFX_ERR_MEMORY_ALLOC);
    }
    memcpy(inputBS.Data, inputStream->begin(), inputStream->Length);
    inputBS.DataLength = inputBS.MaxLength = inputStream->Length;

    /* Decoding header of bitstream */
    EX_HANDLER(sts = MFXVideoDECODE_DecodeHeader(session, &inputBS, &vParams););
    swprintf_s(str, strSize, L"MFXVideoDECODE_DecodeHeader returned status: %X", sts);
    reporter.report(ref new String(str));
    MFX_CHECK(sts);

    /* Important! We need set IOPattern before working with functions below */
    if (hevc)
    {
        vParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    else
    {
        vParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    /* Decoder initialization */
    EX_HANDLER(sts = MFXVideoDECODE_QueryIOSurf(session, &vParams, &allocRequest););
    swprintf_s(str, strSize, L"MFXVideoDECODE_QueryIOSurf returned status: %X", sts);
    reporter.report(ref new String(str));
    MFX_CHECK(sts);

    EX_HANDLER(sts = MFXVideoDECODE_Init(session, &vParams););
    swprintf_s(str, strSize, L"MFXVideoDECODE_Init returned status: %X", sts);
    reporter.report(ref new String(str));
    MFX_CHECK(sts);

    /* Allocating output frames */
    try
    {
        frames.resize(allocRequest.NumFrameSuggested);
    }
    catch (...)
    {
        reporter.report(L"Cannot allocate memory for mfxFrameSurface1 array.");
        MFX_CHECK(MFX_ERR_MEMORY_ALLOC);
    }

    memset(frames.data(), 0, sizeof(mfxFrameSurface1) * allocRequest.NumFrameSuggested);

    try
    {
        framesMemory.resize(allocRequest.NumFrameSuggested);
        for (mfxU16 i = 0; i < allocRequest.NumFrameSuggested; i++) {
            frames[i].Info = allocRequest.Info;
            frames[i].Data.Locked = false;

            /* Count size of aligned row */
            frames[i].Data.Pitch = (allocRequest.Info.Width + 0xF) & (~0xF);

            /* We are allocating memory for following alignment */
            framesMemory[i].reset(new mfxU8[frames[i].Data.Pitch * frames[i].Info.Height * 3 + 0x10]);
            mfxU8 *memPtr = &(*framesMemory[i]);

            /* Align start address to 16 */
            frames[i].Data.Y = reinterpret_cast<mfxU8*>((reinterpret_cast<ptrdiff_t>(memPtr) + 0xF) & (~0xF));
            frames[i].Data.UV = frames[i].Data.Y + frames[i].Data.Pitch * allocRequest.Info.Height;
        }
    }
    catch (...)
    {
        reporter.report(L"Couldn't allocate memory for frames.\nDECODE: ERROR");
        return;
    }

    reporter.report(L"Starting decode process, please, wait...");

    mfxU32 decodedFrameCount = 0, passedFrameCount = 0;
    bool decodingError = false;
    LARGE_INTEGER perfStart, perfFreq, perfEnd, perfElapsed, diskStart, diskEnd, diskElapsed;

    diskElapsed.QuadPart = 0;
    QueryPerformanceFrequency(&perfFreq);
    perfFreq.QuadPart /= 1000;
    QueryPerformanceCounter(&perfStart);

    //
    // Stage 1: Main decoding loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            _sleep(10);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
            freeFrame = NULL;
            for (mfxU16 i = 0; i < allocRequest.NumFrameSuggested; i++) {
                if (!frames[i].Data.Locked) {
                    freeFrame = &frames[i];
                    break;
                }
            }
            if (!freeFrame) {
                _sleep(10);
                continue;
            }
        }

        //break if no data on input
        if (inputBS.DataLength == 0) break;

        // Decode a frame asychronously (returns immediately)
        //  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
        EX_HANDLER(sts = MFXVideoDECODE_DecodeFrameAsync(session, &inputBS, freeFrame, &displayOrderFrame, &syncPoint););
        if (sts != MFX_ERR_NONE)
        {
            swprintf_s(str, strSize, L"MFXVideoDECODE_DecodeFrameAsync returned status: %X", sts);
            reporter.report(ref new String(str));
        }

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncPoint)
            sts = MFX_ERR_NONE;

        if (sts == MFX_ERR_NONE) {
            EX_HANDLER(sts = MFXVideoCORE_SyncOperation(session, syncPoint, MFX_INFINITE););
            decodedFrameCount++;
            if (sts != MFX_ERR_NONE)
            {
                swprintf_s(str, strSize, L"Frame# %d MFXVideoCORE_SyncOperation returned status: %X", decodedFrameCount, sts);
                reporter.report(ref new String(str));
                if (sts < MFX_ERR_NONE)
                {
                    decodingError = true;
                    break;
                }
            }
            QueryPerformanceCounter(&diskStart);

            Array<unsigned char>^ outbytes = ref new Array<unsigned char>(displayOrderFrame->Info.CropW);

            /* We are writing Y-component */
            for (mfxU32 row = 0; row < displayOrderFrame->Info.CropH; row++)
            {
                memcpy(outbytes->begin(), displayOrderFrame->Data.Y + row * displayOrderFrame->Data.Pitch, displayOrderFrame->Info.CropW);
                dataWriter->WriteBytes(outbytes);
            }
            /* We are writing UV-component */
            for (mfxU32 row = 0; row < mfxU32(displayOrderFrame->Info.CropH / 2); row++)
            {
                memcpy(outbytes->begin(), displayOrderFrame->Data.U + row * displayOrderFrame->Data.Pitch, displayOrderFrame->Info.CropW);
                dataWriter->WriteBytes(outbytes);
            }
            /* To do wait() you need start task with task_continuation_context::use_arbitrary(), look after this lambda's body */
            create_task(dataWriter->StoreAsync()).wait();
            create_task(outputStream->FlushAsync()).wait();
            QueryPerformanceCounter(&diskEnd);

            diskElapsed.QuadPart += diskEnd.QuadPart - diskStart.QuadPart;
        }
    }

    // MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
    if (sts == MFX_ERR_MORE_DATA)
    {
        sts = MFX_ERR_NONE;
    }
    else if (sts < MFX_ERR_NONE)
    {
        decodingError = true;
    }

    //
    // Stage 2: Retrieve the buffered decoded frames
    //
    while (!decodingError && (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts)) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            _sleep(10);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        freeFrame = NULL;
        for (mfxU16 i = 0; i < allocRequest.NumFrameSuggested; i++) {
            if (!frames[i].Data.Locked) {
                freeFrame = &frames[i];
                break;
            }
        }
        if (!freeFrame) {
            _sleep(10);
            continue;
        }

        // Decode a frame asychronously (returns immediately)
        EX_HANDLER(sts = MFXVideoDECODE_DecodeFrameAsync(session, NULL, freeFrame, &displayOrderFrame, &syncPoint););
        if (sts != MFX_ERR_NONE)
        {
            swprintf_s(str, strSize, L"MFXVideoDECODE_DecodeFrameAsync returned status: %X", sts);
            reporter.report(ref new String(str));
        }

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncPoint)
            sts = MFX_ERR_NONE;

        if (sts == MFX_ERR_NONE) {
            EX_HANDLER(sts = MFXVideoCORE_SyncOperation(session, syncPoint, MFX_INFINITE););
            decodedFrameCount++;
            if (sts != MFX_ERR_NONE)
            {
                swprintf_s(str, strSize, L"Frame# %d MFXVideoCORE_SyncOperation returned status: %X", decodedFrameCount, sts);
                reporter.report(ref new String(str));
                if (sts < MFX_ERR_NONE)
                {
                    decodingError = true;
                    break;
                }
            }
            QueryPerformanceCounter(&diskStart);

            Array<unsigned char>^ outbytes = ref new Array<unsigned char>(displayOrderFrame->Info.CropW);

            /* We are writing Y-component */
            for (mfxU32 row = 0; row < displayOrderFrame->Info.CropH; row++)
            {
                memcpy(outbytes->begin(), displayOrderFrame->Data.Y + row * displayOrderFrame->Data.Pitch, displayOrderFrame->Info.CropW);
                dataWriter->WriteBytes(outbytes);
            }
            /* We are writing UV-component */
            for (mfxU32 row = 0; row < mfxU32(displayOrderFrame->Info.CropH / 2); row++)
            {
                memcpy(outbytes->begin(), displayOrderFrame->Data.U + row * displayOrderFrame->Data.Pitch, displayOrderFrame->Info.CropW);
                dataWriter->WriteBytes(outbytes);
            }
            /* To do wait() you need start task with task_continuation_context::use_arbitrary(), look after this lambda's body */
            create_task(dataWriter->StoreAsync()).wait();
            create_task(outputStream->FlushAsync()).wait();
            QueryPerformanceCounter(&diskEnd);

            diskElapsed.QuadPart += diskEnd.QuadPart - diskStart.QuadPart;
        }
    }

    QueryPerformanceCounter(&perfEnd);
    perfElapsed.QuadPart = (perfEnd.QuadPart - perfStart.QuadPart - diskElapsed.QuadPart) / perfFreq.QuadPart;
    diskElapsed.QuadPart /= perfFreq.QuadPart;
    swprintf_s(str, strSize, L"Total frames decoded: %d\r\nDECODE FPS: %.2f fps\r\nDISK FPS: %.2f fps",
        decodedFrameCount,
        (double)decodedFrameCount / ((double)perfElapsed.QuadPart / 1000),
        (double)decodedFrameCount / ((double)diskElapsed.QuadPart / 1000)
    );
    reporter.report(ref new String(str));

    /* MediaSDK De-Initialization */
    if (session != NULL)
    {
        EX_HANDLER(sts = MFXVideoDECODE_Close(session););
        swprintf_s(str, strSize, L"MFXVideoDECODE_Close returned status: %X", sts);
        reporter.report(ref new String(str));
        EX_HANDLER(sts = MFXClose(session););
        swprintf_s(str, strSize, L"MFXClose returned status: %X", sts);
        reporter.report(ref new String(str));
        if (!decodingError)
        {
            reporter.report(L"DECODE: SUCCESS");
            return;
        }
    }
}

bool mfx_uwptest::MainPage::MfxCheckRobustness(bool hevc)
{
    auto decodeTask = create_async([hevc](progress_reporter<String^> reporter) {
        VerifyMediaSDKDecoder(reporter, hevc);
    });
    decodeTask->Progress = ref new AsyncActionProgressHandler<String^>([=](IAsyncActionWithProgress<String^>^ asyncInfo, String^ progress)
    {
        Log->Text += progress + "\r\n";
        Log->Height = Log->ActualHeight + 200;
    });
    /* This needs to assign task to Non-UI thread */
    concurrency::task_options decodeTaskOptions = {};
    decodeTaskOptions.set_continuation_context(concurrency::task_continuation_context::use_arbitrary());


    /* Decoding procedure */
    create_task(decodeTask, decodeTaskOptions)
        .then([this]()
    {
        Log->Text += ref new String(L"Decoding task is completed\r\n");
    }, task_continuation_context::use_default())
    /* After all - do command line requests */
        .then([=]() -> String^
    {
        if (DumpToFile)
        {
            this->LogMsg(L"Log dump is required");
        }
        else
        {
            this->LogMsg(L"Log dump is NOT required");
        }
        Sleep(1000); //Looking for correct mechanism for the waiting for queued tasks
        Log->Height = Log->ActualHeight + 100;
        return this->Log->Text;
    }, task_continuation_context::use_default())
        .then([=](String^ LogText)
    {
        if (DumpToFile)
        {
            try
            {
                create_task(DestinationFolder->IsEmpty() ? KnownFolders::GetFolderForUserAsync(nullptr, KnownFolderId::DocumentsLibrary) : StorageFolder::GetFolderFromPathAsync(DestinationFolder))
                    .then([](StorageFolder^ folder)
                {
                    return create_task(folder->CreateFileAsync(L"mfx_uwptest.mfxresult", CreationCollisionOption::ReplaceExisting));
                })
                    .then([](StorageFile^ file)
                {
                    return create_task(file->OpenAsync(FileAccessMode::ReadWrite));
                })
                    .then([LogText](IRandomAccessStream^ stream)
                {
                    InMemoryRandomAccessStream^ memStream = ref new InMemoryRandomAccessStream();
                    DataWriter^ dataWriter = ref new DataWriter(memStream);
                    dataWriter->WriteString(LogText);
                    create_task(stream->WriteAsync(dataWriter->DetachBuffer())).wait();
                })
                    .wait();
            }
            catch (...)
            {
                LogMsg(L"Cannot open output log file. Probably application doesn't have FileSystem access rights.");
                try
                {
                    Sleep(1000); //Looking for correct mechanism for the waiting for queued tasks
                    create_task(Windows::Storage::ApplicationData::Current->LocalFolder->CreateFileAsync(L"runlog.txt", CreationCollisionOption::ReplaceExisting))
                        .then([](StorageFile^ file)
                    {
                        return create_task(file->OpenAsync(FileAccessMode::ReadWrite));
                    })
                        .then([LogText](IRandomAccessStream^ stream)
                    {
                        InMemoryRandomAccessStream^ memStream = ref new InMemoryRandomAccessStream();
                        DataWriter^ dataWriter = ref new DataWriter(memStream);
                        dataWriter->WriteString(LogText + L"Log file is unavailable. It is possible due to FileSystem access restrictions.");
                        create_task(stream->WriteAsync(dataWriter->DetachBuffer())).wait();
                    })
                        .wait();
                }
                catch (...)
                {
                }
            }
        }
    }, task_continuation_context::use_arbitrary())
        .then([]()
    {
        if (CloseImm)
        {
            Application::Current->Exit();
        }
    });


    return 0;
}