// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRootActor.h"

#include "Blueprints/DisplayClusterBlueprint.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Components/DisplayClusterOriginComponent.h"
#include "Components/DisplayClusterSceneComponentSyncParent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/DisplayClusterStageGeometryComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationStrings.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterProjectionStrings.h"
#include "IDisplayClusterConfiguration.h"

#include "DisplayClusterPlayerInput.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "Misc/TransactionObjectEvent.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Engine/TextureRenderTarget2D.h"

#include "Materials/Material.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "TextureResource.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IN-EDITOR STUFF
//////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR

#include "Async/Async.h"
#include "EditorSupportDelegates.h"
#include "LevelEditor.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// ADisplayClusterRootActor
//////////////////////////////////////////////////////////////////////////////////////////////

void ADisplayClusterRootActor::Constructor_Editor()
{
	// Allow tick in editor for preview rendering
	PrimaryActorTick.bStartWithTickEnabled = true;

	FCoreUObjectDelegates::OnPackageReloaded.AddUObject(this, &ADisplayClusterRootActor::HandleAssetReload);

	if (GEditor)
	{
		GEditor->OnEndObjectMovement().AddUObject(this, &ADisplayClusterRootActor::OnEndObjectMovement);
	}
}

void ADisplayClusterRootActor::Destructor_Editor()
{
	OnPreviewGenerated.Unbind();
	OnPreviewDestroyed.Unbind();

	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);

	if (GEditor)
	{
		GEditor->OnEndObjectMovement().RemoveAll(this);
	}
}

void ADisplayClusterRootActor::PostActorCreated_Editor()
{
	ResetEntireClusterPreviewRendering();
}

void ADisplayClusterRootActor::PostLoad_Editor()
{
	ResetEntireClusterPreviewRendering();
}

void ADisplayClusterRootActor::EndPlay_Editor(const EEndPlayReason::Type EndPlayReason)
{
}

void ADisplayClusterRootActor::Destroyed_Editor()
{
	ResetEntireClusterPreviewRendering();

	MarkAsGarbage();
}

void ADisplayClusterRootActor::BeginDestroy_Editor()
{
	ResetEntireClusterPreviewRendering();

	OnPreviewGenerated.Unbind();
	OnPreviewDestroyed.Unbind();
}

void ADisplayClusterRootActor::RerunConstructionScripts_Editor()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ADisplayClusterRootActor::RerunConstructionScripts_Editor"), STAT_RerunConstructionScripts_Editor, STATGROUP_NDisplay);
	
	/* We need to reinitialize since our components are being regenerated here. */
	InitializeRootActor();

	UpdateInnerFrustumPriority();

	StageGeometryComponent->Invalidate();
}

void ADisplayClusterRootActor::UpdateInnerFrustumPriority()
{
	if (InnerFrustumPriority.Num() == 0)
	{
		ResetInnerFrustumPriority();
		return;
	}
	
	TArray<UDisplayClusterICVFXCameraComponent*> Components;
	GetComponents(Components);

	TArray<FString> ValidCameras;
	for (UDisplayClusterICVFXCameraComponent* Camera : Components)
	{
		FString CameraName = Camera->GetName();
		InnerFrustumPriority.AddUnique(CameraName);
		ValidCameras.Add(CameraName);
	}

	// Removes invalid cameras or duplicate cameras.
	InnerFrustumPriority.RemoveAll([ValidCameras, this](const FDisplayClusterComponentRef& CameraRef)
	{
		return !ValidCameras.Contains(CameraRef.Name) || InnerFrustumPriority.FilterByPredicate([CameraRef](const FDisplayClusterComponentRef& CameraRefN2)
		{
			return CameraRef == CameraRefN2;
		}).Num() > 1;
	});
	
	for (int32 Idx = 0; Idx < InnerFrustumPriority.Num(); ++Idx)
	{
		if (UDisplayClusterICVFXCameraComponent* CameraComponent = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *InnerFrustumPriority[Idx].Name))
		{
			CameraComponent->CameraSettings.RenderSettings.RenderOrder = Idx;
		}
	}
}

void ADisplayClusterRootActor::ResetInnerFrustumPriority()
{
	TArray<UDisplayClusterICVFXCameraComponent*> Components;
	GetComponents(Components);

	InnerFrustumPriority.Reset(Components.Num());
	for (UDisplayClusterICVFXCameraComponent* Camera : Components)
	{
		InnerFrustumPriority.Add(Camera->GetName());
	}
	
	// Initialize based on current render priority.
	InnerFrustumPriority.Sort([this](const FDisplayClusterComponentRef& CameraA, const FDisplayClusterComponentRef& CameraB)
	{
		UDisplayClusterICVFXCameraComponent* CameraComponentA = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *CameraA.Name);
		UDisplayClusterICVFXCameraComponent* CameraComponentB = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *CameraB.Name);

		if (CameraComponentA && CameraComponentB)
		{
			return CameraComponentA->CameraSettings.RenderSettings.RenderOrder < CameraComponentB->CameraSettings.RenderSettings.RenderOrder;
		}

		return false;
	});
}

bool ADisplayClusterRootActor::IsSelectedInEditor() const
{
	return bIsSelectedInEditor || Super::IsSelectedInEditor();
}

void ADisplayClusterRootActor::SetIsSelectedInEditor(bool bValue)
{
	bIsSelectedInEditor = bValue;
}

static FName Name_RelativeLocation = USceneComponent::GetRelativeLocationPropertyName();
static FName Name_RelativeRotation = USceneComponent::GetRelativeRotationPropertyName();
static FName Name_RelativeScale3D = USceneComponent::GetRelativeScale3DPropertyName();

void ADisplayClusterRootActor::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	if (bRequiresComponentRefresh
		&& TransactionEvent.GetEventType() == ETransactionObjectEventType::Finalized)
	{
		if ((GEditor && GEditor->bIsSimulatingInEditor && GetWorld() != nullptr) || ReregisterComponentsWhenModified())
		{
			UnregisterAllComponents();
			ReregisterAllComponents();
		}
		bRequiresComponentRefresh = false;
	}
	Super::PostTransacted(TransactionEvent);
}

void ADisplayClusterRootActor::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent)
{
	const FProperty* RootProperty = PropertyChainEvent.PropertyChain.GetActiveNode()->GetValue();
	const FProperty* TailProperty = PropertyChainEvent.PropertyChain.GetTail()->GetValue();
	if (PropertyChainEvent.ChangeType  == EPropertyChangeType::Interactive && RootProperty && TailProperty)
	{
		const FName RootPropertyName = RootProperty->GetFName();
		const FName TailPropertyName = TailProperty->GetFName();
		if (RootPropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, CurrentConfigData)
			&& RootPropertyName != TailPropertyName)
		{
			// Do not propagate the PostEditChangeProperty because we are in an interactive edit of a suboject
			// and we do not need to rerun any construction scripts.
			bIsInteractiveEditingSubobject = true;
			bRequiresComponentRefresh = true;
		}
	}

	Super::PostEditChangeChainProperty(PropertyChainEvent);
	bIsInteractiveEditingSubobject = false;
}

void ADisplayClusterRootActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (bIsInteractiveEditingSubobject)
	{
		return;
	}

	// The AActor method, simplified and modified to skip construction scripts.
	// Component registration still needs to occur or the actor will look like it disappeared.
	auto SuperCallWithoutConstructionScripts = [&]
	{
		if (IsPropertyChangedAffectingDataLayers(PropertyChangedEvent))
		{
			FixupDataLayers(/*bRevertChangesOnLockedDataLayer*/true);
		}

		const bool bTransformationChanged = (PropertyName == Name_RelativeLocation || PropertyName == Name_RelativeRotation || PropertyName == Name_RelativeScale3D);

		if ((GEditor && GEditor->bIsSimulatingInEditor && GetWorld() != nullptr) || ReregisterComponentsWhenModified())
		{
			// If a transaction is occurring we rely on the true parent method instead.
			check (!CurrentTransactionAnnotation.IsValid());

			UnregisterAllComponents();
			ReregisterAllComponents();
		}

		// Let other systems know that an actor was moved
		if (bTransformationChanged)
		{
			GEngine->BroadcastOnActorMoved( this );
		}

		FEditorSupportDelegates::UpdateUI.Broadcast();
		UObject::PostEditChangeProperty(PropertyChangedEvent);
	};

	bool bReinitializeActor = true;
	bool bCanSkipConstructionScripts = false;
	bool bResetPreviewComponents = false;
	if (const UDisplayClusterBlueprint* Blueprint = Cast<UDisplayClusterBlueprint>(UBlueprint::GetBlueprintFromClass(GetClass())))
	{
		bCanSkipConstructionScripts = !Blueprint->bRunConstructionScriptOnInteractiveChange;
	}
	
	if (bCanSkipConstructionScripts && PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive && !CurrentTransactionAnnotation.IsValid())
	{
		// Avoid calling construction scripts when the change occurs while the user is dragging a slider.
		SuperCallWithoutConstructionScripts();
		bReinitializeActor = false;
	}
	else
	{
		bResetPreviewComponents = true;
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
	
	// Config file has been changed, we should rebuild the nDisplay hierarchy
	// Cluster node ID has been changed
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bFreezePreviewRender)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewRenderTargetRatioMult)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bPreviewICVFXFrustums)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewICVFXFrustumsFarDistance)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewMaxTextureDimension)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewNodeId)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, RenderMode))
	{
		bReinitializeActor = false;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, TickPerFrame)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, ViewportsPerFrame))
	{
		ResetEntireClusterPreviewRendering();

		bReinitializeActor = false;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, InnerFrustumPriority))
	{
		ResetInnerFrustumPriority();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bPreviewEnablePostProcess)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, DefaultDisplayDeviceName))
	{
		ResetEntireClusterPreviewRendering();
		bReinitializeActor = false;
	}
	
	if (bReinitializeActor)
	{
		InitializeRootActor();
	}

	if (bResetPreviewComponents)
	{
	}
}

void ADisplayClusterRootActor::PostEditMove(bool bFinished)
{
	// Don't update the preview with the config data if we're just moving the actor.
	Super::PostEditMove(bFinished);
}

void ADisplayClusterRootActor::HandleAssetReload(const EPackageReloadPhase InPackageReloadPhase,
	FPackageReloadedEvent* InPackageReloadedEvent)
{
	if (InPackageReloadPhase == EPackageReloadPhase::PrePackageFixup)
	{
		// Preview components need to be released here. During a package reload BeginDestroy will be called on the
		// actor, but the preview components are already detached and never have DestroyComponent called on them.
		// This causes the package to keep references around and cause a crash during the reload.
		BeginDestroy_Editor();
	}
}

void ADisplayClusterRootActor::OnEndObjectMovement(UObject& InObject)
{
	// If any of this stage actor's components have been moved, invalidate the stage geometry map
	if (InObject.IsA<USceneComponent>() && Cast<USceneComponent>(&InObject)->GetOwner() == this)
	{
		StageGeometryComponent->Invalidate();
	}
}

#endif
