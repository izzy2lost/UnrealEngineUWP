// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_OutputSettings.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "TextureGraph.h"
#include "TG_Node.h"
#include "TG_HelperFunctions.h"
#include "Misc/Paths.h"

FTG_OutputSettings* FTG_OutputExpressionInfo::GetOutputSettings()
{
	check(OutputPtr);
	return &OutputPtr->OutputSettings;
}

void FTG_OutputSettings::Set(int InWidth, int InHeight, FString Name /*= "None"*/, FString Path /*= "None"*/, ETG_TextureFormat Format /*= ETG_TextureFormat::BGRA8*/, ETG_TexturePresetType InTextureType /*= ETG_TexturePresetType::None*/,
	TextureCompressionSettings InCompression /*= TextureCompressionSettings::TC_Default*/, TextureGroup InLodGroup /*= TextureGroup::TEXTUREGROUP_World*/, bool InbSRGB /*= false*/)
{
	BaseName = *Name;
	FolderPath = *Path;
	Width = (EResolution)InWidth;
	Height = (EResolution)InHeight;
	TextureFormat = Format;
	TexturePresetType = InTextureType;

	if (TexturePresetType != ETG_TexturePresetType::None)
	{
		OnSetTexturePresetType(TexturePresetType);
	}
	else
	{
		Compression = InCompression;
		LODGroup = InLodGroup;
		bSRGB = InbSRGB;
	}
}

void FTG_OutputSettings::OnSetTexturePresetType(ETG_TexturePresetType Type)
{
	switch (Type)
	{
	case ETG_TexturePresetType::None:
		LODGroup = TextureGroup::TEXTUREGROUP_World;
		Compression = TextureCompressionSettings::TC_Default;
		bSRGB = true;
		break;
	case ETG_TexturePresetType::Diffuse:
	case ETG_TexturePresetType::Emissive:
		LODGroup = TextureGroup::TEXTUREGROUP_Character;
		Compression = TextureCompressionSettings::TC_Default;
		bSRGB = true;
		break;
	case ETG_TexturePresetType::FX:
		LODGroup = TextureGroup::TEXTUREGROUP_Character;
		Compression = TextureCompressionSettings::TC_Masks;
		bSRGB = false;
		break;
	case ETG_TexturePresetType::Normal:
		LODGroup = TextureGroup::TEXTUREGROUP_CharacterNormalMap;
		Compression = TextureCompressionSettings::TC_Normalmap;
		bSRGB = false;
		break;
	case ETG_TexturePresetType::MaskComp:
	case ETG_TexturePresetType::Specular:
		LODGroup = TextureGroup::TEXTUREGROUP_CharacterSpecular;
		Compression = TextureCompressionSettings::TC_Masks;
		bSRGB = false;
		break;
	case ETG_TexturePresetType::Tangent:
		LODGroup = TextureGroup::TEXTUREGROUP_CharacterSpecular;
		Compression = TextureCompressionSettings::TC_Default;
		bSRGB = false;
		break;
	default:
		LODGroup = TextureGroup::TEXTUREGROUP_World;
		Compression = TextureCompressionSettings::TC_Default;
		bSRGB = true;
		break;
	}
}

void UTG_OutputSettingsSet::InitOutputSettings()
{
	OutputExpressionInfos.Empty();

}

FTG_OutputExpressionInfo* UTG_OutputSettingsSet::GetOutputExpressionInfo(FName OutputName)
{
	auto predicate = [OutputName](const FTG_OutputExpressionInfo& OutputPtr) { return OutputPtr.OutputName == OutputName; };
	auto bExists = OutputExpressionInfos.ContainsByPredicate(predicate);
	check(bExists);
	return OutputExpressionInfos.FindByPredicate(predicate);
}

FTG_OutputSettings* UTG_OutputSettingsSet::GetOutputSetting(FName OutputName)
{
	return GetOutputExpressionInfo(OutputName)->GetOutputSettings();
}

void UTG_OutputSettingsSet::AddOutputSetting(UTextureGraph* InTextureGraph, FName OutputName, UTG_Expression_Output* Output)
{
	Modify();

	FTG_OutputSettings Settings;
	Settings.OutputName = OutputName;
	Settings.BaseName = OutputName;
	FString AssetPath = InTextureGraph->GetPathName();
	FString DefaultDirectory = FPaths::GetPath(AssetPath);

	Settings.FolderPath = FName(DefaultDirectory);
	Settings.TextureFormat = ETG_TextureFormat::BGRA8;

	Output->OutputSettings = Settings;
	
	FTG_OutputExpressionInfo SettingsPtr;
	SettingsPtr.OutputName = OutputName;
	SettingsPtr.OutputPtr = Output;
	OutputExpressionInfos.Add(SettingsPtr);

}

void UTG_OutputSettingsSet::RemoveOutputSetting(FName OutputName)
{
	Modify();

	FTG_OutputExpressionInfo Info = *GetOutputExpressionInfo(OutputName);
	OutputExpressionInfos.Remove(Info);
}

void UTG_OutputSettingsSet::RenameOutputSetting(FName OldName, FName NewName)
{
	Modify();

	auto* Info = GetOutputExpressionInfo(OldName);
	Info->GetOutputSettings()->OutputName = NewName;
	Info->OutputName = NewName;
}

void UTG_OutputSettingsSet::UpdateTitle(UTG_Node* Output)
{
	if (Output)
	{
		for (auto& item : OutputExpressionInfos)
		{
			if (item.OutputPtr == Output->GetExpression() && Output->GetNodeName() != item.OutputName)
			{
				Modify();
				item.OutputName = Output->GetNodeName();
				item.GetOutputSettings()->OutputName = item.OutputName;
			}
		}
	}
}

EResolution UTG_OutputSettingsSet::GetMaxWidth()
{
	EResolution MaxWidth = EResolution::Auto;
	for (auto& Item : OutputExpressionInfos)
	{
		auto ItemWidth = Item.OutputPtr->OutputSettings.Width;
		MaxWidth = static_cast<EResolution>(FMath::Max(static_cast<int32>(MaxWidth), static_cast<int32>(ItemWidth)));
	}
	return MaxWidth;
}

EResolution UTG_OutputSettingsSet::GetMaxHeight()
{
	EResolution MaxHeight = EResolution::Auto;
	for (auto& Item : OutputExpressionInfos)
	{
		auto ItemHeight = Item.OutputPtr->OutputSettings.Height;
		MaxHeight = static_cast<EResolution>(FMath::Max(static_cast<int32>(MaxHeight), static_cast<int32>(ItemHeight)));
	}
	return MaxHeight;
}

int32 UTG_OutputSettingsSet::GetMaxBufferChannels()
{
	uint32 MaxBufferChannels = 0;
	for (auto& Item : OutputExpressionInfos)
	{
		uint32 Channels = 0;
		BufferFormat Format = BufferFormat::Auto;
		TextureHelper::GetBufferFormatAndChannelsFromTGTextureFormat(Item.OutputPtr->OutputSettings.TextureFormat, Format, Channels);
		MaxBufferChannels = FMath::Max(MaxBufferChannels, Channels);
	}
	return MaxBufferChannels;
}

BufferFormat UTG_OutputSettingsSet::GetMaxBufferFormat()
{
	BufferFormat MaxBufferFormat = BufferFormat::Auto;
	for (auto& Item : OutputExpressionInfos)
	{
		uint32 Channels = 0;
		BufferFormat Format = BufferFormat::Auto;
		TextureHelper::GetBufferFormatAndChannelsFromTGTextureFormat(Item.OutputPtr->OutputSettings.TextureFormat, Format, Channels);
		MaxBufferFormat = static_cast<BufferFormat>(FMath::Max(static_cast<int32>(MaxBufferFormat), static_cast<int32>(Format)));
	}
	return MaxBufferFormat;
}
