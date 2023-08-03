/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_PAGE_HEADER_H
#define VERSE_HEAP_PAGE_HEADER_H

#include "pas_utils.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

#define VERSE_HEAP_FIRST_VERSION ((uint64_t)10)

struct verse_heap_page_header;
typedef struct verse_heap_page_header verse_heap_page_header;

struct PAS_ALIGNED(PAS_PAIR_SIZE) verse_heap_page_header {
    uint64_t version;
    bool may_have_set_mark_bits_for_dead_objects;
};

#define VERSE_HEAP_PAGE_HEADER_INITIALIZER ((verse_heap_page_header){ \
        .version = VERSE_HEAP_FIRST_VERSION, \
        .may_have_set_mark_bits_for_dead_objects = false \
    })

PAS_API void verse_heap_page_header_construct(verse_heap_page_header* header);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_PAGE_HEADER_H */

