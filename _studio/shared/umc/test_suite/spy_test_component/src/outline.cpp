// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "outline.h"
#include "umc_h264_dec.h"

void PrintTestData(const TestVideoData * data)
{
    vm_string_printf(VM_STRING("frame - %d\n"), data->GetFrameNumber());
    vm_string_printf(VM_STRING("sequence - %d\n"), data->GetSequenceNumber());
    vm_string_printf(VM_STRING("crc - %X\n"), data->GetCRC32());
    vm_string_printf(VM_STRING("frame type - %d\n"), data->GetFrameType());
    vm_string_printf(VM_STRING("pic struct - %d\n"), data->GetPictureStructure());
    vm_string_printf(VM_STRING("timestamp - %d\n"), (Ipp32s)data->GetIntTime());
    vm_string_printf(VM_STRING("invalid flag - %d\n"), data->GetInvalid());

    if (data->m_is_mfx)
    {
        vm_string_printf(VM_STRING("viewId - %d\n"), data->m_mfx_viewId);
        vm_string_printf(VM_STRING("temporalId - %d\n"), data->m_mfx_temporalId);
        vm_string_printf(VM_STRING("priorityId - %d\n"), data->m_mfx_priorityId);
        vm_string_printf(VM_STRING("data flag - %d\n"), data->m_mfx_dataFlag);
        vm_string_printf(VM_STRING("MFX pic struct - %d\n"), data->m_mfx_picStruct);
    }
}

void PrintSequence(const UMC::VideoDecoderParams * sequence)
{
    const UMC::H264VideoDecoderParams *avc_info = DynamicCast<const UMC::H264VideoDecoderParams>(sequence);

    if (avc_info)
    {
        vm_string_printf(VM_STRING("width - %d\n"), avc_info->m_fullSize.width);
        vm_string_printf(VM_STRING("height - %d\n"), avc_info->m_fullSize.height);
        vm_string_printf(VM_STRING("crop width - %d\n"), sequence->info.clip_info.width);
        vm_string_printf(VM_STRING("crop height - %d\n"), sequence->info.clip_info.height);
        vm_string_printf(VM_STRING("crop x - %d\n"), avc_info->m_cropArea.left);
        vm_string_printf(VM_STRING("crop y - %d\n"), avc_info->m_cropArea.top);
    }
    else
    {
        vm_string_printf(VM_STRING("width - %d\n"), sequence->info.clip_info.width);
        vm_string_printf(VM_STRING("height - %d\n"), sequence->info.clip_info.height);
    }

    vm_string_printf(VM_STRING("aspect ration width - %d\n"), sequence->info.aspect_ratio_width);
    vm_string_printf(VM_STRING("aspect ration height - %d\n"), sequence->info.aspect_ratio_height);
    vm_string_printf(VM_STRING("profile - %d\n"), sequence->info.profile);
    vm_string_printf(VM_STRING("level - %d\n"), sequence->info.level);
    vm_string_printf(VM_STRING("framerate - %d\n"), (Ipp32u)(sequence->info.framerate * 100));
}

OutlineErrors::OutlineErrors()
    : m_exception(UMC::UMC_OK)
{
}

const OutlineException * OutlineErrors::GetLastError() const
{
    return &m_exception;
}

void OutlineErrors::SetLastError(OutlineException * ex)
{
    if (!ex)
    {
        m_exception = OutlineException(UMC::UMC_OK);
        return;
    }

    m_exception = *ex;
}

void OutlineErrors::PrintError()
{
    const OutlineException * ex = GetLastError();

    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("outline error - %s\n"), ex->GetDescription());

    const OutlineError & err = ex->GetError();

    switch(err.GetErrorType())
    {
    case OutlineError::OUTLINE_ERROR_TYPE_UNKNOWN:
        break;
    case OutlineError::OUTLINE_ERROR_TYPE_SEQUENCE:
        vm_string_printf(VM_STRING("\n"));
        vm_string_printf(VM_STRING("Reference sequence\n"));
        PrintSequence(err.GetReferenceSequence());
        vm_string_printf(VM_STRING("\nTest sequence\n"));
        PrintSequence(err.GetSequence());
        vm_string_printf(VM_STRING("\n"));
        break;
    case OutlineError::OUTLINE_ERROR_TYPE_FRAME:
        vm_string_printf(VM_STRING("\n"));
        vm_string_printf(VM_STRING("Reference test data\n"));
        PrintTestData(err.GetReferenceTestVideoData());
        vm_string_printf(VM_STRING("\nTest data\n"));
        PrintTestData(err.GetTestVideoData());
        vm_string_printf(VM_STRING("\n"));
        break;
    }
}
