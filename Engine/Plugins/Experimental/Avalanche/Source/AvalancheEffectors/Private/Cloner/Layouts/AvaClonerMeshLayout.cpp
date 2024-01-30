// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerMeshLayout.h"
#include "Cloner/AvaClonerActor.h"
#include "Cloner/AvaClonerComponent.h"
#include "DataInterface/NiagaraDataInterfaceActorComponent.h"
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraSystem.h"

void UAvaClonerMeshLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerMeshLayout::SetAsset(EAvaClonerMeshAsset InAsset)
{
	if (Asset == InAsset)
	{
		return;
	}

	Asset = InAsset;
	UpdateLayoutParameters();
}

void UAvaClonerMeshLayout::SetSampleData(EAvaClonerMeshSampleData InSampleData)
{
	if (SampleData == InSampleData)
	{
		return;
	}

	SampleData = InSampleData;
	UpdateLayoutParameters();
}

void UAvaClonerMeshLayout::SetSampleActorWeak(const TWeakObjectPtr<AActor>& InSampleActor)
{
	if (SampleActorWeak == InSampleActor)
	{
		return;
	}

	SampleActorWeak = InSampleActor;
	UpdateLayoutParameters();
}

void UAvaClonerMeshLayout::SetSampleActor(AActor* InActor)
{
	SetSampleActorWeak(InActor);
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerMeshLayout> UAvaClonerMeshLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerMeshLayout, Count), &UAvaClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerMeshLayout, Asset), &UAvaClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerMeshLayout, SampleData), &UAvaClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerMeshLayout, SampleActorWeak), &UAvaClonerMeshLayout::OnLayoutPropertyChanged },
};

void UAvaClonerMeshLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerMeshLayout::OnLayoutInactive()
{
	Super::OnLayoutInactive();

	// unbind
	if (USceneComponent* SceneComponent = SceneComponentWeak.Get())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
	}
}

void UAvaClonerMeshLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SampleMeshCount"), Count);
	
	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	
	static const FNiagaraVariable SampleMeshAssetVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshAsset>()), TEXT("SampleMeshAsset"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Asset), SampleMeshAssetVar);
	
	static const FNiagaraVariable SampleMeshDataVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshSampleData>()), TEXT("SampleMeshData"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SampleData), SampleMeshDataVar);
	
	static const FNiagaraVariable SampleMeshActorVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceActorComponent::StaticClass()), TEXT("SampleMeshActor"));
	UNiagaraDataInterfaceActorComponent* ActorMeshDI = Cast<UNiagaraDataInterfaceActorComponent>(ExposedParameters.GetDataInterface(SampleMeshActorVar));

	// unbind
	if (USceneComponent* SceneComponent = SceneComponentWeak.Get())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
	}
	SceneComponentWeak = nullptr;
	
	// bind
	AActor* SampleActor = SampleActorWeak.Get();
	if (SampleActor && SampleActor->GetRootComponent())
	{
		ActorMeshDI->SourceActor = SampleActor;
		SceneComponentWeak = SampleActor->GetRootComponent();
		SceneComponentWeak->TransformUpdated.AddUObject(this, &UAvaClonerMeshLayout::OnSampleMeshTransformed);
	}
	else
	{
		SampleActorWeak.Reset();
		SceneComponentWeak.Reset();
	}

	if (Asset == EAvaClonerMeshAsset::StaticMesh)
	{
		static const FNiagaraVariable SampleMeshStaticVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceStaticMesh::StaticClass()), TEXT("SampleMeshStatic"));
		UNiagaraDataInterfaceStaticMesh* StaticMeshDI = Cast<UNiagaraDataInterfaceStaticMesh>(ExposedParameters.GetDataInterface(SampleMeshStaticVar));
		if (SampleActorWeak.IsValid())
		{
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SampleActorWeak->GetComponentByClass(UStaticMeshComponent::StaticClass())))
			{
				StaticMeshDI->SetSourceComponentFromBlueprints(StaticMeshComponent);
			}
		}
	}

	if (Asset == EAvaClonerMeshAsset::SkeletalMesh)
	{
		static const FNiagaraVariable SampleMeshSkeletalVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceSkeletalMesh::StaticClass()), TEXT("SampleMeshSkeletal"));
		UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshDI = Cast<UNiagaraDataInterfaceSkeletalMesh>(ExposedParameters.GetDataInterface(SampleMeshSkeletalVar));
		if (SampleActorWeak.IsValid())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SampleActorWeak->GetComponentByClass(USkeletalMeshComponent::StaticClass())))
			{
				SkeletalMeshDI->SetSourceComponentFromBlueprints(SkeletalMeshComponent);
			}
		}
	}
}

void UAvaClonerMeshLayout::OnSampleMeshTransformed(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType)
{
	UpdateLayoutParameters();
}
