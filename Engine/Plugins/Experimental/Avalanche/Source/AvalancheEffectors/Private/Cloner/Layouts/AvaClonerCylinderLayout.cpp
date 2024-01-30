// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerCylinderLayout.h"
#include "Cloner/AvaClonerComponent.h"

void UAvaClonerCylinderLayout::SetBaseCount(int32 InBaseCount)
{
	if (BaseCount == InBaseCount)
	{
		return;
	}

	BaseCount = InBaseCount;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetHeightCount(int32 InHeightCount)
{
	if (HeightCount == InHeightCount)
	{
		return;
	}

	HeightCount = InHeightCount;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetHeight(float InHeight)
{
	if (Height == InHeight)
	{
		return;
	}

	Height = InHeight;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetRadius(float InRadius)
{
	if (Radius == InRadius)
	{
		return;
	}

	Radius = InRadius;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetAngleStart(float InAngleStart)
{
	if (AngleStart == InAngleStart)
	{
		return;
	}

	AngleStart = InAngleStart;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetAngleRatio(float InAngleRatio)
{
	if (AngleRatio == InAngleRatio)
	{
		return;
	}

	AngleRatio = InAngleRatio;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetPlane(EAvaClonerPlane InPlane)
{
	if (Plane == InPlane)
	{
		return;
	}

	Plane = InPlane;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetRotation(const FRotator& InRotation)
{
	if (Rotation == InRotation)
	{
		return;
	}

	Rotation = InRotation;
	UpdateLayoutParameters();
}

void UAvaClonerCylinderLayout::SetScale(const FVector& InScale)
{
	if (Scale == InScale)
	{
		return;
	}

	Scale = InScale;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerCylinderLayout> UAvaClonerCylinderLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, BaseCount), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, HeightCount), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, Height), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, Radius), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, AngleStart), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, AngleRatio), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, bOrientMesh), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, Plane), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, Rotation), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerCylinderLayout, Scale), &UAvaClonerCylinderLayout::OnLayoutPropertyChanged },
};

void UAvaClonerCylinderLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerCylinderLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("CylinderBaseCount"), BaseCount);
		
	InComponent->SetIntParameter(TEXT("CylinderHeightCount"), HeightCount);

	InComponent->SetFloatParameter(TEXT("CylinderHeight"), Height);
		
	InComponent->SetFloatParameter(TEXT("CylinderRadius"), Radius);

	InComponent->SetFloatParameter(TEXT("CylinderRatio"), AngleRatio);

	InComponent->SetFloatParameter(TEXT("CylinderStart"), AngleStart);

	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);

	FVector CylinderRotation(Rotation.Yaw, Rotation.Pitch, Rotation.Roll);

	if (Plane == EAvaClonerPlane::XY)
	{
		CylinderRotation = FVector(0);
	}
	else if (Plane == EAvaClonerPlane::YZ)
	{
		CylinderRotation = FVector(0, 90, 0);
	}
	else if (Plane == EAvaClonerPlane::XZ)
	{
		CylinderRotation = FVector(0, 0, 90);
	}
		
	InComponent->SetVectorParameter(TEXT("CylinderRotation"), CylinderRotation);

	InComponent->SetVectorParameter(TEXT("CylinderScale"), Scale);
}
