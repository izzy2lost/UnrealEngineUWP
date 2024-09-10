// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Baking/BakingTypes.h"
#include "CoreMinimal.h"
#include "Drawing/PreviewGeometryActor.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolQueryInterfaces.h"
#include "MeshOpPreviewHelpers.h"
#include "Sampling/MeshMapBaker.h"
#include "Selection/UVToolSelectionAPI.h"

#include "UVEditorUVSnapshotTool.generated.h"


// Forward Declarations
class UUVEditorBakeUVShellResultProperties;
class UUVEditorBakeUVShellProperties;
enum class EBakeMapType;

/**
 * Bake compute state
 */
enum class EUVSnapshotBakeOpState
{
	Clean              = 0,      // No-op - evaluation already launched/complete.
	Evaluate           = 1 << 0, // Inputs are modified and valid, re-evaluate.
	Invalid            = 1 << 1  // Inputs are modified and invalid - retry eval until valid.
};
ENUM_CLASS_FLAGS(EUVSnapshotBakeOpState);

/**
 * ToolBuilder
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorUVSnapshotToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
	// This is a pointer so that it can be updated under the builder without
	// having to set it in the mode after initializing targets.
	// Supports only one target
	const TArray<TObjectPtr<UUVEditorToolMeshInput>>* Targets = nullptr;
};

/**
 * UV Snapshot Tool
 *
 * Exports a texture asset of a UV Layout
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorUVSnapshotTool : public UInteractiveTool, public IUVToolSupportsSelection, public UE::Geometry::IGenericDataOperatorFactory<UE::Geometry::FMeshMapBaker>
{
	GENERATED_BODY()
public:
	// Begin UInteractiveTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }
	// End UInteractiveTool interface

	// Begin IGenericDataOperatorFactory interface
	virtual TUniquePtr<UE::Geometry::TGenericDataOperator<UE::Geometry::FMeshMapBaker>> MakeNewOperator() override;
	// End IGenericDataOperatorFactory interface
	
	/**
	 * The tool will operate on the mesh given here. Supports only one mesh.
	 */
	virtual void SetTarget(const TObjectPtr<UUVEditorToolMeshInput>& TargetIn)
	{
		Target = TargetIn;
	}
protected:
	//
	// Mesh input to UV Editor
	//
	UPROPERTY()
	TObjectPtr<UUVEditorToolMeshInput> Target;

	//
	// Property sets for bake and result
	//
	UPROPERTY()
	TObjectPtr<UUVEditorBakeUVShellProperties> UVShellSettings;
	UPROPERTY()
	TObjectPtr<UUVEditorBakeUVShellResultProperties> ResultSettings;

	//
	// Preview Geometry for display in Unwrapped viewport
	//
	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeoBackgroundQuad;

	//
	// Background compute
	//
	TUniquePtr<TGenericDataBackgroundCompute<UE::Geometry::FMeshMapBaker>> Compute = nullptr;

public:
	// Bake settings
	struct FUVSnapshotBakeSettings
	{
		UE::Geometry::FImageDimensions Dimensions;
		int UVLayer = 0;
		float WireframeThickness = 1.0f;
		FLinearColor WireframeColor = FLinearColor::Blue;
		FLinearColor ShellColor = FLinearColor::Gray;
		FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		int SamplesPerPixel = 4;
		bool operator==(const FUVSnapshotBakeSettings& Other) const
		{
			return Dimensions == Other.Dimensions &&
				UVLayer == Other.UVLayer &&
				WireframeThickness == Other.WireframeThickness &&
				WireframeColor == Other.WireframeColor &&
				ShellColor == Other.ShellColor &&
				BackgroundColor == Other.BackgroundColor &&
				SamplesPerPixel == Other.SamplesPerPixel;
		}
	};
protected:
	FUVSnapshotBakeSettings CachedBakeSettings;
	
private:
	EUVSnapshotBakeOpState OpState = EUVSnapshotBakeOpState::Evaluate;

	// used in the bake
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> DetailMesh;
	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3, ESPMode::ThreadSafe> DetailSpatial;

	/**
	 * Process OpState flags and update background compute
	 * Invoked during render
	 */
	void UpdateResult();

	/**
	 * Retrieves the result of the FMeshMapBaker and generated UTexture2D into the CachedUVMap
	 * 
	 * @param NewResult the resulting FMeshMapBaker from the background compute
	 */
	void OnMapUpdated(const TUniquePtr<UE::Geometry::FMeshMapBaker>& NewResult);

	/**
	 * Sets the result to a nullptr
	 */
	void InvalidateResults() const;

	/**
	 * Invalidates the background compute operator.
	 */
	void InvalidateCompute();

	/**
	 * Updates the preview material on the preview mesh with the
	 * computed results. Invoked by OnMapsUpdated.
	 * Also sets the result to what is currently in CachedUVMap
	 */
	void UpdateVisualization();

	/**
	 * Updates the FUVSnapshotBakeSettings and CachedBakeSettings
	 *
	 * @return current result state (Clean or Evaluate if Bake settings have changed)
	 */
	EUVSnapshotBakeOpState UpdateResult_UVShellMap();

	/**
	 * Internal cache of bake uv texture result
	 */
	UPROPERTY()
	TObjectPtr<UTexture2D> CachedUVMap = nullptr;

	/**
	 * Uses Preview Geometry to draw a preview of the bake in the unwrap viewport
	 */
	void UpdatePreviewMaterialBasedOnBackground();

	/**
	 * Create texture asset from our result Texture2D
	 * @param Texture the result texture to create
	 * @param SourceWorld the source world to define where the texture asset will be stored.
	 * @param SourceAsset if not null, result texture will be stored adjacent to this asset.
	 */
	void CreateTextureAsset(const TObjectPtr<UTexture2D>& Texture, UWorld* SourceWorld, UObject* SourceAsset) const;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorBakeUVShellResultProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Bake */
	UPROPERTY(VisibleAnywhere, Category = Results, meta = (DisplayName = "Result Texture", TransientToolProperty))
	TObjectPtr<UTexture2D> Result;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorBakeUVShellProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** The source mesh UV Layer to sample. */
	UPROPERTY(EditAnywhere, Category = "UV Snapshot Output", meta = (DisplayName = "UV Layer"))
	int UVLayer = 0;

	/** The thickness of the wireframe in pixels. */
	UPROPERTY(EditAnywhere, Category = "UV Snapshot Output", meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0"))
	float WireframeThickness = 1.0f;

	/** The color of wireframe pixels. */
	UPROPERTY(EditAnywhere, Category = "UV Snapshot Output")
	FLinearColor WireframeColor = FLinearColor::Blue;

	/** The color of the UV shell interior pixels. */
	UPROPERTY(EditAnywhere, Category = "UV Snapshot Output")
	FLinearColor ShellColor = FLinearColor::Gray;

	/** The color of pixels external to UV shells. */
	UPROPERTY(EditAnywhere, Category = "UV Snapshot Output")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/** The pixel resolution of the generated textures */
	UPROPERTY(EditAnywhere, Category = Textures)
	EBakeTextureResolution Resolution = EBakeTextureResolution::Resolution256;

	/** Number of samples per pixel */
	UPROPERTY(EditAnywhere, Category = Textures)
	EBakeTextureSamplesPerPixel SamplesPerPixel = EBakeTextureSamplesPerPixel::Sample4;

	/** Saved path where last UVSnapshot was saved to. Empty if this is first save out */
	UPROPERTY()
	FString SavedPath = FString();
};