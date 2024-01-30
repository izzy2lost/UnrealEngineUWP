// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerSphereUniformLayout.h"
#include "Cloner/AvaClonerComponent.h"

void UAvaClonerSphereUniformLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerSphereUniformLayout::SetRadius(float InRadius)
{
	if (Radius == InRadius)
	{
		return;
	}

	Radius = InRadius;
	UpdateLayoutParameters();
}

void UAvaClonerSphereUniformLayout::SetRatio(float InRatio)
{
	if (Ratio == InRatio)
	{
		return;
	}

	Ratio = InRatio;
	UpdateLayoutParameters();
}

void UAvaClonerSphereUniformLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

void UAvaClonerSphereUniformLayout::SetRotation(const FRotator& InRotation)
{
	if (Rotation == InRotation)
	{
		return;
	}

	Rotation = InRotation;
	UpdateLayoutParameters();
}

void UAvaClonerSphereUniformLayout::SetScale(const FVector& InScale)
{
	if (Scale == InScale)
	{
		return;
	}

	Scale = InScale;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerSphereUniformLayout> UAvaClonerSphereUniformLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, Count), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, Radius), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, Ratio), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, bOrientMesh), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, Rotation), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereUniformLayout, Scale), &UAvaClonerSphereUniformLayout::OnLayoutPropertyChanged },
};

void UAvaClonerSphereUniformLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerSphereUniformLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SphereCount"), Count);
			
	InComponent->SetFloatParameter(TEXT("SphereRadius"), Radius);
		
	InComponent->SetFloatParameter(TEXT("SphereRatio"), Ratio);
		
	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);
		
	InComponent->SetVectorParameter(TEXT("SphereRotation"), FVector(Rotation.Yaw, Rotation.Pitch, Rotation.Roll));
		
	InComponent->SetVectorParameter(TEXT("SphereScale"), Scale);
}
