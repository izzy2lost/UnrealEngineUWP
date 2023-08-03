/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap.h"
#include "verse_heap_page_header.h"

#if PAS_ENABLE_VERSE

void verse_heap_page_header_construct(verse_heap_page_header* header)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Allocating verse header %p with latest version %" PRIu64 "\n", header, verse_heap_latest_version);
    header->version = verse_heap_latest_version;
    header->may_have_set_mark_bits_for_dead_objects = false;
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

