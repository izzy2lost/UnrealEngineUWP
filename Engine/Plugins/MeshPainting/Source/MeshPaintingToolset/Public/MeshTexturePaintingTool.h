// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/BaseBrushTool.h"
#include "BaseMeshPaintingToolProperties.h"
#include "MeshPaintHelpers.h"
#include "MeshPaintInteractions.h"
#include "MeshPaintingToolsetTypes.h"
#include "MeshVertexPaintingTool.h"
#include "Misc/ITransaction.h"

#include "MeshTexturePaintingTool.generated.h"

enum class EMeshPaintModeAction : uint8;
enum class EToolShutdownType : uint8;
class FScopedTransaction;
class IMeshPaintComponentAdapter;
class UMeshToolManager;
class UTexture2D;
class UTextureRenderTarget2D;
struct FPaintTexture2DData;
struct FTexturePaintMeshSectionInfo;
struct FTextureTargetListInfo;
struct FToolBuilderState;

/**
 * Builder for the texture color mesh paint tool.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureColorPaintingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	TWeakObjectPtr<UMeshToolManager> SharedMeshToolData;
};

/**
 * Builder for the texture asset mesh paint tool.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureAssetPaintingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	TWeakObjectPtr<UMeshToolManager> SharedMeshToolData;
};


/**
 * Base class for mesh texture paint properties.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTexturePaintingToolProperties : public UMeshPaintingToolProperties
{
	GENERATED_BODY()

public:
	/** Seam painting flag, True if we should enable dilation to allow the painting of texture seams */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	bool bEnableSeamPainting = false;

	/** Optional Texture Brush to which Painting should use */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayThumbnail = "true", TransientToolProperty))
	TObjectPtr<UTexture2D> PaintBrush = nullptr;

	/** Initial Rotation offset to apply to our paint brush */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty, UIMin = "0.0", UIMax = "360.0", ClampMin = "0.0", ClampMax = "360.0"))
	float PaintBrushRotationOffset = 0.0f;

	/** Whether or not to continously rotate the brush towards the painting direction */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty))
	bool bRotateBrushTowardsDirection = false;

	/** Whether or not to apply Texture Color Painting to the Red Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Red"))
	bool bWriteRed = true;

	/** Whether or not to apply Texture Color Painting to the Green Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Green"))
	bool bWriteGreen = true;

	/** Whether or not to apply Texture Color Painting to the Blue Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Blue"))
	bool bWriteBlue = true;

	/** Whether or not to apply Texture Color Painting to the Alpha Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Alpha"))
	bool bWriteAlpha = false;
};

/**
 * Class for texture color paint properties.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureColorPaintingToolProperties : public UMeshTexturePaintingToolProperties
{
	GENERATED_BODY()
};

/**
 * Class for texture asset paint properties.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureAssetPaintingToolProperties : public UMeshTexturePaintingToolProperties
{
	GENERATED_BODY()

public:
	/** UV channel which should be used for painting textures. */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (TransientToolProperty))
	int32 UVChannel = 0;

	/** Texture to which painting should be applied. */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayThumbnail = "true", TransientToolProperty))
	TObjectPtr<UTexture2D> PaintTexture;
};


/**
 * Base class for mesh texture painting tool.
 */
UCLASS(Abstract)
class MESHPAINTINGTOOLSET_API UMeshTexturePaintingTool : public UBaseBrushTool, public IMeshPaintSelectionInterface
{
	GENERATED_BODY()

public:
	UMeshTexturePaintingTool();

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override;
	virtual bool CanAccept() const override;
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual	bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual double EstimateMaximumTargetDimension() override;
	virtual bool IsPainting() const
	{
		return bArePainting;
	}

	FSimpleDelegate& OnPaintingFinished()
	{
		return OnPaintingFinishedDelegate;
	}

	void CommitAllPaintedTextures();
	void ClearAllTextureOverrides();
	
	/** Returns the number of texture that require a commit. */
	int32 GetNumberOfPendingPaintChanges() const;

	void FloodCurrentPaintTexture();
	
	virtual void GetModifiedTexturesToSave(TArray<UObject*>& OutTexturesToSave) const {}

protected:
	virtual void SetAdditionalPaintParameters(FMeshPaintParameters& InPaintParameters) {};
	virtual void FinishPainting();
	void UpdateResult();
	double CalculateTargetEdgeLength(int TargetTriCount);
	bool Paint(const FVector& InRayOrigin, const FVector& InRayDirection);
	bool Paint(const TArrayView<TPair<FVector, FVector>>& Rays);
	virtual void CacheSelectionData();
	FPaintTexture2DData* GetPaintTargetData(const UTexture2D* InTexture);
	FPaintTexture2DData* AddPaintTargetData(UTexture2D* InTexture);
	void GatherTextureTriangles(IMeshPaintComponentAdapter* Adapter, int32 TriangleIndex, const int32 VertexIndices[3], TArray<FTexturePaintTriangleInfo>* TriangleInfo, TArray<FTexturePaintMeshSectionInfo>* SectionInfos, int32 UVChannelIndex);
	void StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintComponentAdapter& GeometryInfo);
	void PaintTexture(FMeshPaintParameters& InParams, int32 UVChannel, TArray<FTexturePaintTriangleInfo>& InInfluencedTriangles, const IMeshPaintComponentAdapter& GeometryInfo, FMeshPaintParameters* LastParams = nullptr);
	void FinishPaintingTexture();
	void OnTransactionStateChanged(const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState);

	virtual UTexture2D* GetSelectedPaintTexture(UMeshComponent const* InMeshComponent) const { return nullptr; }
	virtual int32 GetSelectedUVChannel(UMeshComponent const* InMeshComponent) const { return 0; }
	virtual void CacheTexturePaintData() {}

private:
	bool PaintInternal(const TArrayView<TPair<FVector, FVector>>& Rays, EMeshPaintModeAction PaintAction, float PaintStrength);

	void AddTextureOverrideToComponent(FPaintTexture2DData& TextureData, UMeshComponent* MeshComponent, const IMeshPaintComponentAdapter* MeshPaintAdapter = nullptr);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMeshPaintSelectionMechanic> SelectionMechanic;

	/** Textures eligible for painting retrieved from the current selection */
	TArray<FPaintableTexture> PaintableTextures;

	UPROPERTY(Transient)
	TObjectPtr<UMeshTexturePaintingToolProperties> TextureProperties;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UTexture>> Textures;

	/** Stores data associated with our paint target textures */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTexture2D>, FPaintTexture2DData> PaintTargetData;

	/** Store the component overrides active for each paint target textures
	 * Note this is not transactional because we use it as cache of the current state of the scene that we can clean/update after each transaction.
	 */
	UPROPERTY(Transient, NonTransactional)
	TMap<TObjectPtr<UTexture2D>, FPaintComponentOverride> PaintComponentsOverride;

	/** Texture paint: Will hold a list of texture items that we can paint on */
	TArray<FTextureTargetListInfo> TexturePaintTargetList;

	/** Texture paint: The mesh components that we're currently painting */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> TexturePaintingCurrentMeshComponent;

	/** The original texture that we're painting */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PaintingTexture2D;

	/** Hold the transaction while we are painting */
	TUniquePtr<FScopedTransaction> PaintingTransaction;

	double InitialMeshArea;
	bool bResultValid;
	bool bStampPending;
	bool bInDrag;
	FRay PendingStampRay;
	FRay PendingClickRay;
	FVector2D PendingClickScreenPosition;
	bool bCachedClickRay;

	TArray<FPaintRayResults> LastPaintRayResults;
	bool bRequestPaintBucketFill = false;

	/** Flag for whether or not we are currently painting */
	bool bArePainting;
	bool bDoRestoreRenTargets;
	/** Time kept since the user has started painting */
	float TimeSinceStartedPainting;
	/** Overall time value kept for drawing effects */
	float Time;
	FHitResult LastBestHitResult;
	FSimpleDelegate OnPaintingFinishedDelegate;
	/** Texture paint state */
	/** Cached / stored instance texture paint settings for selected components */
	TMap<UMeshComponent*, FInstanceTexturePaintSettings> ComponentToTexturePaintSettingsMap;
};

/**
 * Class for texture color painting tool.
 * This paints to special textures stored on the mesh components.
 * Behavior should be similar to vertex painting (per instance painting stored on components).
 * But painting texture colors instead of vertex colors is a better fit for very dense mesh types such as used by nanite.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureColorPaintingTool : public UMeshTexturePaintingTool
{
	GENERATED_BODY()

public:
	UMeshTextureColorPaintingTool();

	// Begin UInteractiveTool Interface.
	virtual void Setup() override;
	// End UInteractiveTool Interface.

	// Begin UMeshTexturePaintingTool Interface.
	virtual bool AllowsMultiselect() const override { return true; }
	virtual bool IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const override;
	virtual UTexture2D* GetSelectedPaintTexture(UMeshComponent const* InMeshComponent) const override;
	virtual int32 GetSelectedUVChannel(UMeshComponent const* InMeshComponent) const override;
	virtual void GetModifiedTexturesToSave(TArray<UObject*>& OutTexturesToSave) const override;
	virtual void CacheTexturePaintData() override;
	// End UMeshTexturePaintingTool Interface.

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMeshTextureColorPaintingToolProperties> ColorProperties;
};

/**
 * Class for texture asset painting tool.
 * This paints to texture assets directly from the mesh.
 * The texture asset to paint is selected from the ones referenced in the mesh component's materials.
 */
UCLASS()
class MESHPAINTINGTOOLSET_API UMeshTextureAssetPaintingTool : public UMeshTexturePaintingTool
{
	GENERATED_BODY()

public:
	UMeshTextureAssetPaintingTool();
	
	// Begin UInteractiveTool Interface.
	virtual void Setup() override;
	// End UInteractiveTool Interface.

	// Begin UMeshTexturePaintingTool Interface.
	virtual bool AllowsMultiselect() const override { return false; }
	virtual bool IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const override;
	virtual UTexture2D* GetSelectedPaintTexture(UMeshComponent const* InMeshComponent) const override;
	virtual int32 GetSelectedUVChannel(UMeshComponent const* InMeshComponent) const override;
	virtual void GetModifiedTexturesToSave(TArray<UObject*>& OutTexturesToSave) const override;
	virtual void CacheTexturePaintData() override;
	// End UMeshTexturePaintingTool Interface.

	/** Change selected texture to previous or next available. */
	void CycleTextures(int32 Direction);
	/** Returns true if asset shouldn't be shown in UI because it is not in our paintable texture array. */
	bool ShouldFilterTextureAsset(const FAssetData& AssetData) const;
	/** Call on change to selected paint texture */
	void PaintTextureChanged(const FAssetData& AssetData);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMeshTextureAssetPaintingToolProperties> AssetProperties;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "MeshVertexPaintingTool.h"
#include "TexturePaintToolset.h"
#include "UObject/NoExportTypes.h"
#endif
