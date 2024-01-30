// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerLineLayout.h"
#include "Cloner/AvaClonerComponent.h"

void UAvaClonerLineLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UAvaClonerLineLayout::SetSpacing(float InSpacing)
{
	if (Spacing == InSpacing)
	{
		return;
	}

	Spacing = InSpacing;
	UpdateLayoutParameters();
}

void UAvaClonerLineLayout::SetAxis(EAvaClonerAxis InAxis)
{
	if (Axis == InAxis)
	{
		return;
	}

	Axis = InAxis;
	UpdateLayoutParameters();
}

void UAvaClonerLineLayout::SetDirection(const FVector& InDirection)
{
	if (Direction == InDirection)
	{
		return;
	}

	Direction = InDirection;
	UpdateLayoutParameters();
}

void UAvaClonerLineLayout::SetRotation(const FRotator& InRotation)
{
	if (Rotation == InRotation)
	{
		return;
	}

	Rotation = InRotation;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerLineLayout> UAvaClonerLineLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerLineLayout, Count), &UAvaClonerLineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerLineLayout, Spacing), &UAvaClonerLineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerLineLayout, Axis), &UAvaClonerLineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerLineLayout, Direction), &UAvaClonerLineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerLineLayout, Rotation), &UAvaClonerLineLayout::OnLayoutPropertyChanged },
};

void UAvaClonerLineLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerLineLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("LineCount"), Count);
		
	InComponent->SetFloatParameter(TEXT("LineSpacing"), Spacing);

	FVector LineAxis;
	if (Axis == EAvaClonerAxis::X)
	{
		LineAxis = FVector::XAxisVector;
	}
	else if (Axis == EAvaClonerAxis::Y)
	{
		LineAxis = FVector::YAxisVector;
	}
	else if (Axis == EAvaClonerAxis::Z)
	{
		LineAxis = FVector::ZAxisVector;
	}
	else
	{
		LineAxis = Direction.GetSafeNormal();
	}

	InComponent->SetVectorParameter(TEXT("LineAxis"), LineAxis);

	InComponent->SetVectorParameter(TEXT("LineRotation"), FVector(Rotation.Roll, Rotation.Pitch, Rotation.Yaw));
}
