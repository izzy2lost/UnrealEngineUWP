// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerCircleLayout.h"
#include "Cloner/AvaClonerComponent.h"

void UAvaClonerCircleLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}
	
	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetRadius(float InRadius)
{
	if (Radius == InRadius)
	{
		return;
	}

	Radius = InRadius;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetAngleStart(float InAngleStart)
{
	if (AngleStart == InAngleStart)
	{
		return;
	}

	AngleStart = InAngleStart;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetAngleRatio(float InAngleRatio)
{
	if (AngleRatio == InAngleRatio)
	{
		return;
	}

	AngleRatio = InAngleRatio;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetPlane(EAvaClonerPlane InPlane)
{
	if (Plane == InPlane)
	{
		return;
	}

	Plane = InPlane;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetRotation(const FRotator& InRotation)
{
	if (Rotation == InRotation)
	{
		return;
	}

	Rotation = InRotation;
	UpdateLayoutParameters();
}

void UAvaClonerCircleLayout::SetScale(const FVector& InScale)
{
	if (Scale == InScale)
	{
		return;
	}

	Scale = InScale;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerCircleLayout> UAvaClonerCircleLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, Count), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, Radius), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, AngleStart), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, AngleRatio), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, bOrientMesh), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, Plane), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, Rotation), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCircleLayout, Scale), &UAvaClonerCircleLayout::OnLayoutPropertyChanged },
};

void UAvaClonerCircleLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerCircleLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("CircleCount"), Count);
		
	InComponent->SetFloatParameter(TEXT("CircleRadius"), Radius);
		
	InComponent->SetFloatParameter(TEXT("CircleStart"), AngleStart);

	InComponent->SetFloatParameter(TEXT("CircleRatio"), AngleRatio);
		
	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);

	FVector CircleRotation(Rotation.Yaw, Rotation.Pitch, Rotation.Roll);

	if (Plane == EAvaClonerPlane::XY)
	{
		CircleRotation = FVector(0);
	}
	else if (Plane == EAvaClonerPlane::YZ)
	{
		CircleRotation = FVector(0, 90, 0);
	}
	else if (Plane == EAvaClonerPlane::XZ)
	{
		CircleRotation = FVector(0, 0, 90);
	}

	InComponent->SetVectorParameter(TEXT("CircleRotation"), CircleRotation);

	InComponent->SetVectorParameter(TEXT("CircleScale"), Scale);
}
