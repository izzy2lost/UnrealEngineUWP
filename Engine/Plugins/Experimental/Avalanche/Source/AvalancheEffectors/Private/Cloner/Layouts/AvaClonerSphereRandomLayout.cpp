// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerSphereRandomLayout.h"
#include "Cloner/AvaClonerComponent.h"

void UAvaClonerSphereRandomLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetRadius(float InRadius)
{
	if (Radius == InRadius)
	{
		return;
	}

	Radius = InRadius;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetDistribution(float InDistribution)
{
	if (Distribution == InDistribution)
	{
		return;
	}

	if (InDistribution < 0.f || InDistribution > 1.f)
	{
		return;
	}

	Distribution = InDistribution;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetLongitude(float InLongitude)
{
	if (Longitude == InLongitude)
	{
		return;
	}

	if (InLongitude < 0.f || InLongitude > 1.f)
	{
		return;
	}

	Longitude = InLongitude;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetLatitude(float InLatitude)
{
	if (Latitude == InLatitude)
	{
		return;
	}

	if (InLatitude < 0.f || InLatitude > 1.f)
	{
		return;
	}

	Latitude = InLatitude;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetRotation(const FRotator& InRotation)
{
	if (Rotation == InRotation)
	{
		return;
	}

	Rotation = InRotation;
	UpdateLayoutParameters();
}

void UAvaClonerSphereRandomLayout::SetScale(const FVector& InScale)
{
	if (Scale == InScale)
	{
		return;
	}

	Scale = InScale;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerSphereRandomLayout> UAvaClonerSphereRandomLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Count), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Radius), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Distribution), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Longitude), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Latitude), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, bOrientMesh), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Rotation), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerSphereRandomLayout, Scale), &UAvaClonerSphereRandomLayout::OnLayoutPropertyChanged },
};

void UAvaClonerSphereRandomLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerSphereRandomLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SphereCount"), Count);
			
	InComponent->SetFloatParameter(TEXT("SphereRadius"), Radius);
		
	InComponent->SetFloatParameter(TEXT("SphereDistribution"), Distribution);
	
	InComponent->SetFloatParameter(TEXT("SphereLongitude"), Longitude);
	
	InComponent->SetFloatParameter(TEXT("SphereLatitude"), Latitude);
		
	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);
		
	InComponent->SetVectorParameter(TEXT("SphereRotation"), FVector(Rotation.Yaw, Rotation.Pitch, Rotation.Roll));
		
	InComponent->SetVectorParameter(TEXT("SphereScale"), Scale);
}
