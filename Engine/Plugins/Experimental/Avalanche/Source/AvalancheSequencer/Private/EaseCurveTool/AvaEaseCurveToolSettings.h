// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/EnumAsByte.h"
#include "Engine/DeveloperSettings.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "AvaEaseCurveToolSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "Ease Curve Tool"))
class UAvaEaseCurveToolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAvaEaseCurveToolSettings();

	int32 GetGraphSize() const { return GraphSize; }
	void SetGraphSize(const int32 InNewSize) { GraphSize = InNewSize; }

	EHorizontalAlignment GetGraphHAlign() const { return GraphHAlign.GetValue(); }
	void SetGraphHAlign(const EHorizontalAlignment InNewHAlign) { GraphHAlign = InNewHAlign; }

	bool GetGridSnap() const { return bGridSnap; }
	void SetGridSnap(const bool bInCurveEaseToolGridSnap) { bGridSnap = bInCurveEaseToolGridSnap; }
	void ToggleGridSnap() { bGridSnap = !bGridSnap; }

	int32 GetGridSize() const { return GridSize; }
	void SetGridSize(const int32 InNewSize) { GridSize = InNewSize; }

	bool GetAutoFlipTangents() const { return bAutoFlipTangents; }
	void SetAutoFlipTangents(const bool bInAutoFlipTangents) { bAutoFlipTangents = bInAutoFlipTangents; }
	void ToggleAutoFlipTangents() { bAutoFlipTangents = !bAutoFlipTangents; }

	FString GetPresetsPath() const { return PresetsPath; }
	FString GetNewPresetCategory() const { return NewPresetCategory; }

private:
	/** The height of the curve ease tool in the details panel. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Graph Size", UIMin = 64, ClampMin = 64, UIMax = 256, ClampMax = 256, Delta = 1))
	int32 GraphSize = 140;

	/** The horizontal alignment of the curve ease tool in the details panel. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Graph Horizontal Alignment"))
	TEnumAsByte<EHorizontalAlignment> GraphHAlign = EHorizontalAlignment::HAlign_Fill;

	/** If true, snaps tangents to grid. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Grid Snap"))
	bool bGridSnap = false;

	/** The height of the curve ease tool in the details panel. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Grid Size", UIMin = 4, ClampMin = 4, UIMax = 24, ClampMax = 24, Delta = 1))
	int32 GridSize = 8;

	/** If true, auto flips tangents when sequential key frame curve values are descending. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Auto Flip Tangents"))
	bool bAutoFlipTangents = true;

	/** The location to search for ease curve preset Json files. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "Presets Path"))
	FString PresetsPath;

	/** The name of the category to place newly created curve presets. */
	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTool", meta = (DisplayName = "New Preset Category"))
	FString NewPresetCategory;
};
