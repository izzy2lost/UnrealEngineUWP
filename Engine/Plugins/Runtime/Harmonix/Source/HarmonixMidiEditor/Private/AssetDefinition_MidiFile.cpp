// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetDefinition_MidiFile.h"
#include "HarmonixMidi/MidiFile.h"

#define LOCTEXT_NAMESPACE "Harmonix_Midi"

TSoftClassPtr<UObject> UAssetDefinition_MidiFile::GetAssetClass() const
{
	return UMidiFile::StaticClass();
}

FText UAssetDefinition_MidiFile::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "MidiFileDefinition", "Midi File");
}

FLinearColor  UAssetDefinition_MidiFile::GetAssetColor() const
{

	return FLinearColor(1.0f, 0.5f, 0.0f);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MidiFile::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Audio / NSLOCTEXT("Harmonix", "HmxAssetCategoryName", "Harmonix") };
	return Categories;
}

bool UAssetDefinition_MidiFile::CanImport() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
