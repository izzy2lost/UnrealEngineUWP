// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMTextureSet.h"

#include "Engine/Texture.h"
#include "SceneTypes.h"

UDMTextureSet::UDMTextureSet()
{
	Textures.Reserve(13); // EMaterialProperty has more than 13 entries.

	Textures.Emplace(EMaterialProperty::MP_EmissiveColor);
	Textures.Emplace(EMaterialProperty::MP_Opacity);
	Textures.Emplace(EMaterialProperty::MP_OpacityMask);
	Textures.Emplace(EMaterialProperty::MP_BaseColor);
	Textures.Emplace(EMaterialProperty::MP_Metallic);
	Textures.Emplace(EMaterialProperty::MP_Specular);
	Textures.Emplace(EMaterialProperty::MP_Roughness);
	Textures.Emplace(EMaterialProperty::MP_Anisotropy);
	Textures.Emplace(EMaterialProperty::MP_Normal);
	Textures.Emplace(EMaterialProperty::MP_Tangent);
	Textures.Emplace(EMaterialProperty::MP_SubsurfaceColor);
	Textures.Emplace(EMaterialProperty::MP_AmbientOcclusion);
	Textures.Emplace(EMaterialProperty::MP_Refraction);
}

bool UDMTextureSet::HasMaterialProperty(TEnumAsByte<EMaterialProperty> InMaterialProperty) const
{
	return Textures.Contains(InMaterialProperty);
}

const TMap<TEnumAsByte<EMaterialProperty>, FDMMaterialTexture>& UDMTextureSet::GetTextures() const
{
	return Textures;
}

bool UDMTextureSet::HasMaterialTexture(TEnumAsByte<EMaterialProperty> InMaterialProperty) const
{
	if (const FDMMaterialTexture* MaterialTexture = Textures.Find(InMaterialProperty))
	{
		return !MaterialTexture->Texture.IsNull();
	}

	return false;
}

bool UDMTextureSet::GetMaterialTexture(TEnumAsByte<EMaterialProperty> InMaterialProperty, FDMMaterialTexture& OutMaterialTexture) const
{
	if (const FDMMaterialTexture* MaterialTexture = Textures.Find(InMaterialProperty))
	{
		OutMaterialTexture = *MaterialTexture;
		return true;
	}

	return false;
}

const FDMMaterialTexture* UDMTextureSet::GetMaterialTexture(TEnumAsByte<EMaterialProperty> InMaterialProperty) const
{
	return Textures.Find(InMaterialProperty);
}

void UDMTextureSet::SetMaterialTexture(TEnumAsByte<EMaterialProperty> InMaterialProperty, const FDMMaterialTexture& InMaterialTexture)
{
	if (FDMMaterialTexture* MaterialTexture = Textures.Find(InMaterialProperty))
	{
		*MaterialTexture = InMaterialTexture;
	}
}

bool UDMTextureSet::ContainsTexture(UTexture* InTexture) const
{
	if (!IsValid(InTexture))
	{
		return false;
	}

	for (const TPair<TEnumAsByte<EMaterialProperty>, FDMMaterialTexture>& Pair : Textures)
	{
		if (Pair.Value.Texture == InTexture)
		{
			return true;
		}
	}

	return false;
}
