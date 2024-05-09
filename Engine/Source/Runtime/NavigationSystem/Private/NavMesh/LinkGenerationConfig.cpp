// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavMesh/LinkGenerationConfig.h"

#if WITH_RECAST
#include "Detour/DetourNavLinkBuilderConfig.h"

void FNavLinkGenerationJumpDownConfig::CopyToDetourConfig(dtNavLinkBuilderJumpDownConfig& OutDetourConfig) const
{
	OutDetourConfig.jumpLength = JumpLength;
	OutDetourConfig.jumpDistanceFromEdge = JumpDistanceFromEdge;
	OutDetourConfig.jumpMaxDepth = JumpMaxDepth;
	OutDetourConfig.jumpEndsHeightTolerance	= JumpEndsHeightTolerance;
}

void FNavLinkGenerationJumpOverConfig::CopyToDetourConfig(dtNavLinkBuilderJumpOverConfig& OutDetourConfig) const
{
	OutDetourConfig.jumpLength = JumpLength;
	OutDetourConfig.jumpDistanceFromEdge = JumpDistanceFromEdge;
	OutDetourConfig.jumpHeight = JumpHeight;
	OutDetourConfig.jumpHeightTolerance = JumpHeightTolerance;
	OutDetourConfig.jumpEndsHeightTolerance	= JumpEndsHeightTolerance;
}
#endif //WITH_RECAST
