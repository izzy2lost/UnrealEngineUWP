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
