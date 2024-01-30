// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerGridLayout.h"
#include "Cloner/AvaClonerComponent.h"
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraSystem.h"

void UAvaClonerGridLayout::SetCountX(int32 InCountX)
{
	if (CountX == InCountX)
	{
		return;
	}

	CountX = InCountX;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetCountY(int32 InCountY)
{
	if (CountY == InCountY)
	{
		return;
	}

	CountY = InCountY;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetCountZ(int32 InCountZ)
{
	if (CountZ == InCountZ)
	{
		return;
	}

	CountZ = InCountZ;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetSpacingX(float InSpacingX)
{
	if (SpacingX == InSpacingX)
	{
		return;
	}

	SpacingX = InSpacingX;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetSpacingY(float InSpacingY)
{
	if (SpacingY == InSpacingY)
	{
		return;
	}

	SpacingY = InSpacingY;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetSpacingZ(float InSpacingZ)
{
	if (SpacingZ == InSpacingZ)
	{
		return;
	}

	SpacingZ = InSpacingZ;
	UpdateLayoutParameters();	
}

void UAvaClonerGridLayout::SetConstraint(EAvaClonerGridConstraint InConstraint)
{
	if (Constraint == InConstraint)
	{
		return;
	}

	Constraint = InConstraint;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetInvertConstraint(bool bInInvertConstraint)
{
	if (bInvertConstraint == bInInvertConstraint)
	{
		return;
	}

	bInvertConstraint = bInInvertConstraint;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetSphereConstraint(const FAvaClonerGridConstraintSphere& InConstraint)
{
	SphereConstraint = InConstraint;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetCylinderConstraint(const FAvaClonerGridConstraintCylinder& InConstraint)
{
	CylinderConstraint = InConstraint;
	UpdateLayoutParameters();
}

void UAvaClonerGridLayout::SetTextureConstraint(const FAvaClonerGridConstraintTexture& InConstraint)
{
	TextureConstraint = InConstraint;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TAvaPropertyChangeDispatcher<UAvaClonerGridLayout> UAvaClonerGridLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, CountX), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, CountY), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, CountZ), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, SpacingX), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, SpacingY), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, SpacingZ), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, Constraint), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, bInvertConstraint), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, SphereConstraint), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, CylinderConstraint), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UAvaClonerGridLayout, TextureConstraint), &UAvaClonerGridLayout::OnLayoutPropertyChanged },
};

void UAvaClonerGridLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UAvaClonerGridLayout::OnLayoutParametersChanged(UAvaClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("GridCountX"), CountX);
	
	InComponent->SetIntParameter(TEXT("GridCountY"), CountY);

	InComponent->SetIntParameter(TEXT("GridCountZ"), CountZ);
	
	InComponent->SetVectorParameter(TEXT("GridSpacing"), FVector(SpacingX, SpacingY, SpacingZ));
	
	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable ConstraintVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerGridConstraint>()), TEXT("Constraint"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Constraint), ConstraintVar);

	InComponent->SetBoolParameter(TEXT("ConstraintInvert"), Constraint != EAvaClonerGridConstraint::None ? bInvertConstraint : false);

	InComponent->SetVectorParameter(TEXT("ConstraintCylinderCenter"), CylinderConstraint.Center);
	
	InComponent->SetFloatParameter(TEXT("ConstraintCylinderHeight"), CylinderConstraint.Height);
	
	InComponent->SetFloatParameter(TEXT("ConstraintCylinderRadius"), CylinderConstraint.Radius);

	InComponent->SetVectorParameter(TEXT("ConstraintSphereCenter"), SphereConstraint.Center);
	
	InComponent->SetFloatParameter(TEXT("ConstraintSphereRadius"), SphereConstraint.Radius);
	
	static const FNiagaraVariable ConstraintTextureChannelVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerTextureSampleChannel>()), TEXT("ConstraintTextureChannel"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.Channel), ConstraintTextureChannelVar);
	
	static const FNiagaraVariable ConstraintTextureCompareModeVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerCompareMode>()), TEXT("ConstraintTextureCompareMode"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.CompareMode), ConstraintTextureCompareModeVar);
	
	static const FNiagaraVariable ConstraintTexturePlaneVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerPlane>()), TEXT("ConstraintTexturePlane"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.Plane), ConstraintTexturePlaneVar);

	InComponent->SetFloatParameter(TEXT("ConstraintTextureThreshold"), TextureConstraint.Threshold);
	
	static const FNiagaraVariable ConstraintTextureSamplerVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceTexture::StaticClass()), TEXT("ConstraintTextureSampler"));
	UNiagaraDataInterfaceTexture* TextureSamplerDI = Cast<UNiagaraDataInterfaceTexture>(ExposedParameters.GetDataInterface(ConstraintTextureSamplerVar));
	TextureSamplerDI->SetTexture(TextureConstraint.Texture.Get());
}
