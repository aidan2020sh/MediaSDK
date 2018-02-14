﻿/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "uwp_encode_loop.h"
#include "MainPage.g.h"

namespace simple_encoder
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{

	public:
		MainPage();
    private:
        
        void OperationStatus(Platform::String^ message);

        bool m_bEncoding;
		std::shared_ptr<CEncoder> m_Encoder;

        Windows::Foundation::EventRegistrationToken regToken_for_ToggleEncoding;

        task<StorageFile^> ShowFileOpenPicker();
        task<StorageFile^> ShowFileSavePicker();

        StorageFile^ m_StorageSource;
        StorageFile^ m_StorageSink;

        
        void ToggleEncodingOn(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ToggleEncodingOff(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void OnAppSuspending(Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        void FileOpenPickerClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void FileSavePickerClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void HandleFilePickerException();

        mfxU32 CalculateBitrate(int frameSize);

        mfxVideoParam LoadParams();
        void DoEncode();

        void ScheduleToGuiThread(std::function<void()> p);
        void InvalidateControlsState(Platform::String^ state = "", double progress = -1.);
    };
}
