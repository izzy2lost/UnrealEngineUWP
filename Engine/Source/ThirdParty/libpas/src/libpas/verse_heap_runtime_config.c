/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_runtime_config.h"

#include "verse_heap.h"

#if PAS_ENABLE_VERSE

pas_allocation_result verse_heap_runtime_config_allocate_chunks(verse_heap_runtime_config* config,
                                                                size_t size)
{
    pas_large_free_heap_config page_cache_config;
    pas_simple_large_free_heap* page_cache;
    pas_allocation_result result;

    PAS_ASSERT(pas_is_aligned(size, VERSE_HEAP_CHUNK_SIZE));

    if (config->heap_base) {
        PAS_ASSERT(config->heap_size);
        PAS_ASSERT(config->heap_alignment);
        PAS_ASSERT(config->page_cache);
        page_cache_config.type_size = 1;
        page_cache_config.min_alignment = 1;
        page_cache_config.aligned_allocator = NULL;
        page_cache_config.aligned_allocator_arg = NULL;
        page_cache_config.deallocator = NULL;
        page_cache_config.deallocator_arg = NULL;
        page_cache = config->page_cache;
    } else {
        PAS_ASSERT(!config->heap_size);
        PAS_ASSERT(!config->heap_alignment);
        PAS_ASSERT(!config->page_cache);
        verse_heap_initialize_page_cache_config(&page_cache_config);
        page_cache = &verse_heap_page_cache;
    }

    result = pas_simple_large_free_heap_try_allocate(
        page_cache, size, pas_alignment_create_traditional(VERSE_HEAP_CHUNK_SIZE), &page_cache_config);
    
    if (result.did_succeed) {
        uintptr_t address;
        PAS_ASSERT(result.zero_mode);
        for (address = result.begin; address < result.begin + size; address += VERSE_HEAP_CHUNK_SIZE)
            verse_heap_initialize_chunk_map_entry_ptr(address);
    }
    
    return result;
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */


