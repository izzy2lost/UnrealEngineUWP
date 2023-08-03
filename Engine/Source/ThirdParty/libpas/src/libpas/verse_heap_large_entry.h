/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_LARGE_ENTRY_H
#define VERSE_HEAP_LARGE_ENTRY_H

#include "pas_utils.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct pas_large_heap;
struct verse_heap_large_entry;
typedef struct pas_large_heap pas_large_heap;
typedef struct verse_heap_large_entry verse_heap_large_entry;

struct verse_heap_large_entry {
    uintptr_t begin;
    uintptr_t end;
    pas_heap* heap;
};

/* These must be called with the heap lock held. */
PAS_API verse_heap_large_entry* verse_heap_large_entry_create(uintptr_t begin, uintptr_t end, pas_heap* heap);
PAS_API void verse_heap_large_entry_destroy(verse_heap_large_entry* entry);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_LARGE_ENTRY_H */

