/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if !defined(_WIN32) && !defined(_WIN64)

#define _GNU_SOURCE /* may need on some OS to support PTHREAD_MUTEX_RECURSIVE */
#include "mfx_vm++_pthread.h"

struct _MfxMutexHandle
{
    pthread_mutex_t m_mutex;
};

MfxMutex::MfxMutex(void)
{
    int res = 0;
    pthread_mutexattr_t mutex_attr;

    res = pthread_mutexattr_init(&mutex_attr);
    if (!res)
    {
        res = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
        if (!res) res = pthread_mutex_init(&m_handle.m_mutex, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
    }
    if (res) throw std::bad_alloc();
}

MfxMutex::~MfxMutex(void)
{
    pthread_mutex_destroy(&m_handle.m_mutex);
}

mfxStatus MfxMutex::Lock(void)
{
    return (pthread_mutex_lock(&m_handle.m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MfxMutex::Unlock(void)
{
    return (pthread_mutex_unlock(&m_handle.m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

bool MfxMutex::TryLock(void)
{
    return (pthread_mutex_trylock(&m_handle.m_mutex))? false: true;
}

#endif // #if !defined(_WIN32) && !defined(_WIN64)
