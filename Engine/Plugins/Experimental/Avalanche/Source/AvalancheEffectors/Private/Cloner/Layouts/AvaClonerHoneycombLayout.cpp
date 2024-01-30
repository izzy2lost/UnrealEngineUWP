// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerHoneycombLayout.h"
#include "Cloner/AvaClonerComponent.h"
#include "NiagaraSystem.h"

void UAvaClonerHoneycombLayout::SetPlane(EAvaClonerPlane InPlane)
{
	if (Plane == InPlane)
	{
		return;
	}

	Plane = InPlane;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetWidthCount(int32 InWidthCount)
{
	if (WidthCount == InWidthCount)
	{
		return;
	}

	WidthCount = InWidthCount;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetHeightCount(int32 InHeightCount)
{
	if (HeightCount == InHeightCount)
	{
		return;
	}

	HeightCount = InHeightCount;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetWidthOffset(float InWidthOffset)
{
	if (WidthOffset == InWidthOffset)
	{
		return;
	}

	WidthOffset = InWidthOffset;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetHeightOffset(float InHeightOffset)
{
	if (HeightOffset == InHeightOffset)
	{
		return;
	}

	HeightOffset = InHeightOffset;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetHeightSpacing(float InHeightSpacing)
{
	if (HeightSpacing == InHeightSpacing)
	{
		return;
	}

	HeightSpacing = InHeightSpacing;
	UpdateLayoutParameters();
}

void UAvaClonerHoneycombLayout::SetWidthSpacing(float InWidthSpacing)
{
	if (WidthSpacing == InWidthSpacing)
	{
		return;
	}

	WidthSpacing = InWidthSpacing;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerHoneycombLayout> UAvaClonerHoneycombLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, Plane), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, WidthCount), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, HeightCount), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, WidthOffset), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, HeightOffset), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, WidthSpacing), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerHoneycombLayout, HeightSpacing), &UAvaClonerHoneycombLayout::OnLayoutPropertyChanged },
};

void UAvaClonerHoneycombLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerHoneycombLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);
	
	InComponent->SetIntParameter(TEXT("HoneycombWidthCount"), WidthCount);
		
	InComponent->SetIntParameter(TEXT("HoneycombHeightCount"), HeightCount);
		
	InComponent->SetFloatParameter(TEXT("HoneycombWidthOffset"), WidthOffset);
		
	InComponent->SetFloatParameter(TEXT("HoneycombHeightOffset"), HeightOffset);

	InComponent->SetFloatParameter(TEXT("HoneycombWidthSpacing"), WidthSpacing);

	InComponent->SetFloatParameter(TEXT("HoneycombHeightSpacing"), HeightSpacing);

	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable HoneycombPlaneVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerPlane>()), TEXT("HoneycombPlane"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Plane), HoneycombPlaneVar);
}
