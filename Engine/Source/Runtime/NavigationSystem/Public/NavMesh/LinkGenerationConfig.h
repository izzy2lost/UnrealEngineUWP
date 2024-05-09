// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LinkGenerationConfig.generated.h"

#if WITH_RECAST
struct dtNavLinkBuilderJumpDownConfig;
struct dtNavLinkBuilderJumpOverConfig;
#endif //WITH_RECAST

/** Experimental configuration for generated jump down links. */
USTRUCT()
struct FNavLinkGenerationJumpDownConfig
{
	GENERATED_USTRUCT_BODY()

	// @todo: Rename, describe and find best defaults for those parameters.
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpLength = 150.f; 

	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpDistanceFromEdge = 10.f; 
	
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpMaxDepth = 150.f;

	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpEndsHeightTolerance = 50.f;

#if WITH_RECAST	
	/** Copy configuration to dtNavLinkBuilderJumpDownConfig. */
	void CopyToDetourConfig(dtNavLinkBuilderJumpDownConfig& OutDetourConfig) const;
#endif //WITH_RECAST
};

/** Experimental configuration for generated jump over links. */
USTRUCT()
struct FNavLinkGenerationJumpOverConfig
{
	GENERATED_USTRUCT_BODY()

	// @todo: Rename, describe and find best defaults for those parameters.
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpLength = 200.f; 

	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpDistanceFromEdge = 100.f; 
	
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpHeight = 100.f; 

	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpHeightTolerance = 100.f; 

	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpEndsHeightTolerance = 50.f;

#if WITH_RECAST
	/** Copy configuration to dtNavLinkBuilderJumpOverConfig. */
	void CopyToDetourConfig(dtNavLinkBuilderJumpOverConfig& OutDetourConfig) const;
#endif //WITH_RECAST
};
