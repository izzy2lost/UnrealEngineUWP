// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedPreviewScene.h"
#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "EdMode.h"
#include "IMultiAnimAssetEditor.h"
#include "Misc/NotifyHook.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchRole.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"

class UAnimPreviewInstance;
class UChimeraAsset;
class UDebugSkelMeshComponent;

namespace UE::Chimera
{

class FChimeraAssetPreviewScene;
class FChimeraAssetEditor;
class SChimeraAssetPreview;
class SChimeraAssetViewport;

/////////////////////////////////////////////////
// class struct FChimeraAssetPreviewActor
struct FChimeraAssetPreviewActor
{
public:
	bool SpawnPreviewActor(UWorld* World, const UChimeraAsset* ChimeraAsset, const UE::PoseSearch::FRole& Role);
	void UpdatePreviewActor(const UChimeraAsset* ChimeraAsset, float PlayTime);
	void Destroy();
	UAnimPreviewInstance* GetAnimPreviewInstance();
	const UE::PoseSearch::FRole& GetRole() const { return ActorRole; }
	FTransform GetDebugActorTransformFromSampler() const;
	void ForceDebugActorTransform(const FTransform& ActorTransform);

private:
	enum { PreviewActor, DebugActor, NumActors };
	TStaticArray<TWeakObjectPtr<AActor>, NumActors> ActorPtrs;
	TStaticArray<UE::PoseSearch::FAnimationAssetSampler, NumActors> Samplers; // used to keep track of the animation position / orientation

	float CurrentTime = 0.f;
	UE::PoseSearch::FRole ActorRole = UE::PoseSearch::DefaultRole;
	
	// @todo: add support for blend spaces
	FVector BlendParameters = FVector::ZeroVector;
};

/////////////////////////////////////////////////
// class FChimeraAssetViewModel
class FChimeraAssetViewModel : public TSharedFromThis<FChimeraAssetViewModel>, public FGCObject
{
public:
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FChimeraAssetViewModel"); }
	void Initialize(UChimeraAsset* ChimeraAsset, const TSharedRef<FChimeraAssetPreviewScene>& PreviewScene);
	void RemovePreviewActors();
	void PreviewBackwardEnd();
	void PreviewBackwardStep();
	void PreviewBackward();
	void PreviewPause();
	void PreviewForward();
	void PreviewForwardStep();
	void PreviewForwardEnd();
	const UChimeraAsset* GetChimeraAsset() const { return ChimeraAssetPtr.Get(); }
	void Tick(float DeltaSeconds);
	const TArray<FChimeraAssetPreviewActor>& GetPreviewActors() const { return PreviewActors; }
	TArray<FChimeraAssetPreviewActor>& GetPreviewActors() { return PreviewActors; }
	TRange<double> GetPreviewPlayRange() const;
	void SetPlayTime(float NewPlayTime, bool bInTickPlayTime);
	float GetPlayTime() const { return PlayTime; }
	void SetPreviewProperties(float AnimAssetTime, const FVector& AnimAssetBlendParameters, bool bAnimAssetPlaying);

private:
	UWorld* GetWorld();

	float PlayTime = 0.f;
	float DeltaTimeMultiplier = 1.f;
	/** asset being viewed and edited by this view model. */
	TWeakObjectPtr<UChimeraAsset> ChimeraAssetPtr;
	/** Weak pointer to the PreviewScene */
	TWeakPtr<FChimeraAssetPreviewScene> PreviewScenePtr;
	/** Actors to be displayed in the preview viewport */
	TArray<FChimeraAssetPreviewActor> PreviewActors;
};

/////////////////////////////////////////////////
// class FChimeraAssetEdMode
class FChimeraAssetEdMode : public FEdMode
{
public:
	const static FEditorModeID EdModeId;

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool AllowWidgetMove() override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override;

private:
	FChimeraAssetViewModel* ViewModel = nullptr;
};

/////////////////////////////////////////////////
// class FChimeraAssetViewportClient
class FChimeraAssetViewportClient : public FEditorViewportClient
{
public:
	FChimeraAssetViewportClient(const TSharedRef<FChimeraAssetPreviewScene>& InPreviewScene, const TSharedRef<SChimeraAssetViewport>& InViewport, const TSharedRef<FChimeraAssetEditor>& InAssetEditor);
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void TrackingStarted(const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge) override;
	virtual void TrackingStopped() override;

	/** Get the preview scene we are viewing */
	TSharedRef<FChimeraAssetPreviewScene> GetPreviewScene() const { return PreviewScenePtr.Pin().ToSharedRef(); }
	TSharedRef<FChimeraAssetEditor> GetAssetEditor() const { return AssetEditorPtr.Pin().ToSharedRef();	}

private:
	/** Preview scene we are viewing */
	TWeakPtr<FChimeraAssetPreviewScene> PreviewScenePtr;
	/** Asset editor we are embedded in */
	TWeakPtr<FChimeraAssetEditor> AssetEditorPtr;
};

/////////////////////////////////////////////////
// class FChimeraAssetPreviewScene
class FChimeraAssetPreviewScene : public FAdvancedPreviewScene
{
public:
	FChimeraAssetPreviewScene(ConstructionValues CVs, const TSharedRef<FChimeraAssetEditor>& Editor);
	virtual void Tick(float InDeltaTime) override;
	TSharedRef<FChimeraAssetEditor> GetEditor() const { return EditorPtr.Pin().ToSharedRef(); }

private:
	/** The asset editor we are embedded in */
	TWeakPtr<FChimeraAssetEditor> EditorPtr;
};

/////////////////////////////////////////////////
// struct FChimeraAssetPreviewRequiredArgs
struct FChimeraAssetPreviewRequiredArgs
{
	FChimeraAssetPreviewRequiredArgs(const TSharedRef<FChimeraAssetEditor>& InAssetEditor, const TSharedRef<FChimeraAssetPreviewScene>& InPreviewScene)
		: AssetEditor(InAssetEditor)
		, PreviewScene(InPreviewScene)
	{
	}

	TSharedRef<FChimeraAssetEditor> AssetEditor;
	TSharedRef<FChimeraAssetPreviewScene> PreviewScene;
};

/////////////////////////////////////////////////
// class SChimeraAssetViewport
class SChimeraAssetViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SChimeraAssetViewport) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const FChimeraAssetPreviewRequiredArgs& InRequiredArgs);
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

protected:
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

	/** The viewport toolbar */
	TSharedPtr<SCommonEditorViewportToolbarBase> ViewportToolbar;
	/** Viewport client */
	TSharedPtr<FChimeraAssetViewportClient> ViewportClient;
	/** The preview scene that we are viewing */
	TWeakPtr<FChimeraAssetPreviewScene> PreviewScenePtr;
	/** Asset editor we are embedded in */
	TWeakPtr<FChimeraAssetEditor> AssetEditorPtr;
};

/////////////////////////////////////////////////
// class SChimeraAssetPreview
class SChimeraAssetPreview : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnScrubPositionChanged, double, bool)
	DECLARE_DELEGATE(FOnButtonClickedEvent)

	SLATE_BEGIN_ARGS(SChimeraAssetPreview) {}
		SLATE_ATTRIBUTE(FLinearColor, SliderColor);
		SLATE_ATTRIBUTE(double, SliderScrubTime);
		SLATE_ATTRIBUTE(TRange<double>, SliderViewRange);
		SLATE_EVENT(FOnScrubPositionChanged, OnSliderScrubPositionChanged);
		SLATE_EVENT(FOnButtonClickedEvent, OnBackwardEnd);
		SLATE_EVENT(FOnButtonClickedEvent, OnBackwardStep);
		SLATE_EVENT(FOnButtonClickedEvent, OnBackward);
		SLATE_EVENT(FOnButtonClickedEvent, OnPause);
		SLATE_EVENT(FOnButtonClickedEvent, OnForward);
		SLATE_EVENT(FOnButtonClickedEvent, OnForwardStep);
		SLATE_EVENT(FOnButtonClickedEvent, OnForwardEnd);
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const FChimeraAssetPreviewRequiredArgs& InRequiredArgs);

protected:
	TAttribute<FLinearColor> SliderColor;
	TAttribute<double> SliderScrubTime;
	TAttribute<TRange<double>> SliderViewRange = TRange<double>(0.0, 1.0);
	FOnScrubPositionChanged OnSliderScrubPositionChanged;
	FOnButtonClickedEvent OnBackwardEnd;
	FOnButtonClickedEvent OnBackwardStep;
	FOnButtonClickedEvent OnBackward;
	FOnButtonClickedEvent OnPause;
	FOnButtonClickedEvent OnForward;
	FOnButtonClickedEvent OnForwardStep;
	FOnButtonClickedEvent OnForwardEnd;
};

/////////////////////////////////////////////////
// class FChimeraAssetEditor
class FChimeraAssetEditor : public IMultiAnimAssetEditor, public FNotifyHook
{
public:

	void InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UChimeraAsset* ChimeraAsset);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void SetPreviewProperties(float AnimAssetTime, const FVector& AnimAssetBlendParameters, bool bAnimAssetPlaying) override;

	const UChimeraAsset* GetChimeraAsset() const { return ViewModel.IsValid() ? ViewModel->GetChimeraAsset() : nullptr; }
	FChimeraAssetViewModel* GetViewModel() const { return ViewModel.Get(); }
	void PreviewBackwardEnd() { ViewModel->PreviewBackwardEnd(); }
	void PreviewBackwardStep() { ViewModel->PreviewBackwardStep();	}
	void PreviewBackward() { ViewModel->PreviewBackward(); }
	void PreviewPause() { ViewModel->PreviewPause(); }
	void PreviewForward() { ViewModel->PreviewForward(); }
	void PreviewForwardStep() { ViewModel->PreviewForwardStep(); }
	void PreviewForwardEnd() { ViewModel->PreviewForwardEnd(); }

private:

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);

	TSharedPtr<SChimeraAssetPreview> PreviewWidget;
	TSharedPtr<IDetailsView> EditingAssetWidget;
	TSharedPtr<FChimeraAssetPreviewScene> PreviewScene;
	TSharedPtr<FChimeraAssetViewModel> ViewModel;
};

} // UE::Chimera

