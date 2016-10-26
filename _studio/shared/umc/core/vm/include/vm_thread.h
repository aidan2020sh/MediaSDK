//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_THREAD_H__
#define __VM_THREAD_H__

#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* IMPORTANT:
 *    The thread return value is always saved for the calling thread to
 *    collect. To avoid memory leak, always vm_thread_wait() for the
 *    child thread to terminate in the calling thread.
 */

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#define VM_THREAD_CALLCONVENTION __stdcall
#else
#define VM_THREAD_CALLCONVENTION
#endif

typedef Ipp32u (VM_THREAD_CALLCONVENTION * vm_thread_callback)(void *);

typedef enum
{
    VM_THREAD_PRIORITY_HIGHEST,
    VM_THREAD_PRIORITY_HIGH,
    VM_THREAD_PRIORITY_NORMAL,
    VM_THREAD_PRIORITY_LOW,
    VM_THREAD_PRIORITY_LOWEST
} vm_thread_priority;

/* Set the thread handler to an invalid value */
void vm_thread_set_invalid(vm_thread *thread);

/* Verify if the thread handler is valid */
Ipp32s vm_thread_is_valid(vm_thread *thread);

/* Create a thread. The thread is set to run at the call.
   Return 1 if successful */
Ipp32s vm_thread_create(vm_thread *thread, vm_thread_callback func, void *arg);


/* Attach to externally created thread. Should be called from the thread in question.
Return 1 if successful */
Ipp32s vm_thread_attach(vm_thread *thread, vm_thread_callback func, void *arg);

/* Wait until a thread exists */
void vm_thread_wait(vm_thread *thread);

/* Set thread priority. Return 1 if successful */
Ipp32s vm_thread_set_priority(vm_thread *thread, vm_thread_priority priority);

/* Close thread after all */
void vm_thread_close(vm_thread *thread);

/* Get current thread priority */
vm_thread_priority vm_get_current_thread_priority(void);

/* Set current thread priority */
void vm_set_current_thread_priority(vm_thread_priority priority);

typedef struct
{
  Ipp32s schedtype;
  Ipp32s priority;
} vm_thread_linux_schedparams;

/* Set thread scheduling type and corresponding parameters. */
Ipp32s vm_thread_set_scheduling(vm_thread* thread, void* params);

/* Set thread's affinity mask */
void vm_set_thread_affinity_mask(vm_thread *thread, Ipp64u mask);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_THREAD_H__ */
