/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utility_heap.h"
#include "verse_heap_large_entry.h"

#if PAS_ENABLE_VERSE

verse_heap_large_entry* verse_heap_large_entry_create(uintptr_t begin, uintptr_t end, pas_heap* heap)
{
    verse_heap_large_entry* result;

    result = (verse_heap_large_entry*)pas_utility_heap_allocate(
        sizeof(verse_heap_large_entry), "verse_heap_large_entry");

    result->begin = begin;
    result->end = end;
    result->heap = heap;

    return result;
}

void verse_heap_large_entry_destroy(verse_heap_large_entry* entry)
{
    pas_utility_heap_deallocate(entry);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */
