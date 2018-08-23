//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_semaphore.h"

namespace UMC
{

Semaphore::Semaphore(void)
{
    vm_semaphore_set_invalid(&m_handle);

} // Semaphore::Semaphore(void)

Semaphore::~Semaphore(void)
{
    if (vm_semaphore_is_valid(&m_handle))
    {
        vm_semaphore_post(&m_handle);
        vm_semaphore_destroy(&m_handle);
    }

} // Semaphore::~Semaphore(void)

Status Semaphore::Init(int32_t iInitCount)
{
    if (vm_semaphore_is_valid(&m_handle))
    {
        vm_semaphore_post(&m_handle);
        vm_semaphore_destroy(&m_handle);
    }
    return vm_semaphore_init(&m_handle, iInitCount);

} // Status Semaphore::Init(int32_t iInitCount)

Status Semaphore::Init(int32_t iInitCount, int32_t iMaxCount)
{
    if (vm_semaphore_is_valid(&m_handle))
    {
        vm_semaphore_post(&m_handle);
        vm_semaphore_destroy(&m_handle);
    }
    return vm_semaphore_init_max(&m_handle, iInitCount, iMaxCount);

} // Status Semaphore::Init(int32_t iInitCount, int32_t iMaxCount)

} // namespace UMC
