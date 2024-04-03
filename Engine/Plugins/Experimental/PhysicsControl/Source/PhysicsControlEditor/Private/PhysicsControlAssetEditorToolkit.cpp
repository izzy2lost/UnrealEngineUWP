// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetEditorToolkit.h"

#include "AnimationEditorPreviewActor.h"
#include "EditorModeManager.h"
#include "IDetailsView.h"
#include "IPersonaToolkit.h"
#include "Modules/ModuleManager.h"
#include "PersonaModule.h"
#include "PhysicsControlAssetApplicationMode.h"
#include "PhysicsControlAsset.h"
#include "PhysicsControlAssetEditorMode.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "PhysicsControlAssetEditorData.h"

#define LOCTEXT_NAMESPACE "PhysicsControlAssetEditorToolkit"

const FName PhysicsControlAssetEditorModes::Editor("PhysicsControlAssetEditorMode");
const FName PhysicsControlAssetEditorAppName = FName(TEXT("PhysicsControlAssetEditorApp"));

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::InitAssetEditor(
	const EToolkitMode::Type        Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UPhysicsControlAsset*    InPhysicsControlAsset)
{
	bIsInitialized = false;

	// Initialise EditorData
	{
		EditorData = MakeShared<FPhysicsControlAssetEditorData>();
		EditorData->PhysicsControlAsset = InPhysicsControlAsset;
		EditorData->CachePreviewMesh();
	}

	// Create Persona toolkit
	{
		FPersonaToolkitArgs PersonaToolkitArgs;
		PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(
			this, &FPhysicsControlAssetEditorToolkit::HandlePreviewSceneCreated);
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		PersonaToolkit = PersonaModule.CreatePersonaToolkit(InPhysicsControlAsset, PersonaToolkitArgs);
		PersonaModule.RecordAssetOpened(FAssetData(InPhysicsControlAsset));
	}

	// Note - we might want to make a custom skeleton tree view here, based on showing either the
	// animation or the physics bones/skeleton. See FPhysicsAssetEditor::InitPhysicsAssetEditor

	GEditor->RegisterForUndo(this);

	// Initialise the asset editor
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		PhysicsControlAssetEditorAppName,
		FTabManager::FLayout::NullLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		InPhysicsControlAsset);

	// Create and set the application mode.
	ApplicationMode = new FPhysicsControlAssetApplicationMode(SharedThis(this), PersonaToolkit->GetPreviewScene());
	AddApplicationMode(PhysicsControlAssetEditorModes::Editor, MakeShareable(ApplicationMode));
	SetCurrentMode(PhysicsControlAssetEditorModes::Editor);

	// Activate the editor mode.
	GetEditorModeManager().SetDefaultMode(FPhysicsControlAssetEditorMode::ModeName);
	GetEditorModeManager().ActivateMode(FPhysicsControlAssetEditorMode::ModeName);

	FPhysicsControlAssetEditorMode* EditorMode = GetEditorModeManager().
		GetActiveModeTyped<FPhysicsControlAssetEditorMode>(FPhysicsControlAssetEditorMode::ModeName);
	EditorMode->SetEditorToolkit(this);

	bIsInitialized = true;
}


//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MLDeformerEditor", "ML Deformer Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

//======================================================================================================================
FName FPhysicsControlAssetEditorToolkit::GetToolkitFName() const
{
	return FName("PhysicsControlAssetEditor");
}

//======================================================================================================================
FText FPhysicsControlAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("PhysicsControlAssetEditorAppLabel", "Physics Control Asset Editor");
}

//======================================================================================================================
FText FPhysicsControlAssetEditorToolkit::GetToolkitName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(EditorData->PhysicsControlAsset->GetName()));
	return FText::Format(LOCTEXT("PhysicsControlAssetEditorToolkitName", "{AssetName}"), Args);
}

//======================================================================================================================
FLinearColor FPhysicsControlAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

//======================================================================================================================
FString FPhysicsControlAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("PhysicsControlAssetEditor");
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditorData->PhysicsControlAsset);
}

//======================================================================================================================
TStatId FPhysicsControlAssetEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysicsControlAssetEditorToolkit, STATGROUP_Tickables);
}

//======================================================================================================================
TSharedRef<IPersonaToolkit> FPhysicsControlAssetEditorToolkit::GetPersonaToolkit() const
{
	return PersonaToolkit.ToSharedRef();
}

//======================================================================================================================
// For inspiration here, see:
// - PhysicsAssetEditor
// - MLDeformerEditorToolkit
// - IKRigToolkit
// though note that they all do things differently!
void FPhysicsControlAssetEditorToolkit::HandlePreviewSceneCreated(
	const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	EditorData->PreviewScene = InPersonaPreviewScene;

	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(
		AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);

	// Create the preview skeletal mesh component
	ViewportSkeletalMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
	// turn off default bone rendering
	//ViewportSkeletalMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;

	// Setup and apply an anim instance to the skeletal mesh component
	ViewportAnimInstance = NewObject<UAnimPreviewInstance>(
		ViewportSkeletalMeshComponent, TEXT("PhysicsControlAssetEditorAnimInstance"));
	ViewportSkeletalMeshComponent->PreviewInstance = ViewportAnimInstance;
	//ViewportAnimInstance->InitializeAnimation();

	// Set the skeletal mesh on the component, using the asset. Note that this will change if/when
	// the asset doesn't hold a mesh.
	USkeletalMesh* Mesh = EditorData->PhysicsControlAsset->PreviewSkeletalMesh.Get();
	ViewportSkeletalMeshComponent->SetSkeletalMesh(Mesh);

	// apply mesh to the preview scene
	InPersonaPreviewScene->SetPreviewMeshComponent(ViewportSkeletalMeshComponent);
	InPersonaPreviewScene->SetAllowMeshHitProxies(false);
	InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);
	ViewportSkeletalMeshComponent->bSelectable = false;
	InPersonaPreviewScene->SetPreviewMesh(Mesh);
	InPersonaPreviewScene->AddComponent(ViewportSkeletalMeshComponent, FTransform::Identity);
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::HandleViewportCreated(const TSharedRef<IPersonaViewport>& InPersonaViewport)
{
	PersonaViewport = InPersonaViewport;
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::ShowEmptyDetails() const
{
	DetailsView->SetObject(EditorData->PhysicsControlAsset);
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
	DetailsView->OnFinishedChangingProperties().AddSP(
		this, &FPhysicsControlAssetEditorToolkit::OnFinishedChangingDetails);
	ShowEmptyDetails();
}

//======================================================================================================================
void FPhysicsControlAssetEditorToolkit::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const bool bPreviewMeshChanged = PropertyChangedEvent.GetPropertyName() == UPhysicsControlAsset::GetPreviewMeshPropertyName();
	if (bPreviewMeshChanged)
	{
		USkeletalMesh* Mesh = EditorData->PhysicsControlAsset->PreviewSkeletalMesh.LoadSynchronous();
		ViewportSkeletalMeshComponent->SetSkeletalMesh(Mesh);
		EditorData->CachePreviewMesh();
	}
}

#undef LOCTEXT_NAMESPACE

