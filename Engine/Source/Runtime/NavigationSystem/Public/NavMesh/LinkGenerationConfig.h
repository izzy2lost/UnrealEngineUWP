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

	/** Should this config be used to generate links. */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	bool bEnabled = true;
	
	/** Horizontal length of the jump. How far from the starting point we will look for ground. */ 
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpLength = 150.f; 

	/** How far from the edge is the jump started */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpDistanceFromEdge = 10.f; 

	/** How far below the starting height we want to look for landing ground. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpMaxDepth = 150.f;

	/** Tolerance at both ends of the jump to find ground. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpEndsHeightTolerance = 80.f;

	/** Value multiplied by CellSize to find the distance between sampling trajectories. Default is 1. */
    /*  Larger values improve generation speed but might introduce sampling errors.  */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=1, ClampMin=1))
	float SamplingSeparationFactor = 1.f;
	
	/** When filtering similar links, distance used to compare between segment endpoints to match similar links. Use greater distance for more filtering (0 to deactivate filtering). */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float FilterDistanceThreshold = 80.f;

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

	/** Should this config be used to generate links. */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	bool bEnabled = true;
	
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
	float JumpEndsHeightTolerance = 80.f;

	/** Value multiplied by CellSize to find the distance between sampling trajectories. Default is 1. */
    /*  Larger values improve generation speed but might introduce sampling errors.  */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=1, ClampMin=1))
	float SamplingSeparationFactor = 1.f;

	/** When filtering similar links, distance used to compare between segment endpoints to match similar links. Use greater distance for more filtering (0 to deactivate filtering). */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float FilterDistanceThreshold = 80.f;

#if WITH_RECAST
	/** Copy configuration to dtNavLinkBuilderJumpOverConfig. */
	void CopyToDetourConfig(dtNavLinkBuilderJumpOverConfig& OutDetourConfig) const;
#endif //WITH_RECAST
};
