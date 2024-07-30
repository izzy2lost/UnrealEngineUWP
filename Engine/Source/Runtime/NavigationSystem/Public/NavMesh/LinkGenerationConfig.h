// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavigationTypes.h"

#include "LinkGenerationConfig.generated.h"

class UBaseGeneratedNavLinksProxy;

#if WITH_RECAST
struct dtNavLinkBuilderJumpDownConfig;
struct dtNavLinkBuilderJumpOverConfig;
#endif //WITH_RECAST

/** Experimental configuration for generated jump down links. */
USTRUCT()
struct FNavLinkGenerationJumpDownConfig
{
	GENERATED_BODY()

	/** Should this config be used to generate links. */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	bool bEnabled = true;
	
	/** Horizontal length of the jump. How far from the starting point we will look for ground. */ 
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpLength = 150.f; 

	/** How far from the edge is the jump started. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpDistanceFromEdge = 10.f; 

	/** How far below the starting height we want to look for landing ground. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpMaxDepth = 150.f;

	/** Peak height relative to the height of the starting point. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(Units=cm, UIMin=0, ClampMin=0))
	float JumpHeight = 50.f;

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

	/** Class used to handle links made with this configuration.
	 * Using this allows to implement custom behaviors when using navlinks, for example during the pathfollow.
	 * Note that having a proxy is not required for successful navlink pathfinding, but it does allow for custom behavior at the start and the end of a given navlink.
	 * This implies that using LinkProxyClass is optional and it can remain empty (the default value).
	 * @see INavLinkCustomInterface 
	 * @see UGeneratedNavLinksProxy
	 */
	UPROPERTY(EditAnywhere, Category= Settings)
	TSubclassOf<UBaseGeneratedNavLinksProxy> LinkProxyClass;

	/** Identifier used identify the current proxy handler. All links generated through this config will use the same handler. */
	UPROPERTY()
	FNavLinkId LinkProxyId;

	/** Current proxy. The proxy instance is build from the LinkProxyClass (provided it's not null).
	 * A proxy will be created if a @see LinkProxyClass is used.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UBaseGeneratedNavLinksProxy> LinkProxy = nullptr;

	/** Is the link proxy registered to the navigation system CustomNavLinksMap.
	 * Registration occurs on PostRegisterAllComponents or on PostLoadPreRebuild if a new proxy was created. */
	UPROPERTY(Transient)
	bool bLinkProxyRegistered = false;

#if WITH_RECAST	
	/** Copy configuration to dtNavLinkBuilderJumpDownConfig. */
	void CopyToDetourConfig(dtNavLinkBuilderJumpDownConfig& OutDetourConfig) const;
#endif //WITH_RECAST
};

