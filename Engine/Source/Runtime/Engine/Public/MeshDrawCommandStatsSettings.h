// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/NameTypes.h"

#include "MeshDrawCommandStatsSettings.generated.h"

/** Description of a stat category used in the MeshDrawCommandStats system. */
USTRUCT()
struct FMeshDrawCommandStatsBudget
{
	GENERATED_BODY()

	/** Category name. */
	UPROPERTY(config, EditAnywhere, Category = Engine)
	FName CategoryName;
	/** The category primitive budget. This is the maximum triangles expected, post-culling, summed across all passes. */
	UPROPERTY(config, EditAnywhere, Category = Engine)
	int32 PrimitiveBudget = 0;
};

/** User settings used by the MeshDrawCommandStats system. */
UCLASS(Config=Engine, defaultconfig, meta = (DisplayName = "Mesh Stats"))
class ENGINE_API UMeshDrawCommandStatsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Budgets used by r.MeshDrawCommands.Stats */
	UPROPERTY(config, EditAnywhere, Category = Engine)
	TArray<FMeshDrawCommandStatsBudget> Budgets;
	/** The total primitive budget. This is the maximimum triangles expected, post-culling, summed across all passes. */
	UPROPERTY(config, EditAnywhere, Category = Engine)
	int32 TotalPrimitiveBudget = 0;
};
