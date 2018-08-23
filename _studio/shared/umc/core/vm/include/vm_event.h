//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_EVENT_H__
#define __VM_EVENT_H__

#include "vm_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Invalidate an event */
void vm_event_set_invalid(vm_event *event);

/* Verify if an event is valid */
int32_t vm_event_is_valid(vm_event *event);

/* Init an event. Event is created unset. return 1 if success */
vm_status vm_event_init(vm_event *event, int32_t manual, int32_t state);
vm_status vm_event_named_init(vm_event *event, int32_t manual, int32_t state, const char *pcName);

/* Set the event to either HIGH (>0) or LOW (0) state */
vm_status vm_event_signal(vm_event *event);
vm_status vm_event_reset(vm_event *event);

/* Pulse the event 0 -> 1 -> 0 */
vm_status vm_event_pulse(vm_event *event);

/* Wait for event to be high with blocking */
vm_status vm_event_wait(vm_event *event);

/* Wait for event to be high without blocking, return 1 if signaled */
vm_status vm_event_timed_wait(vm_event *event, uint32_t msec);

/* Destroy the event */
void vm_event_destroy(vm_event *event);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_EVENT_H__ */
