//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MEMORY_ALLOCATOR_H__
#define __UMC_MEMORY_ALLOCATOR_H__

#include "umc_defs.h"
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_mutex.h"

#define MID_INVALID 0

namespace UMC
{

typedef size_t MemID;

enum UMC_ALLOC_FLAGS
{
    UMC_ALLOC_PERSISTENT  = 0x01,
    UMC_ALLOC_FUNCLOCAL   = 0x02
};

class MemoryAllocatorParams
{
     DYNAMIC_CAST_DECL_BASE(MemoryAllocatorParams)

public:
    MemoryAllocatorParams(){}
    virtual ~MemoryAllocatorParams(){}
};

class MemoryAllocator
{
    DYNAMIC_CAST_DECL_BASE(MemoryAllocator)

public:
    MemoryAllocator(void){}
    virtual ~MemoryAllocator(void){}

    // Initiates object
    virtual Status Init(MemoryAllocatorParams *)  { return UMC_OK;}

    // Closes object and releases all allocated memory
    virtual Status Close() = 0;

    // Allocates or reserves physical memory and returns unique ID
    // Sets lock counter to 0
    virtual Status Alloc(MemID *pNewMemID, size_t Size, uint32_t Flags, uint32_t Align = 16) = 0;

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual void* Lock(MemID MID) = 0;

    // Unlock() decreases lock counter
    virtual Status Unlock(MemID MID) = 0;

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual Status Free(MemID MID) = 0;

    // Immediately deallocates memory regardless of whether it is in use (locked) or no
    virtual Status DeallocateMem(MemID MID) = 0;

protected:
    Mutex m_guard;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    MemoryAllocator(const MemoryAllocator &);
    MemoryAllocator & operator = (const MemoryAllocator &);
};

} //namespace UMC

#endif //__UMC_MEMORY_ALLOCATOR_H__
