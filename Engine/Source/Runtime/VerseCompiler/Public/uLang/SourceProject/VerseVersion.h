// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <cstdint>

namespace Verse
{
    namespace Version
    {
        static constexpr uint32_t Primordial = 0; // A retroactively defined version for pre-versioned Verse.

        static constexpr uint32_t V1 = 1;
        // Changes in V1 (note that more may be added as long as LatestStable < V1):
        static constexpr uint32_t SetMutatesFallibility = V1;
        static constexpr uint32_t MapLiteralKeysHandleIterationAndFailure = V1;
        static constexpr uint32_t DontMixCommaAndSemicolonInBlocks = V1;
        static constexpr uint32_t UniqueAttributeRequiresAllocatesEffect = V1;
        static constexpr uint32_t LocalQualifiers = V1;
        static constexpr uint32_t StructFieldsMustBePublic = V1;

        static constexpr uint32_t LatestStable = Primordial;
        static constexpr uint32_t LatestUnstable = V1;

        static constexpr uint32_t Minimum = Primordial;
        static constexpr uint32_t Default = LatestStable;
        static constexpr uint32_t Maximum = LatestUnstable;
    }
}
