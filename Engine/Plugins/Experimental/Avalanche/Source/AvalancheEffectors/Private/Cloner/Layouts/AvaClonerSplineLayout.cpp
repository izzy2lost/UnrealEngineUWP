// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerSplineLayout.h"

#include "Cloner/AvaClonerActor.h"
#include "Cloner/AvaClonerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataInterfaceSpline.h"
#include "NiagaraSystem.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

void UAvaClonerSplineLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerSplineLayout::SetSplineActorWeak(const TWeakObjectPtr<AActor>& InSplineActor)
{
	if (SplineActorWeak == InSplineActor)
	{
		return;
	}

	SplineActorWeak = InSplineActor;
	UpdateLayoutParameters();
}

void UAvaClonerSplineLayout::SetSplineActor(AActor* InSplineActor)
{
	SetSplineActorWeak(InSplineActor);
}

void UAvaClonerSplineLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
void UAvaClonerSplineLayout::SpawnLinkedSplineActor()
{
	const AAvaClonerActor* ClonerActor = GetClonerActor();
	
	if (!ClonerActor)
	{
		return;
	}

	UWorld* ClonerWorld = ClonerActor->GetWorld();
	
	FActorSpawnParameters Params;
	Params.bTemporaryEditorActor = false;

	AActor* SpawnedSplineActor = ClonerWorld->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	
	if (!SpawnedSplineActor)
	{
		return;
	}

	// Construct the new component and attach as needed
	USplineComponent* const NewComponent = NewObject<USplineComponent>(SpawnedSplineActor
		, USplineComponent::StaticClass()
		, MakeUniqueObjectName(SpawnedSplineActor, USplineComponent::StaticClass(), TEXT("SplineComponent"))
		, RF_Transactional);

	SpawnedSplineActor->SetRootComponent(NewComponent);
	
	// Add to SerializedComponents array so it gets saved
	SpawnedSplineActor->AddInstanceComponent(NewComponent);
	NewComponent->OnComponentCreated();
	NewComponent->RegisterComponent();

	// Rerun construction scripts
	SpawnedSplineActor->RerunConstructionScripts();

	SpawnedSplineActor->SetActorLocation(ClonerActor->GetActorLocation());
	SpawnedSplineActor->SetActorRotation(ClonerActor->GetActorRotation());
	
	SetSplineActorWeak(SpawnedSplineActor);
	FActorLabelUtilities::RenameExistingActor(SpawnedSplineActor, TEXT("SplineActor"), true);
}

const TAvaPropertyChangeDispatcher<UAvaClonerSplineLayout> UAvaClonerSplineLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSplineLayout, Count), &UAvaClonerSplineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSplineLayout, SplineActorWeak), &UAvaClonerSplineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSplineLayout, bOrientMesh), &UAvaClonerSplineLayout::OnLayoutPropertyChanged },
};

void UAvaClonerSplineLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerSplineLayout::OnLayoutInactive()
{
	Super::OnLayoutInactive();

	// unbind
	if (USplineComponent* SplineComponent = SplineComponentWeak.Get())
	{
		SplineComponent->TransformUpdated.RemoveAll(this);
		USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
	}
}

void UAvaClonerSplineLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SampleSplineCount"), Count);

	const FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable SampleSplineVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceSpline::StaticClass()), TEXT("SampleSpline"));
	UNiagaraDataInterfaceSpline* SplineDI = Cast<UNiagaraDataInterfaceSpline>(ExposedParameters.GetDataInterface(SampleSplineVar));

	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);
	
	// unbind
	SplineDI->Source = nullptr;
	
	if (USplineComponent* SplineComponent = SplineComponentWeak.Get())
    {
		SplineComponent->TransformUpdated.RemoveAll(this);
		SplineComponentWeak.Reset();
    }
	
	SplineComponentWeak = nullptr;

	// bind
	if (AActor* SplineActor = SplineActorWeak.Get())
	{
		if (USplineComponent* SplineComponent = SplineActor->FindComponentByClass<USplineComponent>())
		{
			SplineDI->Source = SplineActor;
			SplineComponentWeak = SplineComponent;
			
			SplineComponent->TransformUpdated.RemoveAll(this);
			SplineComponent->TransformUpdated.AddUObject(this, &UAvaClonerSplineLayout::OnSampleSplineTransformed);

			USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
			USceneComponent::MarkRenderStateDirtyEvent.AddUObject(this, &UAvaClonerSplineLayout::OnSampleSplineRenderStateUpdated);

			if (AAvaClonerActor* ClonerActor = GetClonerActor())
			{
				ClonerActor->SetActorTransform(SplineActor->GetActorTransform());
			}
		}
	}
}

void UAvaClonerSplineLayout::OnSampleSplineTransformed(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType)
{
	UpdateLayoutParameters();
}

void UAvaClonerSplineLayout::OnSampleSplineRenderStateUpdated(UActorComponent& InComponent)
{
	if (SplineActorWeak.IsValid() && InComponent.GetOwner() == SplineActorWeak.Get())
	{
		UpdateLayoutParameters();
	}
}
