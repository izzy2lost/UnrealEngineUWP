// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "MeshVertexPaintingTool.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "MeshPaintMode.generated.h"

class UMeshPaintingToolProperties;
class UMeshVertexPaintingToolProperties;
class UMeshVertexColorPaintingToolProperties;
class UMeshTexturePaintingToolProperties;
class UMeshTextureColorPaintingToolProperties;
class UMeshTextureAssetPaintingToolProperties;
class UMeshPaintModeSettings;
class IMeshPaintComponentAdapter;
class UMeshComponent;
class UMeshToolManager;

/**
 * Mesh paint Mode.  Extends editor viewports with the ability to paint data on meshes
 */
UCLASS()
class UMeshPaintMode : public UEdMode, public ILegacyEdModeViewportInterface
{
public:
	GENERATED_BODY()

	/** Default constructor for UMeshPaintMode */
	UMeshPaintMode();
	static UMeshPaintingToolProperties* GetToolProperties();
	static UMeshVertexPaintingToolProperties* GetVertexToolProperties();
	static UMeshVertexColorPaintingToolProperties* GetVertexColorToolProperties();
	static UMeshVertexWeightPaintingToolProperties* GetVertexWeightToolProperties();
	static UMeshTexturePaintingToolProperties* GetTextureToolProperties();
	static UMeshTextureColorPaintingToolProperties* GetTextureColorToolProperties();
	static UMeshTextureAssetPaintingToolProperties* GetTextureAssetToolProperties();
	static UMeshPaintMode* GetMeshPaintMode();
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	static FName MeshPaintMode_VertexColor;
	static FName MeshPaintMode_VertexWeights;
	static FName MeshPaintMode_TextureColor;
	static FName MeshPaintMode_TextureAsset;
	static FString VertexSelectToolName;
	static FString TextureColorSelectToolName;
	static FString TextureAssetSelectToolName;
	static FString VertexColorPaintToolName;
	static FString VertexWeightPaintToolName;
	static FString TextureColorPaintToolName;
	static FString TextureAssetPaintToolName;

	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
	/** Returns the instance of ComponentClass found in the current Editor selection */
	template<typename ComponentClass>
	TArray<ComponentClass*> GetSelectedComponents() const;

	uint32 GetCachedVertexDataSize() const
	{
		return CachedVertexDataSize;
	}

protected:

	/** Binds UI commands to actions for the mesh paint mode */
	virtual void BindCommands() override;

	// UEdMode interface
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void ActivateDefaultTool() override;
	virtual void UpdateOnPaletteChange(FName NewPalette);
	// end UEdMode Interface
	
	void UpdateSelectedMeshes();

	void UpdateToolForSelection(const TArray<UMeshComponent*>& CurrentMeshComponents);

	void UpdateOnMaterialChange(bool bInvalidateHitProxies);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstanceMap);
	void OnResetViewMode();
	void OnVertexPaintFinished();

	void UpdateCachedVertexDataSize();

	bool IsInSelectTool() const;
	bool IsInPaintTool() const;

	// Start command bindings
	void SwapColors();
	bool CanSwapColors() const;
	void FillVertexColors();
	bool CanFillVertexColors() const;
	void FillTexture();
	bool CanFillTexture() const;
	void ApplyVertexColorsToAsset();
	bool CanApplyVertexColorsToAsset() const;
	void CommitTextureColorsToAsset();
	bool CanCommitTextureColorsToAsset() const;
	void PropagateVertexColorsToLODs();
	bool CanPropagateVertexColorsToLODs() const;
	void SaveVertexColorsToAssets();
	bool CanSaveVertexColorsToAssets() const;
	void SaveTexturePackages();
	bool CanSaveTexturePackages() const;
	void AddMeshPaintTextures();
	bool CanAddMeshPaintTextures() const;
	void RemoveInstanceVertexColors();
	bool CanRemoveInstanceVertexColors() const;
	void RemoveMeshPaintTexture();
	bool CanRemoveMeshPaintTextures() const;
	void CopyInstanceVertexColors();
	bool CanCopyInstanceVertexColors() const;
	void CopyMeshPaintTexture();
	bool CanCopyMeshPaintTexture() const;
	void Copy();
	bool CanCopy() const;
	void PasteInstanceVertexColors();
	bool CanPasteInstanceVertexColors() const;
	void PasteMeshPaintTexture();
	bool CanPasteMeshPaintTexture() const;
	void Paste();
	bool CanPaste() const;
	void ImportVertexColorsFromFile();
	bool CanImportVertexColorsFromFile() const;
	void ImportVertexColorsFromMeshPaintTexture();
	bool CanImportVertexColorsFromMeshPaintTexture() const;
	void ImportMeshPaintTextureFromVertexColors();
	bool CanImportMeshPaintTextureFromVertexColors() const;
	void FixVertexColors();
	bool CanFixVertexColors() const;
	void CycleMeshLODs(int32 Direction);
	bool CanCycleMeshLODs() const;
	void CycleTextures(int32 Direction);
	bool CanCycleTextures() const;
	// End command bindings

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMeshPaintModeSettings> ModeSettings;

	// End vertex paint state
	FGetSelectedMeshComponents MeshComponentDelegate;
	uint32 CachedVertexDataSize;
	bool bRecacheVertexDataSize;

	FDelegateHandle PaletteChangedHandle;
};

