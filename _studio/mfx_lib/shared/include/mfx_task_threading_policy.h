/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_TASK_THREADING_POLICY_H
#define __MFX_TASK_THREADING_POLICY_H

// Declare task execution flag's
enum
{
    // Task can't share processing unit with other tasks
    MFX_TASK_INTRA = 1,
    // Task can share processing unit with other tasks
    MFX_TASK_INTER = 2,
    // Task can be managed by thread 0 only
    MFX_TASK_DEDICATED = 4,
    // Task share executing threads with other tasks
    MFX_TASK_SHARED = 8,
    // Task is waiting for result of another task.
    // it doesn't have its own return value.
    MFX_TASK_WAIT = 16

};

typedef
enum mfxTaskThreadingPolicy
{
    // The plugin doesn't support parallel task execution.
    // Tasks need to be processed one by one.
    MFX_TASK_THREADING_INTRA = MFX_TASK_INTRA,

    // The plugin supports parallel task execution.
    // Tasks can be processed independently.
    MFX_TASK_THREADING_INTER = MFX_TASK_INTER,

    // All task marked 'dedicated' is executed by thread #0 only.
    MFX_TASK_THREADING_DEDICATED = MFX_TASK_DEDICATED | MFX_TASK_INTRA,

    // As inter, but the plugin has limited processing resources.
    // The total number of threads is limited.
    MFX_TASK_THREADING_SHARED = MFX_TASK_SHARED,

    // Tasks of these type is 'waiting' tasks
    MFX_TASK_THREADING_DEDICATED_WAIT = MFX_TASK_WAIT | MFX_TASK_DEDICATED | MFX_TASK_INTRA,

#ifdef MFX_VA
    MFX_TASK_THREADING_DEFAULT = MFX_TASK_THREADING_DEDICATED
#else // !MFX_VA
    MFX_TASK_THREADING_DEFAULT = MFX_TASK_THREADING_INTRA
#endif // MFX_VA

} mfxTaskThreadingPolicy;

#endif // __MFX_TASK_THREADING_POLICY_H
