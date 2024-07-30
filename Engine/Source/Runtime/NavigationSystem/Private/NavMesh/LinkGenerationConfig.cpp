// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavMesh/LinkGenerationConfig.h"
#include "BaseGeneratedNavLinksProxy.h"

#if WITH_RECAST
#include "Detour/DetourNavLinkBuilderConfig.h"

void FNavLinkGenerationJumpDownConfig::CopyToDetourConfig(dtNavLinkBuilderJumpDownConfig& OutDetourConfig) const
{
	OutDetourConfig.enabled = bEnabled;
	OutDetourConfig.jumpLength = JumpLength;
	OutDetourConfig.jumpDistanceFromEdge = JumpDistanceFromEdge;
	OutDetourConfig.jumpMaxDepth = JumpMaxDepth;
	OutDetourConfig.jumpHeight = JumpHeight;
	OutDetourConfig.jumpEndsHeightTolerance	= JumpEndsHeightTolerance;
	OutDetourConfig.samplingSeparationFactor = SamplingSeparationFactor;
	OutDetourConfig.filterDistanceThreshold = FilterDistanceThreshold;

	if (LinkProxy)
	{
		OutDetourConfig.linkUserId = LinkProxy->GetId().GetId();	
	}
}

#endif //WITH_RECAST
