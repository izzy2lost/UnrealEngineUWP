// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChimeraAssetEditor.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "AssetEditorModeManager.h"
#include "Chimera/ChimeraAsset.h"
#include "ChimeraEditorDefines.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Modules/ModuleManager.h"
#include "PreviewProfileController.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "SSimpleTimeSlider.h"
#include "UnrealWidget.h"
#include "Viewports.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "ChimeraAssetEditor"

namespace UE::Chimera
{

constexpr float StepDeltaTime = 1.f / 30.f;

/////////////////////////////////////////////////
// class FChimeraAssetEdMode
bool FChimeraAssetPreviewActor::SpawnPreviewActor(UWorld* World, const UChimeraAsset* ChimeraAsset, const UE::PoseSearch::FRole& Role)
{
	UAnimationAsset* PreviewAsset = ChimeraAsset->GetAnimationAsset(Role);
	if (!PreviewAsset)
	{
		return false;
	}

	ActorRole = Role;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 ActorIndex = 0; ActorIndex < NumActors; ++ActorIndex)
	{
		UE::PoseSearch::FAnimationAssetSampler& Sampler = Samplers[ActorIndex];

		const FTransform Origin = ChimeraAsset->GetDebugWarpOrigin(GetRole(), ActorIndex == DebugActor);
		Sampler.Init(PreviewAsset, Origin, BlendParameters);

		const FTransform ActorTransform = Sampler.ExtractRootTransform(CurrentTime);

		TWeakObjectPtr<AActor>& ActorPtr = ActorPtrs[ActorIndex];

		ActorPtr = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		ActorPtr->SetFlags(RF_Transient);

		UDebugSkelMeshComponent* Mesh = NewObject<UDebugSkelMeshComponent>(ActorPtr.Get());
		Mesh->RegisterComponentWithWorld(World);

		UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(Mesh);
		Mesh->PreviewInstance = AnimInstance;
		AnimInstance->InitializeAnimation();

		USkeleton* Skeleton = PreviewAsset->GetSkeleton();
		Mesh->SetSkeletalMesh(Skeleton->GetPreviewMesh(true));
		Mesh->EnablePreview(true, PreviewAsset);

		AnimInstance->SetAnimationAsset(PreviewAsset, ChimeraAsset->IsLooping(), 0.f);
		AnimInstance->SetBlendSpacePosition(BlendParameters);

		AnimInstance->PlayAnim(ChimeraAsset->IsLooping(), 0.f);
		if (!ActorPtr->GetRootComponent())
		{
			ActorPtr->SetRootComponent(Mesh);
		}

		AnimInstance->SetPosition(Sampler.ToNormalizedTime(CurrentTime));
		AnimInstance->SetPlayRate(0.f);
		AnimInstance->SetBlendSpacePosition(BlendParameters);

		ActorPtr->SetActorTransform(ActorTransform);

		UE_LOG(LogChimeraEditor, Log, TEXT("Spawned preview Actor: %s"), *GetNameSafe(ActorPtr.Get()));
	}
	return true;
}

void FChimeraAssetPreviewActor::UpdatePreviewActor(const UChimeraAsset* ChimeraAsset, float PlayTime)
{
	if (UAnimationAsset* PreviewAsset = ChimeraAsset->GetAnimationAsset(GetRole()))
	{
		bool bPlayTimeUpdated = false;

		float NewCurrentTime = 0.f;
		FAnimationRuntime::AdvanceTime(false, PlayTime, NewCurrentTime, PreviewAsset->GetPlayLength());

		if (!FMath::IsNearlyEqual(CurrentTime, NewCurrentTime))
		{
			CurrentTime = NewCurrentTime;
			bPlayTimeUpdated = true;
		}

		for (int32 ActorIndex = 0; ActorIndex < NumActors; ++ActorIndex)
		{
			const FTransform Origin = ChimeraAsset->GetDebugWarpOrigin(GetRole(), ActorIndex == DebugActor);

			bool bSamplerReinitialized = false;
			UE::PoseSearch::FAnimationAssetSampler& Sampler = Samplers[ActorIndex];
			if (PreviewAsset != Sampler.GetAsset() || !Origin.Equals(Sampler.GetRootTransformOrigin()))
			{
				// reinitializing the Sampler if the PreviewAsset or the origin transform changed
				Sampler.Init(PreviewAsset, Origin, BlendParameters);
				bSamplerReinitialized = true;
			}

			TWeakObjectPtr<AActor>& ActorPtr = ActorPtrs[ActorIndex];
			if (ActorPtr != nullptr)
			{
				if (UDebugSkelMeshComponent* Mesh = Cast<UDebugSkelMeshComponent>(ActorPtr->GetRootComponent()))
				{
					if (UAnimPreviewInstance* AnimInstance = Mesh->PreviewInstance.Get())
					{
						bool bPreviewAssetChanged = false;
						if (AnimInstance->GetAnimationAsset() != PreviewAsset)
						{
							AnimInstance->SetAnimationAsset(PreviewAsset, ChimeraAsset->IsLooping(), 0.f);
							bPreviewAssetChanged = true;
						}

						if (bPlayTimeUpdated || bSamplerReinitialized || bPreviewAssetChanged)
						{
							// SetPosition is in [0..1] range for blendspaces
							AnimInstance->SetPosition(Sampler.ToNormalizedTime(CurrentTime));
							AnimInstance->SetPlayRate(0.f);
							AnimInstance->SetBlendSpacePosition(BlendParameters);

							const FTransform ActorTransform = Sampler.ExtractRootTransform(CurrentTime);
							ActorPtr->SetActorTransform(ActorTransform);
						}
					}
				}
			}
		}
	}
}

void FChimeraAssetPreviewActor::Destroy()
{
	for (TWeakObjectPtr<AActor>& ActorPtr : ActorPtrs)
	{
		if (ActorPtr != nullptr)
		{
			ActorPtr->Destroy();
		}
		ActorPtr = nullptr;
	}
}


UAnimPreviewInstance* FChimeraAssetPreviewActor::GetAnimPreviewInstance()
{
	if (const AActor* Actor = ActorPtrs[DebugActor].Get())
	{
		if (const UDebugSkelMeshComponent* Mesh = Cast<UDebugSkelMeshComponent>(Actor->GetRootComponent()))
		{
			return Mesh->PreviewInstance.Get();
		}
	}
	
	return nullptr;
}

FTransform FChimeraAssetPreviewActor::GetDebugActorTransformFromSampler() const
{
	const UE::PoseSearch::FAnimationAssetSampler& Sampler = Samplers[DebugActor];
	const FTransform ActorTransform = Sampler.ExtractRootTransform(CurrentTime);
	return ActorTransform;
}

void FChimeraAssetPreviewActor::ForceDebugActorTransform(const FTransform& ActorTransform)
{
	TWeakObjectPtr<AActor>& ActorPtr = ActorPtrs[DebugActor];
	if (ActorPtr != nullptr)
	{
		ActorPtr->SetActorTransform(ActorTransform);
	}
}

/////////////////////////////////////////////////
// class FChimeraAssetViewModel
void FChimeraAssetViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ChimeraAssetPtr);
}

void FChimeraAssetViewModel::Initialize(UChimeraAsset* ChimeraAsset, const TSharedRef<FChimeraAssetPreviewScene>& PreviewScene)
{
	ChimeraAssetPtr = ChimeraAsset;
	PreviewScenePtr = PreviewScene;
}

void FChimeraAssetViewModel::PreviewBackwardEnd()
{
	SetPlayTime(0.f, false);
}

void FChimeraAssetViewModel::PreviewBackwardStep()
{
	if (const UChimeraAsset* ChimeraAsset = GetChimeraAsset())
	{
		const float NewPlayTime = FMath::Clamp(PlayTime - StepDeltaTime, 0.f, ChimeraAsset->GetPlayLength());
		SetPlayTime(NewPlayTime, false);
	}
}

void FChimeraAssetViewModel::PreviewBackward()
{
	DeltaTimeMultiplier = -1.f;
}

void FChimeraAssetViewModel::PreviewPause()
{
	DeltaTimeMultiplier = 0.f;
}

void FChimeraAssetViewModel::PreviewForward()
{
	DeltaTimeMultiplier = 1.f;
}

void FChimeraAssetViewModel::PreviewForwardStep()
{
	if (const UChimeraAsset* ChimeraAsset = GetChimeraAsset())
	{
		const float NewPlayTime = FMath::Clamp(PlayTime + StepDeltaTime, 0.f, ChimeraAsset->GetPlayLength());
		SetPlayTime(NewPlayTime, false);
	}
}

void FChimeraAssetViewModel::PreviewForwardEnd()
{
	if (const UChimeraAsset* ChimeraAsset = GetChimeraAsset())
	{
		SetPlayTime(ChimeraAsset->GetPlayLength(), false);
	}
}

UWorld* FChimeraAssetViewModel::GetWorld()
{
	check(PreviewScenePtr.IsValid());
	return PreviewScenePtr.Pin()->GetWorld();
}

void FChimeraAssetViewModel::Tick(float DeltaSeconds)
{
	const UChimeraAsset* ChimeraAsset = GetChimeraAsset();
	if (!ChimeraAsset)
	{
		RemovePreviewActors();
		return;
	}

	PlayTime += DeltaSeconds * DeltaTimeMultiplier;
	PlayTime = FMath::Clamp(PlayTime, 0.f, ChimeraAsset->GetPlayLength());

	UE::PoseSearch::FRoleToIndex ChimeraAssetRoleToIndex;
	for (int32 RoleIndex = 0; RoleIndex < ChimeraAsset->GetNumRoles(); ++RoleIndex)
	{
		ChimeraAssetRoleToIndex.Add(ChimeraAsset->GetRole(RoleIndex)) = RoleIndex;
	}

	// iterating backwards because of the possible RemoveAtSwap 
	UE::PoseSearch::FRoleToIndex PreviewActorsRoleToIndex;
	for (int32 ActorIndex = PreviewActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		FChimeraAssetPreviewActor& PreviewActor = PreviewActors[ActorIndex];
		if (!ChimeraAssetRoleToIndex.Find(PreviewActor.GetRole()))
		{
			PreviewActor.Destroy();
			PreviewActors.RemoveAtSwap(ActorIndex, EAllowShrinking::No);
		}
		else
		{
			PreviewActorsRoleToIndex.Add(PreviewActor.GetRole());		
		}
	}

	if (PreviewActors.Num() != ChimeraAssetRoleToIndex.Num())
	{
		for (int32 RoleIndex = 0; RoleIndex < ChimeraAsset->GetNumRoles(); ++RoleIndex)
		{
			if (!PreviewActorsRoleToIndex.Find(ChimeraAsset->GetRole(RoleIndex)))
			{
				FChimeraAssetPreviewActor PreviewActor;
				if (PreviewActor.SpawnPreviewActor(GetWorld(), ChimeraAsset, ChimeraAsset->GetRole(RoleIndex)))
				{
					PreviewActors.Add(PreviewActor);
				}
			}
		}
	}

	for (FChimeraAssetPreviewActor& PreviewActor : PreviewActors)
	{
		PreviewActor.UpdatePreviewActor(ChimeraAsset, PlayTime);
	}
	
	UE::PoseSearch::FRoleToIndex PreviewActorRoleToIndex;
	PreviewActorRoleToIndex.Reserve(PreviewActors.Num());
	for (int32 PreviewActorIndex = 0; PreviewActorIndex < PreviewActors.Num(); ++PreviewActorIndex)
	{
		PreviewActorRoleToIndex.Add(PreviewActors[PreviewActorIndex].GetRole()) = PreviewActorIndex;
	}

	// testing CalculateWarpTransforms
#if WITH_EDITORONLY_DATA
	if (ChimeraAsset->bEnableDebugWarp && PreviewActors.Num() == ChimeraAsset->GetNumRoles())
	{
		const UWorld* DebugDrawWorld = nullptr;
		TArray<FTransform, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> ActorTransforms;
		ActorTransforms.SetNum(ChimeraAsset->GetNumRoles());
		for (int32 ChimeraAssetRoleIndex = 0; ChimeraAssetRoleIndex < ChimeraAsset->GetNumRoles(); ++ChimeraAssetRoleIndex)
		{
			const UE::PoseSearch::FRole& ChimeraAssetRole = ChimeraAsset->GetRole(ChimeraAssetRoleIndex);
			const int32 PreviewActorIndex = PreviewActorRoleToIndex[ChimeraAssetRole];

			FChimeraAssetPreviewActor& PreviewActor = PreviewActors[PreviewActorIndex];
			check(PreviewActor.GetRole() == ChimeraAssetRole);

			ActorTransforms[ChimeraAssetRoleIndex] = PreviewActor.GetDebugActorTransformFromSampler();
			if (!DebugDrawWorld)
			{
				if (UAnimPreviewInstance* AnimPreviewInstance = PreviewActor.GetAnimPreviewInstance())
				{
					DebugDrawWorld = AnimPreviewInstance->GetWorld();
				}
			}
		}

		TArray<FTransform, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> FullAlignedActorTransforms;
		FullAlignedActorTransforms.SetNum(ChimeraAsset->GetNumRoles());
		ChimeraAsset->CalculateWarpTransforms(PlayTime, ActorTransforms, FullAlignedActorTransforms, DebugDrawWorld);

		for (int32 ChimeraAssetRoleIndex = 0; ChimeraAssetRoleIndex < ChimeraAsset->GetNumRoles(); ++ChimeraAssetRoleIndex)
		{
			const UE::PoseSearch::FRole& ChimeraAssetRole = ChimeraAsset->GetRole(ChimeraAssetRoleIndex);
			const int32 PreviewActorIndex = PreviewActorRoleToIndex[ChimeraAssetRole];

			FChimeraAssetPreviewActor& PreviewActor = PreviewActors[PreviewActorIndex];
			check(PreviewActor.GetRole() == ChimeraAssetRole);
			FTransform DebugActorTransform;
			DebugActorTransform.Blend(ActorTransforms[ChimeraAssetRoleIndex], FullAlignedActorTransforms[ChimeraAssetRoleIndex], ChimeraAsset->DebugWarpAmount);
			PreviewActor.ForceDebugActorTransform(DebugActorTransform);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void FChimeraAssetViewModel::RemovePreviewActors()
{
	PlayTime = 0.f;
	DeltaTimeMultiplier = 1.f;

	for (FChimeraAssetPreviewActor& PreviewActor : PreviewActors)
	{
		PreviewActor.Destroy();
	}

	PreviewActors.Reset();
}

TRange<double> FChimeraAssetViewModel::GetPreviewPlayRange() const
{
	constexpr double ViewRangeSlack = 0.2;
	if (const UChimeraAsset* ChimeraAsset = GetChimeraAsset())
	{
		return TRange<double>(-ViewRangeSlack, ChimeraAsset->GetPlayLength() + ViewRangeSlack);
	}
	
	return TRange<double>(-ViewRangeSlack, ViewRangeSlack);
}

void FChimeraAssetViewModel::SetPlayTime(float NewPlayTime, bool bInTickPlayTime)
{
	if (const UChimeraAsset* ChimeraAsset = GetChimeraAsset())
	{
		NewPlayTime = FMath::Clamp(NewPlayTime, 0.f, ChimeraAsset->GetPlayLength());
		DeltaTimeMultiplier = bInTickPlayTime ? DeltaTimeMultiplier : 0.f;

		if (!FMath::IsNearlyEqual(PlayTime, NewPlayTime))
		{
			PlayTime = NewPlayTime;

			for (FChimeraAssetPreviewActor& PreviewActor : PreviewActors)
			{
				PreviewActor.UpdatePreviewActor(ChimeraAsset, PlayTime);
			}
		}
	}
}

void FChimeraAssetViewModel::SetPreviewProperties(float AnimAssetTime, const FVector& AnimAssetBlendParameters, bool bAnimAssetPlaying)
{
	// @todo: add support for blend spaces (pass AnimAssetBlendParameters as input)
	SetPlayTime(AnimAssetTime, bAnimAssetPlaying);
}

/////////////////////////////////////////////////
// class FChimeraAssetEdMode
const FEditorModeID FChimeraAssetEdMode::EdModeId = TEXT("ChimeraAssetEdMode");

void FChimeraAssetEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	if (FChimeraAssetViewportClient* ChimeraAssetViewportClient = static_cast<FChimeraAssetViewportClient*>(ViewportClient))
	{
		// ensure we redraw even if PIE is active
		ChimeraAssetViewportClient->Invalidate();

		if (!ViewModel)
		{
			ViewModel = ChimeraAssetViewportClient->GetAssetEditor()->GetViewModel();
		}
	}

	if (ViewModel)
	{
		ViewModel->Tick(DeltaTime);
	}
}

void FChimeraAssetEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
}

bool FChimeraAssetEdMode::AllowWidgetMove()
{
	return FEdMode::ShouldDrawWidget();
}

bool FChimeraAssetEdMode::ShouldDrawWidget() const
{
	return FEdMode::ShouldDrawWidget();
}

bool FChimeraAssetEdMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return FEdMode::GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

/////////////////////////////////////////////////
// class FChimeraAssetViewportClient
FChimeraAssetViewportClient::FChimeraAssetViewportClient(
	const TSharedRef<FChimeraAssetPreviewScene>& InPreviewScene,
	const TSharedRef<SChimeraAssetViewport>& InViewport,
	const TSharedRef<FChimeraAssetEditor>& InAssetEditor)
	: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewport))
	, PreviewScenePtr(InPreviewScene)
	, AssetEditorPtr(InAssetEditor)
{
	Widget->SetUsesEditorModeTools(ModeTools.Get());
	StaticCastSharedPtr<FAssetEditorModeManager>(ModeTools)->SetPreviewScene(&InPreviewScene.Get());
	ModeTools->SetDefaultMode(FChimeraAssetEdMode::EdModeId);

	SetRealtime(true);

	SetWidgetCoordSystemSpace(COORD_Local);
	ModeTools->SetWidgetMode(UE::Widget::WM_Translate);
}

void FChimeraAssetViewportClient::TrackingStarted(
	const struct FInputEventState& InInputState,
	bool bIsDraggingWidget,
	bool bNudge)
{
	ModeTools->StartTracking(this, Viewport);
}

void FChimeraAssetViewportClient::TrackingStopped()
{
	ModeTools->EndTracking(this, Viewport);
	Invalidate();
}

void FChimeraAssetViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
}

/////////////////////////////////////////////////
// class FChimeraAssetPreviewScene
FChimeraAssetPreviewScene::FChimeraAssetPreviewScene(ConstructionValues CVs, const TSharedRef<FChimeraAssetEditor>& Editor)
: FAdvancedPreviewScene(CVs)
, EditorPtr(Editor)
{
	// Disable killing actors outside of the world
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(true);
	WorldSettings->bEnableWorldBoundsChecks = false;

	// Spawn an owner for FloorMeshComponent so CharacterMovementComponent can detect it as a valid floor and slide 
	// along it
	{
		AActor* FloorActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform());
		check(FloorActor);

		static const FString NewName = FString(TEXT("FloorComponent"));
		FloorMeshComponent->Rename(*NewName, FloorActor);

		FloorActor->SetRootComponent(FloorMeshComponent);
	}
}

void FChimeraAssetPreviewScene::Tick(float InDeltaTime)
{
	FAdvancedPreviewScene::Tick(InDeltaTime);

	// Trigger Begin Play in this preview world.
	// This is needed for the CharacterMovementComponent to be able to switch to falling mode. 
	// See: UCharacterMovementComponent::StartFalling
	if (PreviewWorld && !PreviewWorld->GetBegunPlay())
	{
		for (FActorIterator It(PreviewWorld); It; ++It)
		{
			It->DispatchBeginPlay();
		}

		PreviewWorld->SetBegunPlay(true);
	}

	GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
}

/////////////////////////////////////////////////
// class SChimeraAssetViewport
void SChimeraAssetViewport::Construct(
	const FArguments& InArgs,
	const FChimeraAssetPreviewRequiredArgs& InRequiredArgs)
{
	PreviewScenePtr = InRequiredArgs.PreviewScene;
	AssetEditorPtr = InRequiredArgs.AssetEditor;

	SEditorViewport::Construct(
		SEditorViewport::FArguments()
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		.AddMetaData<FTagMetaData>(TEXT("AnimationTools.Viewport"))
	);
}

void SChimeraAssetViewport::BindCommands()
{
	SEditorViewport::BindCommands();
}

TSharedRef<FEditorViewportClient> SChimeraAssetViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FChimeraAssetViewportClient>(
		PreviewScenePtr.Pin().ToSharedRef(),
		SharedThis(this),
		AssetEditorPtr.Pin().ToSharedRef());
	ViewportClient->ViewportType = LVT_Perspective;
	ViewportClient->bSetListenerPosition = false;
	ViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	ViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SChimeraAssetViewport::MakeViewportToolbar()
{
	return SAssignNew(ViewportToolbar, SCommonEditorViewportToolbarBase, SharedThis(this));
}

TSharedRef<SEditorViewport> SChimeraAssetViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SChimeraAssetViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SChimeraAssetViewport::OnFloatingButtonClicked()
{
}

/////////////////////////////////////////////////
// class SChimeraAssetPreview
void SChimeraAssetPreview::Construct(const FArguments& InArgs, const FChimeraAssetPreviewRequiredArgs& InRequiredArgs)
{
	SliderColor = InArgs._SliderColor;
	SliderScrubTime = InArgs._SliderScrubTime;
	SliderViewRange = InArgs._SliderViewRange;
	OnSliderScrubPositionChanged = InArgs._OnSliderScrubPositionChanged;

	OnBackwardEnd = InArgs._OnBackwardEnd;
	OnBackwardStep = InArgs._OnBackwardStep;
	OnBackward = InArgs._OnBackward;
	OnPause = InArgs._OnPause;
	OnForward = InArgs._OnForward;
	OnForwardStep = InArgs._OnForwardStep;
	OnForwardEnd = InArgs._OnForwardEnd;

	FSlimHorizontalToolBarBuilder ToolBarBuilder(
		TSharedPtr<const FUICommandList>(), 
		FMultiBoxCustomization::None, 
		nullptr, true);

	auto AddToolBarButton = [&ToolBarBuilder](FName ButtonImageName, FOnButtonClickedEvent& OnClicked)
		{
			ToolBarBuilder.AddToolBarWidget(
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked_Lambda([&OnClicked]()
					{
						if (OnClicked.IsBound())
						{
							OnClicked.Execute();
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
				[
					SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Image(FAppStyle::Get().GetBrush(ButtonImageName))
				]);
		};

	//ToolBarBuilder.SetStyle(&FAppStyle::Get(), "PaletteToolBar");
	ToolBarBuilder.BeginSection("Preview");
	{
		AddToolBarButton("Animation.Backward_End", OnBackwardEnd);
		AddToolBarButton("Animation.Backward_Step", OnBackwardStep);
		AddToolBarButton("Animation.Backward", OnBackward);
		AddToolBarButton("Animation.Pause", OnPause);
		AddToolBarButton("Animation.Forward", OnForward);
		AddToolBarButton("Animation.Forward_Step", OnForwardStep);
		AddToolBarButton("Animation.Forward_End", OnForwardEnd);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SChimeraAssetViewport, InRequiredArgs)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				ToolBarBuilder.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSimpleTimeSlider)
				.ClampRangeHighlightSize(0.15f)
				.ClampRangeHighlightColor_Lambda([this]()
					{
						return SliderColor.Get();
					})
				.ScrubPosition_Lambda([this]()
					{
						return SliderScrubTime.Get();
					})
				.ViewRange_Lambda([this]()
					{
						return SliderViewRange.Get();
					})
				.ClampRange_Lambda([this]()
					{
						return SliderViewRange.Get();
					})
				.OnScrubPositionChanged_Lambda([this](double NewScrubTime, bool bIsScrubbing)
					{
						if (bIsScrubbing)
						{
							OnSliderScrubPositionChanged.ExecuteIfBound(NewScrubTime, bIsScrubbing);
						}
					})
			]
		]
	];
}

/////////////////////////////////////////////////
// class FChimeraAssetEditor
const FName ChimeraAssetEditorAppName = FName(TEXT("ChimeraAssetEditorApp"));

// Tab identifiers
struct FChimeraAssetEditorTabs
{
	static const FName AssetDetailsID;
	static const FName ViewportID;
};
const FName FChimeraAssetEditorTabs::AssetDetailsID(TEXT("ChimeraAssetEditorAssetDetailsTabID"));
const FName FChimeraAssetEditorTabs::ViewportID(TEXT("ChimeraAssetEditorViewportTabID"));

void FChimeraAssetEditor::InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UChimeraAsset* ChimeraAsset)
{
	// Create Preview Scene
	if (!PreviewScene.IsValid())
	{
		PreviewScene = MakeShareable(
				new FChimeraAssetPreviewScene(
					FPreviewScene::ConstructionValues()
					.SetCreatePhysicsScene(false)
					.SetTransactional(false)
					.ForceUseMovementComponentInNonGameWorld(true),
					StaticCastSharedRef<FChimeraAssetEditor>(AsShared())));

		//Temporary fix for missing attached assets - MDW (Copied from FPersonaToolkit::CreatePreviewScene)
		PreviewScene->GetWorld()->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);
	}

	// Create view model
	ViewModel = MakeShared<FChimeraAssetViewModel>();
	ViewModel->Initialize(ChimeraAsset, PreviewScene.ToSharedRef());

	// Create viewport widget
	{
		FChimeraAssetPreviewRequiredArgs PreviewArgs(
			StaticCastSharedRef<FChimeraAssetEditor>(AsShared()),
			PreviewScene.ToSharedRef());
			
		PreviewWidget = SNew(SChimeraAssetPreview, PreviewArgs)
			.SliderColor(FLinearColor::Red)
			.SliderScrubTime_Lambda([this]()
				{
					return ViewModel->GetPlayTime();
				})
			.SliderViewRange_Lambda([this]() 
				{ 
					return ViewModel->GetPreviewPlayRange();
				})
			.OnSliderScrubPositionChanged_Lambda([this](float NewScrubPosition, bool bScrubbing)
				{
					ViewModel->SetPlayTime(NewScrubPosition, !bScrubbing);
				})
			.OnBackwardEnd_Raw(this, &FChimeraAssetEditor::PreviewBackwardEnd)
			.OnBackwardStep_Raw(this, &FChimeraAssetEditor::PreviewBackwardStep)
			.OnBackward_Raw(this, &FChimeraAssetEditor::PreviewBackward)
			.OnPause_Raw(this, &FChimeraAssetEditor::PreviewPause)
			.OnForward_Raw(this, &FChimeraAssetEditor::PreviewForward)
			.OnForwardStep_Raw(this, &FChimeraAssetEditor::PreviewForwardStep)
			.OnForwardEnd_Raw(this, &FChimeraAssetEditor::PreviewForwardEnd);
	}

	// asset details widget
	FDetailsViewArgs AssetDetailsArgs;
	AssetDetailsArgs.bHideSelectionTip = true;
	AssetDetailsArgs.NotifyHook = this;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	EditingAssetWidget = PropertyModule.CreateDetailView(AssetDetailsArgs);
	EditingAssetWidget->SetObject(ChimeraAsset);

	// Define Editor Layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
	FTabManager::NewLayout("Standalone_ChimeraAssetDatabaseEditor_Layout_v0.02")
		->AddArea
		(
			// Main application area
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(FChimeraAssetEditorTabs::AssetDetailsID, ETabState::OpenedTab)
					->SetHideTabWell(false)
			)
			->Split
			(
				FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(FChimeraAssetEditorTabs::ViewportID, ETabState::OpenedTab)
					->SetHideTabWell(false)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		ChimeraAssetEditorAppName,
		StandaloneDefaultLayout,
		true,
		true,
		ChimeraAsset,
		false);

	RegenerateMenusAndToolbars();
}

void FChimeraAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_ChimeraAssetEditor", "Chimera Asset Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(
		FChimeraAssetEditorTabs::ViewportID,
		FOnSpawnTab::CreateSP(this, &FChimeraAssetEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
			
	InTabManager->RegisterTabSpawner(
		FChimeraAssetEditorTabs::AssetDetailsID,
		FOnSpawnTab::CreateSP(this, &FChimeraAssetEditor::SpawnTab_AssetDetails))
		.SetDisplayName(LOCTEXT("ChimeraAssetDetailsTab", "Chimera Asset Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FChimeraAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FChimeraAssetEditorTabs::ViewportID);
	InTabManager->UnregisterTabSpawner(FChimeraAssetEditorTabs::AssetDetailsID);
}

FName FChimeraAssetEditor::GetToolkitFName() const
{
	return FName("ChimeraAssetEditor");
}

FText FChimeraAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("ChimeraAssetEditorAppLabel", "Chimera Asset Editor");
}

FText FChimeraAssetEditor::GetToolkitName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(GetNameSafe(GetChimeraAsset())));
	return FText::Format(LOCTEXT("ChimeraAssetEditorToolkitName", "{AssetName}"), Args);
}

FLinearColor FChimeraAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FChimeraAssetEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("ChimeraAssetEditor");
}

void FChimeraAssetEditor::SetPreviewProperties(float AnimAssetTime, const FVector& AnimAssetBlendParameters, bool bAnimAssetPlaying)
{
	ViewModel->SetPreviewProperties(AnimAssetTime, AnimAssetBlendParameters, bAnimAssetPlaying);
}

TSharedRef<SDockTab> FChimeraAssetEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FChimeraAssetEditorTabs::ViewportID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Label(LOCTEXT("ViewportTab_Title", "Viewport"));

	if (PreviewWidget.IsValid())
	{
		SpawnedTab->SetContent(PreviewWidget.ToSharedRef());
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FChimeraAssetEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FChimeraAssetEditorTabs::AssetDetailsID);

	return SNew(SDockTab)
		.Label(LOCTEXT("ChimeraAsset_Details_Title", "Chimera Asset Details"))
		[
			EditingAssetWidget.ToSharedRef()
		];
}

} // namespace UE::Chimera

#undef LOCTEXT_NAMESPACE
