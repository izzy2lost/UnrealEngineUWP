// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMDefs.h"

#include "DMTextureSetMaterialProperty.h"
#include "Misc/Paths.h"

int32 FDMUpdateGuard::GuardCount = 0;
uint32 FDMInitializationGuard::GuardCount = 0;

FString UE::DynamicMaterial::CreateNodeComment(const ANSICHAR* InFile, int InLine, const ANSICHAR* InFunction, const FString* InComment)
{
	FString File = FPaths::GetCleanFilename(ANSI_TO_TCHAR(InFile)).RightChop(2);

	if (InComment)
	{
		return FString::Printf(TEXT("%s[%d]: %hs: %s"), *File, InLine, InFunction, **InComment);
	}
	else
	{
		return FString::Printf(TEXT("%s[%d]: %hs"), *File, InLine, InFunction);
	}
}

EDMTextureSetMaterialProperty UE::DynamicMaterial::MaterialPropertyTypeToMaterialProperty(EDMMaterialPropertyType InPropertyType)
{
	switch (InPropertyType)
	{
		case EDMMaterialPropertyType::BaseColor:
			return EDMTextureSetMaterialProperty::BaseColor;

		case EDMMaterialPropertyType::EmissiveColor:
			return EDMTextureSetMaterialProperty::EmissiveColor;

		case EDMMaterialPropertyType::Opacity:
			return EDMTextureSetMaterialProperty::Opacity;

		case EDMMaterialPropertyType::OpacityMask:
			return EDMTextureSetMaterialProperty::OpacityMask;

		case EDMMaterialPropertyType::Roughness:
			return EDMTextureSetMaterialProperty::Roughness;

		case EDMMaterialPropertyType::Specular:
			return EDMTextureSetMaterialProperty::Specular;

		case EDMMaterialPropertyType::Metallic:
			return EDMTextureSetMaterialProperty::Metallic;

		case EDMMaterialPropertyType::Normal:
			return EDMTextureSetMaterialProperty::Normal;

		case EDMMaterialPropertyType::PixelDepthOffset:
			return EDMTextureSetMaterialProperty::PixelDepthOffset;

		case EDMMaterialPropertyType::WorldPositionOffset:
			return EDMTextureSetMaterialProperty::WorldPositionOffset;

		case EDMMaterialPropertyType::AmbientOcclusion:
			return EDMTextureSetMaterialProperty::AmbientOcclusion;

		case EDMMaterialPropertyType::Anisotropy:
			return EDMTextureSetMaterialProperty::Anisotropy;

		case EDMMaterialPropertyType::Refraction:
			return EDMTextureSetMaterialProperty::Refraction;

		case EDMMaterialPropertyType::Tangent:
			return EDMTextureSetMaterialProperty::Tangent;

		default:
			return EDMTextureSetMaterialProperty::None;
	}
}

EDMMaterialPropertyType UE::DynamicMaterial::MaterialPropertyToMaterialPropertyType(EDMTextureSetMaterialProperty InPropertyType)
{
	switch (InPropertyType)
	{
		case EDMTextureSetMaterialProperty::BaseColor:
			return EDMMaterialPropertyType::BaseColor;

		case EDMTextureSetMaterialProperty::EmissiveColor:
			return EDMMaterialPropertyType::EmissiveColor;

		case EDMTextureSetMaterialProperty::Opacity:
			return EDMMaterialPropertyType::Opacity;

		case EDMTextureSetMaterialProperty::OpacityMask:
			return EDMMaterialPropertyType::OpacityMask;

		case EDMTextureSetMaterialProperty::Roughness:
			return EDMMaterialPropertyType::Roughness;

		case EDMTextureSetMaterialProperty::Specular:
			return EDMMaterialPropertyType::Specular;

		case EDMTextureSetMaterialProperty::Metallic:
			return EDMMaterialPropertyType::Metallic;

		case EDMTextureSetMaterialProperty::Normal:
			return EDMMaterialPropertyType::Normal;

		case EDMTextureSetMaterialProperty::PixelDepthOffset:
			return EDMMaterialPropertyType::PixelDepthOffset;

		case EDMTextureSetMaterialProperty::WorldPositionOffset:
			return EDMMaterialPropertyType::WorldPositionOffset;

		case EDMTextureSetMaterialProperty::AmbientOcclusion:
			return EDMMaterialPropertyType::AmbientOcclusion;

		case EDMTextureSetMaterialProperty::Anisotropy:
			return EDMMaterialPropertyType::Anisotropy;

		case EDMTextureSetMaterialProperty::Refraction:
			return EDMMaterialPropertyType::Refraction;

		case EDMTextureSetMaterialProperty::Tangent:
			return EDMMaterialPropertyType::Tangent;

		default:
			return EDMMaterialPropertyType::None;
	}
}

bool FDMInitializationGuard::IsInitializing()
{
	return GuardCount > 0;
}

FDMInitializationGuard::FDMInitializationGuard()
{
	// Used the struct name to make it clear it's a static variable.
	++FDMInitializationGuard::GuardCount;
}

FDMInitializationGuard::~FDMInitializationGuard()
{
	if (FDMInitializationGuard::GuardCount > 0)
	{
		--FDMInitializationGuard::GuardCount;
	}
}
