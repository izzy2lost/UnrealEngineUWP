// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"

class FMenuBuilder;
class UChaosClothComponent;
class FPrimitiveDrawInterface;
class FCanvas;
class FSceneView;
class STextComboBox;

namespace ESelectInfo
{
enum Type : int;
}

namespace UE::Chaos::ClothAsset
{

class FChaosClothAssetEditor3DViewportClient;

class FClothEditorSimulationVisualization
{
public:
	FClothEditorSimulationVisualization();

	void ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient);
	void DebugDrawSimulation(const UChaosClothComponent* ClothComponent, FPrimitiveDrawInterface* PDI);
	void DebugDrawSimulationTexts(const UChaosClothComponent* ClothComponent, FCanvas* Canvas, const FSceneView* SceneView);
	FText GetDisplayString(const UChaosClothComponent* ClothComponent) const;
	void RefreshMenusForClothComponent(const UChaosClothComponent* ClothComponent);

	// WeightMaps 
	const FString* GetCurrentlySelectedWeightMap() const { return CurrentlySelectedWeightMap.Get(); }
	void ExtendViewportShowMenuWeightMapSelector(FMenuBuilder& MenuBuilder, TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient);
private:
	/** Return whether or not - given the current enabled options - the simulation should be disabled. */
	bool ShouldDisableSimulation() const;
	/** Show/hide all cloth sections for the specified mesh compoment. */
	void ShowClothSections(UChaosClothComponent* ClothComponent, bool bIsClothSectionsVisible) const;
	/** Callback for weight map selection*/
	void WeightMapSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

private:
	/** Flags used to store the checked status for the visualization options. */
	TBitArray<> Flags;
	TSharedPtr<STextComboBox> WeightMapSelector;
	TArray<TSharedPtr<FString>> WeightMapNames;
	TSharedPtr<FString> CurrentlySelectedWeightMap;
	
};
} // namespace UE::Chaos::ClothAsset
