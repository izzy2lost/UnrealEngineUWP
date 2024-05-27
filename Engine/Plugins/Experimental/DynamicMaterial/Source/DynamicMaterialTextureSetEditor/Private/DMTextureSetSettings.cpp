// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMTextureSetSettings.h"

UDMTextureSetSettings::UDMTextureSetSettings()
{
	FDMTextureSetFilter BaseColor;
	BaseColor.FilterStrings = {TEXT("Base_Color"), TEXT("BaseColor"), TEXT("Base_Colour"), TEXT("BaseColour"), TEXT("_BC"), TEXT("Diffuse"), TEXT("Albedo")};
	BaseColor.MaterialProperties = {{EMaterialProperty::MP_BaseColor, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Roughness;
	Roughness.FilterStrings = {TEXT("Roughness"), TEXT("Rough"), TEXT("_R")};
	Roughness.MaterialProperties = {{EMaterialProperty::MP_Roughness, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Normal;
	Normal.FilterStrings = {TEXT("Normal"), TEXT("Norm"), TEXT("_N")};
	Normal.MaterialProperties = {{EMaterialProperty::MP_Normal, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Metallic;
	Metallic.FilterStrings = {TEXT("Metallic"), TEXT("Metal"), TEXT("_M")};
	Metallic.MaterialProperties = {{EMaterialProperty::MP_Metallic, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter AmbientOcclusion;
	AmbientOcclusion.FilterStrings = {TEXT("AmbientOcclusion"), TEXT("Ambient_Occlusion"), TEXT("_AO")};
	AmbientOcclusion.MaterialProperties = {{EMaterialProperty::MP_AmbientOcclusion, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Specular;
	Specular.FilterStrings = {TEXT("Specular"), TEXT("_S")};
	Specular.MaterialProperties = {{EMaterialProperty::MP_EmissiveColor, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Emissive;
	Emissive.FilterStrings = {TEXT("Emissive"), TEXT("Emission"), TEXT("_E")};
	Emissive.MaterialProperties = {{EMaterialProperty::MP_EmissiveColor, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter Opacity;
	Opacity.FilterStrings = {TEXT("Opacity"), TEXT("_O"), TEXT("Alpha"), TEXT("_A")};
	Opacity.MaterialProperties = {{EMaterialProperty::MP_EmissiveColor, EDMTextureChannelMask::RGBA}};

	FDMTextureSetFilter ORM;
	ORM.FilterStrings = {TEXT("_ORM")};
	ORM.MaterialProperties = {
		{EMaterialProperty::MP_Opacity, EDMTextureChannelMask::Red},
		{EMaterialProperty::MP_Roughness, EDMTextureChannelMask::Green},
		{EMaterialProperty::MP_Metallic, EDMTextureChannelMask::Blue}
	};

	Filters.Reserve(9);

	Filters.Add(MoveTemp(BaseColor));
	Filters.Add(MoveTemp(Roughness));
	Filters.Add(MoveTemp(Normal));
	Filters.Add(MoveTemp(Metallic));
	Filters.Add(MoveTemp(AmbientOcclusion));
	Filters.Add(MoveTemp(Specular));
	Filters.Add(MoveTemp(Emissive));
	Filters.Add(MoveTemp(Opacity));
	Filters.Add(MoveTemp(ORM));
}

UDMTextureSetSettings* UDMTextureSetSettings::Get()
{
	return GetMutableDefault<UDMTextureSetSettings>();
}
