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

	Super::OnUnregister();

	RestoreComponentValues();
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

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents, bApplyToChildActors);

		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (!IsValid(Component))
			{
				continue;
			}
		
			Component->bVisibleInReflectionCaptures = false;
			Component->bVisibleInRealTimeSkyCaptures = false;
			Component->bVisibleInRayTracing = false;
			Component->SetVisibleInRayTracing(false);
			Component->SetVisibleInSceneCaptureOnly(false);

			Component->SetAffectDistanceFieldLighting(false);
			Component->SetAffectDynamicIndirectLighting(false);
			Component->SetAffectIndirectLightingWhileHidden(false);

			Component->SetCastShadow(bCastShadow);
			Component->SetVisibility(bIsVisibleInEditor, true);
			Component->SetHiddenInGame(bIsHiddenInGame);

			Component->SetRenderCustomDepth(bSetStencil);
			Component->SetCustomDepthStencilValue(StencilId);	

			Component->SetRenderInMainPass(bRenderInMainPass);
			Component->SetRenderInDepthPass(bRenderDepth);

			if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
			{
				DynamicMeshComponent->SetEnableWireframeRenderPass(bDrawWireframe);
				DynamicMeshComponent->WireframeColor = WireframeColor;
			}			

			if (Material)
			{
				for (int32 MaterialIdx = 0; MaterialIdx < Component->GetNumMaterials(); ++MaterialIdx)
				{
					Component->SetMaterial(MaterialIdx, Material);
				}
			}
		}

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

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents, bApplyToChildActors);

		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (!IsValid(Component))
			{
				continue;
			}
			
			FSoftComponentReference ComponentReference;
			ComponentReference.PathToComponent = Component->GetPathName(Owner);
			
			if (FAvaGizmoComponentPrimitiveValues* StoredComponentValues = ComponentValues.Find(ComponentReference))
			{
				Component->bVisibleInReflectionCaptures = StoredComponentValues->bVisibleInReflectionCaptures;
				Component->bVisibleInRealTimeSkyCaptures = StoredComponentValues->bVisibleInRealTimeSkyCaptures;
				Component->bVisibleInRayTracing = StoredComponentValues->bVisibleInRayTracing;

				Component->SetAffectDistanceFieldLighting(StoredComponentValues->bAffectDistanceFieldLighting);
				Component->SetAffectDynamicIndirectLighting(StoredComponentValues->bAffectDynamicIndirectLighting);
				Component->SetAffectIndirectLightingWhileHidden(StoredComponentValues->bAffectIndirectLightingWhileHidden);
										
				Component->CastShadow = StoredComponentValues->bCastShadow;
				Component->SetVisibility(StoredComponentValues->bIsVisible);
				Component->bHiddenInGame = StoredComponentValues->bIsHiddenInGame;

				Component->SetRenderCustomDepth(StoredComponentValues->bRendersCustomDepth);
				Component->SetCustomDepthStencilValue(StoredComponentValues->CustomStencilId);

				Component->SetRenderInMainPass(StoredComponentValues->bRendersInMainPass);
				Component->SetRenderInDepthPass(StoredComponentValues->bRendersDepth);

				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
				{
					DynamicMeshComponent->SetEnableWireframeRenderPass(StoredComponentValues->bDrawWireframe);
					DynamicMeshComponent->WireframeColor = StoredComponentValues->WireframeColor;
				}

				const int32 ActualNumMaterials = Component->GetNumMaterials();
				const int32 ExpectedNumMaterials = StoredComponentValues->Materials.Num();

				if (ActualNumMaterials != ExpectedNumMaterials)
				{
					UE_LOG(LogTemp, Warning, TEXT("Material count mismatch when restoring component '%s' - got %i, expected %i"),
						*Component->GetName(),
						ActualNumMaterials,
						ExpectedNumMaterials);
				}

				for (int32 MaterialIdx = 0; MaterialIdx < FMath::Min(ActualNumMaterials, ExpectedNumMaterials); ++MaterialIdx)
				{
					Component->SetMaterial(MaterialIdx, StoredComponentValues->Materials[MaterialIdx]);
				}
			}
		}

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
			TArray<UPrimitiveComponent*> PrimitiveComponents;
        	Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents, bApplyToChildActors);

			bool bWasOneOrMorePrimitiveComponents = false;
        	for (UPrimitiveComponent* Component : PrimitiveComponents)
        	{
        		if (!IsValid(Component))
        		{
        			continue;
        		}
        		
				bWasOneOrMorePrimitiveComponents = true;
				
				FSoftComponentReference ComponentReference;
				ComponentReference.PathToComponent = Component->GetPathName(Owner);
				
				FAvaGizmoComponentPrimitiveValues& StoredComponentValues = ComponentValues.FindOrAdd(ComponentReference);
				
				StoredComponentValues.bVisibleInReflectionCaptures = Component->bVisibleInReflectionCaptures;
				StoredComponentValues.bVisibleInRealTimeSkyCaptures = Component->bVisibleInRealTimeSkyCaptures;
				StoredComponentValues.bVisibleInRayTracing = Component->bVisibleInRayTracing;
				
				StoredComponentValues.bAffectDistanceFieldLighting = Component->bAffectDistanceFieldLighting;
				StoredComponentValues.bAffectDynamicIndirectLighting = Component->bAffectDynamicIndirectLighting;
				StoredComponentValues.bAffectIndirectLightingWhileHidden = Component->bAffectIndirectLightingWhileHidden;
				
				StoredComponentValues.bCastShadow = Component->CastShadow;
				StoredComponentValues.bIsVisible = Component->IsVisible();
				StoredComponentValues.bIsHiddenInGame = Component->bHiddenInGame;

				StoredComponentValues.bRendersCustomDepth = Component->bRenderCustomDepth > 0;
				StoredComponentValues.CustomStencilId = Component->CustomDepthStencilValue;

				StoredComponentValues.bRendersInMainPass = Component->bRenderInMainPass;
				StoredComponentValues.bRendersDepth = Component->bRenderInDepthPass;

				// @todo: move this impl specific stuff to a Handler/Provider subsystem
				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
				{
					StoredComponentValues.bDrawWireframe = DynamicMeshComponent->GetEnableWireframeRenderPass();
					StoredComponentValues.WireframeColor = DynamicMeshComponent->WireframeColor;
				}

				TArray<UMaterialInterface*> UsedMaterials;
				Component->GetUsedMaterials(UsedMaterials);

				Algo::Transform(UsedMaterials, StoredComponentValues.Materials, [](UMaterialInterface* InMaterial)
				{
					return InMaterial;
				});
			}

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
