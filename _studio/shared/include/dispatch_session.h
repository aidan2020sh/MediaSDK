/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: dispatch_session.h

\* ****************************************************************************** */
#ifndef __DISPATCH_SESSION_H__
#define __DISPATCH_SESSION_H__

struct JumpTable;

struct DispatchSession {
    JumpTable *jump_table;
};

#endif
