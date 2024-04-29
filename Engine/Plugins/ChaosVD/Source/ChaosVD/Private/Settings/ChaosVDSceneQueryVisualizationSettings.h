// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDCoreSettings.h"

#include "ChaosVDSceneQueryVisualizationSettings.generated.h"

/** Set of visualization flags options for Scene Queries */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDSceneQueryVisualizationFlags: uint32
{
	None					= 0 UMETA(Hidden),
	EnableDraw				= 1 << 0,
	DrawLineTraceQueries	= 1 << 1,
	DrawSweepQueries		= 1 << 2,
	DrawOverlapQueries		= 1 << 3,
	DrawHits				= 1 << 4,
	OnlyDrawSelectedQuery	= 1 << 5,
	HideEmptyQueries		= 1 << 6,
	HideSubQueries			= 1 << 7,
};
ENUM_CLASS_FLAGS(EChaosVDSceneQueryVisualizationFlags);

UCLASS(config=ChaosVD)
class UChaosVDSceneQueriesVisualizationSettings : public UChaosVDVisualizationSettingsObjectBase
{
	GENERATED_BODY()

public:

	static void SetSceneQueryDataVisualizationFlags(EChaosVDSceneQueryVisualizationFlags NewFlags);
	static EChaosVDSceneQueryVisualizationFlags GetSceneQueryDataVisualizationFlags();

	/** If true, any debug draw text available will be drawn */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	bool bShowText = false;

private:
	/** Set of flags to enable/disable visualization of specific scene queries data as debug draw */
	EChaosVDSceneQueryVisualizationFlags GlobalSceneQueriesVisualizationFlags = EChaosVDSceneQueryVisualizationFlags::DrawHits | EChaosVDSceneQueryVisualizationFlags::DrawLineTraceQueries;
};
