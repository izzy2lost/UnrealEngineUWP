/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_CHUNK_MAP_ENTRY_H
#define VERSE_HEAP_CHUNK_MAP_ENTRY_H

#include "pas_compact_tagged_atomic_ptr.h"
#include "pas_page_kind.h"
#include "verse_heap_config.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct pas_stream;
struct verse_heap_chunk_map_entry;
struct verse_heap_large_entry;
typedef struct pas_stream pas_stream;
typedef struct verse_heap_chunk_map_entry verse_heap_chunk_map_entry;
typedef struct verse_heap_large_entry verse_heap_large_entry;

struct verse_heap_chunk_map_entry {
    pas_compact_tagged_atomic_ptr_impl encoded_value;
};

#define VERSE_HEAP_CHUNK_MAP_ENTRY_IS_EMPTY_VALUE ((pas_compact_tagged_atomic_ptr_impl)0)
#define VERSE_HEAP_CHUNK_MAP_ENTRY_IS_SMALL_SEGREGATED_BIT ((pas_compact_tagged_atomic_ptr_impl)1)
#define VERSE_HEAP_CHUNK_MAP_ENTRY_IS_MEDIUM_SEGREGATED_VALUE ((pas_compact_tagged_atomic_ptr_impl)2)

static PAS_ALWAYS_INLINE bool verse_heap_chunk_map_entry_is_empty(verse_heap_chunk_map_entry entry)
{
    return entry.encoded_value == VERSE_HEAP_CHUNK_MAP_ENTRY_IS_EMPTY_VALUE;
}

static PAS_ALWAYS_INLINE bool verse_heap_chunk_map_entry_is_small_segregated(verse_heap_chunk_map_entry entry)
{
    return !!(entry.encoded_value & VERSE_HEAP_CHUNK_MAP_ENTRY_IS_SMALL_SEGREGATED_BIT);
}

static PAS_ALWAYS_INLINE bool verse_heap_chunk_map_entry_is_medium_segregated(verse_heap_chunk_map_entry entry)
{
    return entry.encoded_value == VERSE_HEAP_CHUNK_MAP_ENTRY_IS_MEDIUM_SEGREGATED_VALUE;
}

static PAS_ALWAYS_INLINE bool verse_heap_chunk_map_entry_is_large(verse_heap_chunk_map_entry entry)
{
    return !verse_heap_chunk_map_entry_is_small_segregated(entry)
        && entry.encoded_value > VERSE_HEAP_CHUNK_MAP_ENTRY_IS_MEDIUM_SEGREGATED_VALUE;
}

static PAS_ALWAYS_INLINE unsigned verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(
    verse_heap_chunk_map_entry entry)
{
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_small_segregated(entry));
    return entry.encoded_value;
}

static PAS_ALWAYS_INLINE verse_heap_large_entry* verse_heap_chunk_map_entry_large_entry(
    verse_heap_chunk_map_entry entry)
{
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_large(entry));
    return (verse_heap_large_entry*)(entry.encoded_value + pas_compact_heap_reservation_base);
}

static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry verse_heap_chunk_map_entry_create_empty(void)
{
    verse_heap_chunk_map_entry result;
    result.encoded_value = 0;
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_empty(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_small_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_medium_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_large(result));
    return result;
}

static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry verse_heap_chunk_map_entry_create_large(
    verse_heap_large_entry* entry)
{
    verse_heap_chunk_map_entry result;
    uintptr_t offset;
    offset = (uintptr_t)entry - pas_compact_heap_reservation_base;
    PAS_ASSERT(entry);
    PAS_ASSERT(offset < pas_compact_heap_reservation_size);
    result.encoded_value = offset;
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_large(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_empty(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_small_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_medium_segregated(result));
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_large_entry(result) == entry);
    return result;
}

static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry verse_heap_chunk_map_entry_create_small_segregated(
    unsigned bitvector)
{
    verse_heap_chunk_map_entry result;
    result.encoded_value =
        VERSE_HEAP_CHUNK_MAP_ENTRY_IS_SMALL_SEGREGATED_BIT | bitvector;
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_small_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_large(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_medium_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_empty(result));
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(result)
                       == (bitvector | VERSE_HEAP_CHUNK_MAP_ENTRY_IS_SMALL_SEGREGATED_BIT));
    return result;
}

static PAS_ALWAYS_INLINE verse_heap_chunk_map_entry verse_heap_chunk_map_entry_create_medium_segregated(void)
{
    verse_heap_chunk_map_entry result;
    result.encoded_value = VERSE_HEAP_CHUNK_MAP_ENTRY_IS_MEDIUM_SEGREGATED_VALUE;
    PAS_TESTING_ASSERT(verse_heap_chunk_map_entry_is_medium_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_small_segregated(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_large(result));
    PAS_TESTING_ASSERT(!verse_heap_chunk_map_entry_is_empty(result));
    return result;
}

/* This is paranoia - I don't know if assigning struct values made up of atomic-sized fields results in the
   compiler emitting atomic stores/loads or not. Calling this side-steps that question. */
static PAS_ALWAYS_INLINE void verse_heap_chunk_map_entry_copy_atomically(
    verse_heap_chunk_map_entry* destination, verse_heap_chunk_map_entry* source)
{
    destination->encoded_value = source->encoded_value;
}

static PAS_ALWAYS_INLINE bool verse_heap_chunk_map_entry_weak_cas_atomically(
    verse_heap_chunk_map_entry* destination,
    verse_heap_chunk_map_entry expected, verse_heap_chunk_map_entry new_value)
{
    return pas_compare_and_swap_uint32_weak(
        &destination->encoded_value, expected.encoded_value, new_value.encoded_value);
}

PAS_API void verse_heap_chunk_map_entry_dump(verse_heap_chunk_map_entry entry, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_CHUNK_MAP_ENTRY_H */

