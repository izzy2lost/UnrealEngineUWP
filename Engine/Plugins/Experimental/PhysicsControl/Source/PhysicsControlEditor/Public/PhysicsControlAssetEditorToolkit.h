// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "IHasPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "IPersonaViewport.h"
#include "PersonaAssetEditorToolkit.h"

class FPhysicsControlAssetApplicationMode;
class FPhysicsControlAssetEditorData;
class UAnimPreviewInstance;
class UPhysicsControlAsset;

namespace PhysicsControlAssetEditorModes
{
	extern const FName Editor;
}

/**
 * The main toolkit/editor for working with Physics Control Profile assets
 */
class PHYSICSCONTROLEDITOR_API FPhysicsControlAssetEditorToolkit :
	public FPersonaAssetEditorToolkit,
	public IHasPersonaToolkit,
	public FGCObject,
	public FEditorUndoClient,
	public FTickableEditorObject
{
public:
	friend class FPhysicsControlAssetApplicationMode;

public:

	/** Initialize the asset editor. This will register the application mode, init the preview scene, etc. */
	void InitAssetEditor(
		const EToolkitMode::Type        Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UPhysicsControlAsset*    InPhysicsControlAsset);

	// FAssetEditorToolkit overrides.
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	// ~END FAssetEditorToolkit overrides.

	// FGCObject overrides.
	virtual FString GetReferencerName() const override { return TEXT("FPhysicsControlAssetEditorToolkit"); }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// ~END FGCObject overrides.

	// FTickableEditorObject overrides.
	virtual void Tick(float DeltaTime) override {};
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	// ~END FTickableEditorObject overrides.

	// IHasPersonaToolkit overrides.
	virtual TSharedRef<IPersonaToolkit> GetPersonaToolkit() const override;
	// ~END IHasPersonaToolkit overrides.

	IPersonaToolkit* GetPersonaToolkitPointer() const { return PersonaToolkit.Get(); }

private:
	/** Preview scene setup. */
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
	void HandleViewportCreated(const TSharedRef<IPersonaViewport>& InPersonaViewport);
	void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);
	void OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent);
	void ShowEmptyDetails() const;

private:
	/** The persona toolkit. */
	TSharedPtr<IPersonaToolkit> PersonaToolkit = nullptr;

	// Persona viewport.
	TSharedPtr<IPersonaViewport> PersonaViewport = nullptr;

	/** Data and methods shared across multiple classes */
	TSharedPtr<FPhysicsControlAssetEditorData> EditorData;

	// Asset properties tab 
	TSharedPtr<IDetailsView> DetailsView;

	FPhysicsControlAssetApplicationMode* ApplicationMode = nullptr;

	// viewport anim instance 
	UPROPERTY(transient, NonTransactional)
	TObjectPtr<UAnimPreviewInstance> ViewportAnimInstance;

	// viewport skeletal mesh 
	UDebugSkelMeshComponent* ViewportSkeletalMeshComponent;

	/** Has the asset editor been initialized? */
	bool bIsInitialized = false;
};
