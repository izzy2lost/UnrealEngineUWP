// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/AvaGizmoComponent.h"

#include "AvaLog.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"
#include "Viewport/Interaction/IAvaGizmoObject.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

UAvaGizmoComponent::UAvaGizmoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static constexpr const TCHAR* DefaultMaterialPath = TEXT("Material'/Avalanche/M_SeeThrough.M_SeeThrough'");
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> DefaultMaterial(DefaultMaterialPath);
	if (DefaultMaterial.Succeeded())
	{
		Material = DefaultMaterial.Get();
	}
	else
	{
		UE_LOG(LogAva, Warning, TEXT("DefaultMaterial could not be loaded from path: '%s'"), DefaultMaterialPath);
	}
}

void UAvaGizmoComponent::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (bIsGizmoEnabled && !bAreGizmoValuesApplied)
		{
			StoreComponentValues();
			ApplyGizmoValues();
		}
	}
}

#if WITH_EDITOR
void UAvaGizmoComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyGizmoValues();
}
#endif

void UAvaGizmoComponent::OnRegister()
{
	// @note: I haven't seen this used anywhere else
	if (AActor* Owner = GetOwner();
		IsValid(Owner))
	{
		if (UWorld* World = Owner->GetWorld())
		{
			if (bIsGizmoEnabled && !bAreGizmoValuesApplied)
			{
				StoreComponentValues();
				ApplyGizmoValues();
			}
			
			PostRegisterComponentsHandle = World->AddOnPostRegisterAllActorComponentsHandler(
				FOnPostRegisterAllActorComponents::FDelegate::CreateUObject(this, &UAvaGizmoComponent::OnPostRegisterParentComponents));
		}
	}

	Super::OnRegister();
}

void UAvaGizmoComponent::OnUnregister()
{
	// @note: I haven't seen this used anywhere else
	if (AActor* Owner = GetOwner();
		IsValid(Owner))
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->RemoveOnPostRegisterAllActorComponentsHandler(PostRegisterComponentsHandle);
		}
	}

	RestoreComponentValues();
	
	Super::OnUnregister();
}

void UAvaGizmoComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	if (bIsGizmoEnabled)
	{
		if (!bAreGizmoValuesApplied)
		{
			StoreComponentValues();
			ApplyGizmoValues();
		}
	}
}

void UAvaGizmoComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Only restore if destroying this single component, not on actor teardown
	if (!bDestroyingHierarchy)
	{
		RestoreComponentValues();	
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UAvaGizmoComponent::ApplyGizmoValues()
{
	if (!bIsGizmoEnabled || bIsRestoringValues)
	{
		return;
	}

	if (AActor* Owner = GetOwner();
		IsValid(Owner))
	{
		bAreGizmoValuesApplied = true;
	
		Owner->ForEachComponent<UPrimitiveComponent>(bApplyToChildActors, [this](UPrimitiveComponent* InComponent)
		{
			InComponent->bVisibleInReflectionCaptures = false;
			InComponent->bVisibleInRealTimeSkyCaptures = false;
			InComponent->bVisibleInRayTracing = false;
			InComponent->SetVisibleInRayTracing(false);
			InComponent->SetVisibleInSceneCaptureOnly(false);

			InComponent->SetAffectDistanceFieldLighting(false);
			InComponent->SetAffectDynamicIndirectLighting(false);
			InComponent->SetAffectIndirectLightingWhileHidden(false);

			InComponent->SetCastShadow(bCastShadow);
			InComponent->SetVisibility(bIsVisibleInEditor, true);
			InComponent->SetHiddenInGame(bIsHiddenInGame);

			InComponent->SetRenderCustomDepth(bSetStencil);
			InComponent->SetCustomDepthStencilValue(StencilId);	

			InComponent->SetRenderInMainPass(bRenderInMainPass);
			InComponent->SetRenderInDepthPass(bRenderDepth);

			if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(InComponent))
			{
				DynamicMeshComponent->SetEnableWireframeRenderPass(bDrawWireframe);
				DynamicMeshComponent->WireframeColor = WireframeColor;
			}			

			if (Material)
			{
				for (int32 MaterialIdx = 0; MaterialIdx < InComponent->GetNumMaterials(); ++MaterialIdx)
				{
					InComponent->SetMaterial(MaterialIdx, Material);
				}
			}
		});

		if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			TArray<UActorComponent*> GizmoObjectComponents = Owner->GetComponentsByInterface(UAvaGizmoObjectInterface::StaticClass());
			for (UActorComponent* Component : GizmoObjectComponents)
			{
				IAvaGizmoObjectInterface::Execute_ToggleGizmo(Component, this, true);	
			}
		}

#if WITH_EDITOR
		// Mark camera cut to remove temporal effects
		for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			LevelVC->SetIsCameraCut();
		}
#endif

		Owner->MarkComponentsRenderStateDirty();
	}
}

void UAvaGizmoComponent::RestoreComponentValues()
{
	if (!bAreGizmoValuesApplied || bIsRestoringValues)
	{
		return;
	}

	bIsRestoringValues = true;

	ON_SCOPE_EXIT
	{
		bIsRestoringValues = false;
	};

	if (AActor* Owner = GetOwner();
		IsValid(Owner))
	{
		bAreGizmoValuesApplied = false;
		
		Owner->ForEachComponent<UPrimitiveComponent>(bApplyToChildActors, [this, &Owner](UPrimitiveComponent* InComponent)
		{
			FSoftComponentReference ComponentReference;
			ComponentReference.PathToComponent = InComponent->GetPathName(Owner);
			
			if (FAvaGizmoComponentPrimitiveValues* StoredComponentValues = ComponentValues.Find(ComponentReference))
			{
				InComponent->bVisibleInReflectionCaptures = StoredComponentValues->bVisibleInReflectionCaptures;
				InComponent->bVisibleInRealTimeSkyCaptures = StoredComponentValues->bVisibleInRealTimeSkyCaptures;
				InComponent->bVisibleInRayTracing = StoredComponentValues->bVisibleInRayTracing;

				InComponent->SetAffectDistanceFieldLighting(StoredComponentValues->bAffectDistanceFieldLighting);
				InComponent->SetAffectDynamicIndirectLighting(StoredComponentValues->bAffectDynamicIndirectLighting);
				InComponent->SetAffectIndirectLightingWhileHidden(StoredComponentValues->bAffectIndirectLightingWhileHidden);
										
				InComponent->CastShadow = StoredComponentValues->bCastShadow;
				InComponent->SetVisibility(StoredComponentValues->bIsVisible);
				InComponent->bHiddenInGame = StoredComponentValues->bIsHiddenInGame;

				InComponent->SetRenderCustomDepth(StoredComponentValues->bRendersCustomDepth);
				InComponent->SetCustomDepthStencilValue(StoredComponentValues->CustomStencilId);

				InComponent->SetRenderInMainPass(StoredComponentValues->bRendersInMainPass);
				InComponent->SetRenderInDepthPass(StoredComponentValues->bRendersDepth);

				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(InComponent))
				{
					DynamicMeshComponent->SetEnableWireframeRenderPass(StoredComponentValues->bDrawWireframe);
					DynamicMeshComponent->WireframeColor = StoredComponentValues->WireframeColor;
				}

				const int32 ActualNumMaterials = InComponent->GetNumMaterials();
				const int32 ExpectedNumMaterials = StoredComponentValues->Materials.Num();

				if (ActualNumMaterials != ExpectedNumMaterials)
				{
					UE_LOG(LogTemp, Warning, TEXT("Material count mismatch when restoring component '%s' - got %i, expected %i"),
						*InComponent->GetName(),
						ActualNumMaterials,
						ExpectedNumMaterials);
				}

				for (int32 MaterialIdx = 0; MaterialIdx < FMath::Min(ActualNumMaterials, ExpectedNumMaterials); ++MaterialIdx)
				{
					InComponent->SetMaterial(MaterialIdx, StoredComponentValues->Materials[MaterialIdx]);
				}
			}
		});

		if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			TArray<UActorComponent*> GizmoObjectComponents = Owner->GetComponentsByInterface(UAvaGizmoObjectInterface::StaticClass());
			for (UActorComponent* Component : GizmoObjectComponents)
			{
				if (IsValid(Component))
				{
					IAvaGizmoObjectInterface::Execute_ToggleGizmo(Component, this, false);	
				}
			}
		}

		Owner->MarkComponentsRenderStateDirty();
	}
}

void UAvaGizmoComponent::SetGizmoEnabled(const bool bInIsEnabled)
{
	if (bIsGizmoEnabled != bInIsEnabled)
	{
		bIsGizmoEnabled = bInIsEnabled;
		if (bIsGizmoEnabled)
		{
			ApplyGizmoValues();
		}
		else
		{
			RestoreComponentValues();
		}
	}
}

bool UAvaGizmoComponent::AppliesToChildActors() const
{
	return bApplyToChildActors;
}

void UAvaGizmoComponent::SetApplyToChildActors(bool bInValue)
{
	if (bApplyToChildActors != bInValue)
	{
		bApplyToChildActors = bInValue;
		ApplyGizmoValues();
	}
}

UMaterialInterface* UAvaGizmoComponent::GetMaterial() const
{
	return Material;
}

void UAvaGizmoComponent::SetMaterial(UMaterialInterface* InMaterial)
{
	if (Material != InMaterial)
	{
		Material = InMaterial;
		ApplyGizmoValues();
	}
}

void UAvaGizmoComponent::SetMaterialToDefault()
{
	static constexpr const TCHAR* MaterialPath = TEXT("Material'/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial'");
	if (UMaterialInterface* FoundMaterial = LoadObject<UMaterialInterface>(GetTransientPackage(), MaterialPath))
	{
		SetMaterial(FoundMaterial);
	}
}

void UAvaGizmoComponent::SetMaterialToSeeThrough()
{
	static constexpr const TCHAR* MaterialPath = TEXT("Material'/Avalanche/M_SeeThrough.M_SeeThrough'");
	if (UMaterialInterface* FoundMaterial = LoadObject<UMaterialInterface>(GetTransientPackage(), MaterialPath))
	{
		SetMaterial(FoundMaterial);
	}
}

bool UAvaGizmoComponent::CastsShadow() const
{
	return bCastShadow;
}

void UAvaGizmoComponent::SetCastShadow(bool bInValue)
{
	if (bCastShadow != bInValue)
	{
		bCastShadow = bInValue;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::IsVisibleInEditor() const
{
	return bIsVisibleInEditor;
}

void UAvaGizmoComponent::SetVisibleInEditor(bool bInValue)
{
	if (bIsVisibleInEditor != bInValue)
	{
		bIsVisibleInEditor = bInValue;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::IsHiddenInGame() const
{
	return bIsHiddenInGame;
}

void UAvaGizmoComponent::SetHiddenInGame(bool bInValue)
{
	if (bIsHiddenInGame != bInValue)
	{
		bIsHiddenInGame = bInValue;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::RendersInMainPass() const
{
	return bRenderInMainPass;
}

void UAvaGizmoComponent::SetRenderInMainPass(const bool bInValue)
{
	if (bRenderInMainPass != bInValue)
	{
		bRenderInMainPass = bInValue;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::RendersDepth() const
{
	return bRenderDepth;
}

void UAvaGizmoComponent::SetRenderDepth(const bool bInValue)
{
	if (bRenderDepth != bInValue)
	{
		bRenderDepth = bInValue;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::ShowsWireframe() const
{
	return bDrawWireframe;
}

void UAvaGizmoComponent::SetShowWireframe(bool bInValue)
{
	if (bDrawWireframe != bInValue)
	{
		bDrawWireframe = bInValue;
		ApplyGizmoValues();
	}
}

void UAvaGizmoComponent::SetWireframeColor(const FLinearColor& InColor)
{
	if (WireframeColor != InColor)
	{
		WireframeColor = InColor;
		ApplyGizmoValues();
	}
}

bool UAvaGizmoComponent::SetsStencil() const
{
	return bSetStencil;
}

void UAvaGizmoComponent::SetsStencil(const bool bInSetStencil)
{
	if (bSetStencil != bInSetStencil)
	{
		bSetStencil = bInSetStencil;
		ApplyGizmoValues();
	}
}

const uint8 UAvaGizmoComponent::GetStencilId() const
{
	return StencilId;
}

void UAvaGizmoComponent::SetStencilId(const uint8 InStencilId)
{
	if (StencilId != InStencilId)
	{
		StencilId = InStencilId;
		ApplyGizmoValues();
	}
}

void UAvaGizmoComponent::StoreComponentValues()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (const AActor* Owner = GetOwner();
			IsValid(Owner))
		{
			bool bWasOneOrMorePrimitiveComponents = false;
			Owner->ForEachComponent<UPrimitiveComponent>(bApplyToChildActors, [this, &Owner, &bWasOneOrMorePrimitiveComponents](UPrimitiveComponent* InComponent)
			{
				bWasOneOrMorePrimitiveComponents = true;
				
				FSoftComponentReference ComponentReference;
				ComponentReference.PathToComponent = InComponent->GetPathName(Owner);
				
				FAvaGizmoComponentPrimitiveValues& StoredComponentValues = ComponentValues.FindOrAdd(ComponentReference);
				
				StoredComponentValues.bVisibleInReflectionCaptures = InComponent->bVisibleInReflectionCaptures;
				StoredComponentValues.bVisibleInRealTimeSkyCaptures = InComponent->bVisibleInRealTimeSkyCaptures;
				StoredComponentValues.bVisibleInRayTracing = InComponent->bVisibleInRayTracing;
				
				StoredComponentValues.bAffectDistanceFieldLighting = InComponent->bAffectDistanceFieldLighting;
				StoredComponentValues.bAffectDynamicIndirectLighting = InComponent->bAffectDynamicIndirectLighting;
				StoredComponentValues.bAffectIndirectLightingWhileHidden = InComponent->bAffectIndirectLightingWhileHidden;
				
				StoredComponentValues.bCastShadow = InComponent->CastShadow;
				StoredComponentValues.bIsVisible = InComponent->IsVisible();
				StoredComponentValues.bIsHiddenInGame = InComponent->bHiddenInGame;

				StoredComponentValues.bRendersCustomDepth = InComponent->bRenderCustomDepth > 0;
				StoredComponentValues.CustomStencilId = InComponent->CustomDepthStencilValue;

				StoredComponentValues.bRendersInMainPass = InComponent->bRenderInMainPass;
				StoredComponentValues.bRendersDepth = InComponent->bRenderInDepthPass;

				// @todo: move this impl specific stuff to a Handler/Provider subsystem
				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(InComponent))
				{
					StoredComponentValues.bDrawWireframe = DynamicMeshComponent->GetEnableWireframeRenderPass();
					StoredComponentValues.WireframeColor = DynamicMeshComponent->WireframeColor;
				}

				TArray<UMaterialInterface*> UsedMaterials;
				InComponent->GetUsedMaterials(UsedMaterials);

				Algo::Transform(UsedMaterials, StoredComponentValues.Materials, [](UMaterialInterface* InMaterial)
				{
					return InMaterial;
				});
			});

			if (!bWasOneOrMorePrimitiveComponents)
			{
				UE_LOG(LogAva, Warning, TEXT("Actor '%s' doesn't contain any components applicable to UAvaGizmoComponent"), *Owner->GetName());
			}
		}
	}
}

void UAvaGizmoComponent::OnPostRegisterParentComponents(AActor* InActor)
{
	if (AActor* Owner = GetOwner();
		IsValid(Owner) && Owner == InActor)
	{
		if (bIsGizmoEnabled)
		{
			if (!bAreGizmoValuesApplied)
			{
				StoreComponentValues();
				
				// Allows for all other components to do stuff first
				ApplyGizmoValues();
			}
		}
	}
}
