// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaEaseCurveTangents.h"
#include "Containers/Array.h"
#include "EditorSubsystem.h"
#include "Templates/SharedPointer.h"
#include "AvaEaseCurveSubsystem.generated.h"

struct FAvaEaseCurvePreset;

UCLASS()
class UAvaEaseCurveSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static UAvaEaseCurveSubsystem& Get();

	static void ExploreJsonPresetsFolder();

	void ReloadPresetsFromJson();

	TArray<FString> GetEaseCurveCategories() const;

	TArray<TSharedPtr<FAvaEaseCurvePreset>> GetEaseCurvePresets() const;
	TArray<TSharedPtr<FAvaEaseCurvePreset>> GetEaseCurvePresets(const FString& InCategory) const;

	bool DoesPresetExist(const FString& InName) const;
	bool DoesPresetExist(const FString& InCategory, const FString& InName) const;

	TSharedPtr<FAvaEaseCurvePreset> AddPreset(FAvaEaseCurvePreset InPreset);
	TSharedPtr<FAvaEaseCurvePreset> AddPreset(const FString& InName, const FAvaEaseCurveTangents& InTangents);

	bool RemovePreset(const FAvaEaseCurvePreset& InPreset);
	bool RemovePreset(const FString& InCategory, const FString& InName);

	bool ChangePresetCategory(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategory) const;

	TSharedPtr<FAvaEaseCurvePreset> FindPreset(const FString& InName);
	TSharedPtr<FAvaEaseCurvePreset> FindPresetByTangents(const FAvaEaseCurveTangents& InTangents);

	bool DoesPresetCategoryExist(const FString& InCategory);
	bool RenamePresetCategory(const FString& InCategory, const FString& InNewCategoryName);
	bool AddNewPresetCategory();
	bool RemovePresetCategory(const FString& InCategory);

	bool RenamePreset(const FString& InCategory, const FString& InPreset, const FString& InNewPresetName);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	//~ End USubsystem

protected:
	/** Cached presets from Json files. Each Json file represents a category. */
	TSortedMap<FString, TArray<TSharedPtr<FAvaEaseCurvePreset>>> Presets;
};
