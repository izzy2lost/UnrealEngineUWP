// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"
#include "UObject/GCObject.h"

#include "Delegates/IDelegateInstance.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

class APostProcessVolume;
class FAdvancedPreviewScene;
class FDMMaterialPreviewViewportClient;
class FEditorViewportClient;
class SDMMaterialEditor;
class UDynamicMaterialModelBase;
class UMaterialInterface;
class UMeshComponent;
enum class EDMMaterialPreviewMesh : uint8;
struct FPropertyChangedEvent;

/** Based on SMaterialEditor3DPreviewViewport (private) */
class SDMMaterialPreview : public SEditorViewport, public FGCObject
{
	SLATE_DECLARE_WIDGET(SDMMaterialPreview, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialPreview) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialPreview() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget, UDynamicMaterialModelBase* InMaterialModelBase);

	//~ Begin SEditorViewport
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	//~ End SEditorViewport

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject

protected:
	TWeakPtr<SDMMaterialEditor> EditorWidgetWeak;

	TSharedPtr<FDMMaterialPreviewViewportClient> EditorViewportClient;
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;

	TObjectPtr<UMeshComponent> PreviewMeshComponent;
	TObjectPtr<UMaterialInterface> PreviewMaterial;
	TObjectPtr<APostProcessVolume> PostProcessVolumeActor;

	void RefreshViewport();

	void SetPreviewType(EDMMaterialPreviewMesh InPrimitiveType);

	ECheckBoxState IsPreviewTypeSet(EDMMaterialPreviewMesh InPrimitiveType) const;

	void SetPreviewAsset(UObject* InAsset);

	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);

	void ApplyPreviewMaterial_Default();

	/** Spawn post processing volume actor if the material has post processing as domain. */
	void ApplyPreviewMaterial_PostProcess();

	void SetShowPreviewBackground(bool bInShowBackground);

	void TogglePreviewBackground();

	ECheckBoxState IsPreviewBackgroundEnabled() const;

	void OnPropertyChanged(UObject* InObjectBeingModified, FPropertyChangedEvent& InPropertyChangedEvent);

	void OnFeatureLevelChanged(ERHIFeatureLevel::Type InNewFeatureLevel);

	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	TSharedRef<SWidget> GenerateToolbarMenu();

	void OnEditorSettingsChanged(const FPropertyChangedEvent& InPropertyChangedEvent);

	//~ Begin SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	//~ End SEditorViewport
};
