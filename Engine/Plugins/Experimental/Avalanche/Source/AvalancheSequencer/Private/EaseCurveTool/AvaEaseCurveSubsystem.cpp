// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveSubsystem.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaEaseCurveSubsystem, Log, All);

namespace UE::EaseCurveTool::Json::Private
{
	bool GetPresetFilePath(FString InCategory, const bool bSetNewCategory, FString& OutFilePath)
	{
		OutFilePath.Empty();

		const UAvaEaseCurveToolSettings* const Settings = GetDefault<UAvaEaseCurveToolSettings>();
		check(Settings);

		FString PresetsPath = Settings->GetPresetsPath();
		PresetsPath = FPaths::ConvertRelativePathToFull(PresetsPath);

		if (!FPaths::DirectoryExists(PresetsPath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::GetPresetFilePath() - Configured presets directory does not exist: %s"), *PresetsPath);
			return false;
		}

		if (InCategory.IsEmpty())
		{
			if (!bSetNewCategory)
			{
				return false;
			}

			const FString EaseCurveTool_NewPresetCategory = Settings->GetNewPresetCategory();
			if (EaseCurveTool_NewPresetCategory.IsEmpty())
			{
				UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::GetPresetFilePath() - No preset category and no configured default category"));
				return false;
			}

			InCategory = EaseCurveTool_NewPresetCategory;
		}

		PresetsPath = FPaths::Combine(PresetsPath, InCategory) + TEXT(".json");
		OutFilePath = PresetsPath;

		return FPaths::FileExists(PresetsPath);
	}

	bool LoadCurvePresetsJson(const FString& InFilePath, TSharedPtr<FJsonObject>& OutRootObject)
	{
		FString FileJsonString;
		if (!FFileHelper::LoadFileToString(FileJsonString, *InFilePath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::LoadCurvePresetsJson() - Unable to load Json file: %s"), *InFilePath);
			return false;
		}

		const TSharedPtr<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FileJsonString);
		OutRootObject = MakeShared<FJsonObject>();
		if (!FJsonSerializer::Deserialize(JsonReader.ToSharedRef(), OutRootObject))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::LoadCurvePresetsJson() - Unable to parse file [%s]. Json=[%s]"), *InFilePath, *FileJsonString);
			return false;
		}

		return true;
	}

	bool SaveCurvePresetsJson(const FString& InFilePath, const TSharedRef<FJsonObject>& InRootObject)
	{
		FString FileJsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FileJsonString);
		if (!FJsonSerializer::Serialize(InRootObject, Writer))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::SaveCurvePresetsJson() - Unable to serialize [%s]. Json=[%s]"), *InFilePath, *FileJsonString);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(FileJsonString, *InFilePath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::SaveCurvePresetsJson() - Unable to save Json file: %s"), *InFilePath);
			return false;
		}

		return true;
	}

	bool DoesPresetCategoryExist(const FString& InCategory)
	{
		FString OutFilePath;
		return GetPresetFilePath(InCategory, false, OutFilePath);
	}

	FString AddNewPresetCategory()
	{
		static const FString DefaultNewCategoryName = TEXT("New Category");

		FString NewCategoryName;

		int32 CurrentNum = 1;
		FString FilePath;

		while (true)
		{
			NewCategoryName = (CurrentNum == 1)
				? DefaultNewCategoryName 
				: DefaultNewCategoryName + TEXT(" ") + FString::FromInt(CurrentNum);

			if (!GetPresetFilePath(NewCategoryName, false, FilePath))
			{
				break;
			}

			CurrentNum++;
		}

		const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
		if (!SaveCurvePresetsJson(FilePath, RootObject.ToSharedRef()))
		{
			return FString();
		}

		return NewCategoryName;
	}

	bool RemovePresetCategory(const FString& InCategory)
	{
		FString FilePath;
		if (!GetPresetFilePath(InCategory, false, FilePath))
		{
			return false;
		}

		return IFileManager::Get().Delete(*FilePath);
	}

	bool RenamePresetCategory(const FString& InCategory, const FString& InNewCategory)
	{
		if (!DoesPresetCategoryExist(InCategory))
		{
			return false;
		}

		if (DoesPresetCategoryExist(InNewCategory))
		{
			return false;
		}

		FString OldFilePath;
		if (!GetPresetFilePath(InCategory, false, OldFilePath))
		{
			return false;
		}

		FString NewFilePath;
		GetPresetFilePath(InNewCategory, false, NewFilePath);

		TSharedPtr<FJsonObject> RootObject;
		if (!LoadCurvePresetsJson(OldFilePath, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		if (!SaveCurvePresetsJson(NewFilePath, RootObject.ToSharedRef()))
		{
			return false;
		}

		return RemovePresetCategory(InCategory);
	}
	
	bool DoesPresetExist(const FString& InCategory, const FString& InPreset)
	{
		FString PresetsPath;
		if (!GetPresetFilePath(InCategory, false, PresetsPath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::DoesPresetExist() - Could not find file for preset category: %s"), *InCategory);
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		if (!LoadCurvePresetsJson(PresetsPath, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::DoesPresetExist() - Unable to load Json file: %s"), *PresetsPath);
			return false;
		}

		return RootObject->Values.Contains(InPreset);
	}

	bool AddPreset(const FString& InCategory, const FString& InPreset, const FAvaEaseCurveTangents& InTangents)
	{
		FString PresetsPath;
		const bool bCategoryFileExists = GetPresetFilePath(InCategory, true, PresetsPath);
		if (PresetsPath.IsEmpty())
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::AddPreset() - Could not find file for preset category: %s"), *InCategory);
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;

		if (bCategoryFileExists)
		{
			if (!UE::EaseCurveTool::Json::Private::LoadCurvePresetsJson(PresetsPath, RootObject) || !RootObject.IsValid())
			{
				UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::AddPreset() - Unable to load Json file: %s"), *PresetsPath);
				return false;
			}
		}
		else
		{
			RootObject = MakeShared<FJsonObject>();
		}

		RootObject->Values.Add(InPreset, MakeShared<FJsonValueString>(InTangents.ToJson()));

		if (!SaveCurvePresetsJson(PresetsPath, RootObject.ToSharedRef()))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::AddPreset() - Unable to save Json file: %s"), *PresetsPath);
			return false;
		}

		return true;
	}

	bool RemovePreset(const FString& InCategory, const FString& InPreset)
	{
		FString PresetsPath;
		if (!GetPresetFilePath(InCategory, false, PresetsPath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Could not find file for preset category: %s"), *InCategory);
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		if (!LoadCurvePresetsJson(PresetsPath, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Unable to load Json file: %s"), *PresetsPath);
			return false;
		}

		RootObject->Values.Remove(InPreset);

		if (!SaveCurvePresetsJson(PresetsPath, RootObject.ToSharedRef()))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Unable to save Json file: %s"), *PresetsPath);
			return false;
		}

		return true;
	}

	bool RenamePreset(const FString& InCategory, const FString& InPreset, const FString& InNewPreset)
	{
		FString PresetsPath;
		if (!GetPresetFilePath(InCategory, false, PresetsPath))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Could not find file for preset category: %s"), *InCategory);
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		if (!LoadCurvePresetsJson(PresetsPath, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Unable to load Json file: %s"), *PresetsPath);
			return false;
		}

		if (const TSharedPtr<FJsonValue>* Value = RootObject->Values.Find(InPreset))
		{
			const TSharedPtr<FJsonValue>& ValueRef = *Value;
			RootObject->Values.Add(InNewPreset, MakeShared<FJsonValueString>(ValueRef->AsString()));
			RootObject->Values.Remove(InPreset);
		}

		if (!SaveCurvePresetsJson(PresetsPath, RootObject.ToSharedRef()))
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("CurveEaseToolJson::RemovePreset() - Unable to save Json file: %s"), *PresetsPath);
			return false;
		}

		return true;
	}
}

UAvaEaseCurveSubsystem& UAvaEaseCurveSubsystem::Get()
{
	check(GEditor);
	return *GEditor->GetEditorSubsystem<UAvaEaseCurveSubsystem>();
}

void UAvaEaseCurveSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);

	ReloadPresetsFromJson();
}

void UAvaEaseCurveSubsystem::ExploreJsonPresetsFolder()
{
	const FString EaseCurvePresetsPath = FPaths::ConvertRelativePathToFull(GetDefault<UAvaEaseCurveToolSettings>()->GetPresetsPath());
	FPlatformProcess::ExploreFolder(*EaseCurvePresetsPath);
}

void UAvaEaseCurveSubsystem::ReloadPresetsFromJson()
{
	const UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetDefault<UAvaEaseCurveToolSettings>();
	check(EaseCurveToolSettings);
	const FString EaseCurvePresetsPath = EaseCurveToolSettings->GetPresetsPath();

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFilesRecursive(JsonFiles, *EaseCurvePresetsPath, TEXT("*.json"), true, false);

	// Load all Json files found as their own separate category. Category Name = File Name
	Presets.Empty();
	for (const FString& File : JsonFiles)
	{
		TSharedPtr<FJsonObject> RootObject;
		if (!UE::EaseCurveTool::Json::Private::LoadCurvePresetsJson(File, RootObject) || !RootObject.IsValid())
		{
			UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::LoadCurvePresetsJson() - Unable to load Json file [%s]"), *File);
			continue;
		}

		const FString CategoryName = FPaths::GetBaseFilename(File);

		TArray<TSharedPtr<FAvaEaseCurvePreset>>& PresetCategoryRef = Presets.FindOrAdd(CategoryName);

		for (TMap<FString, TSharedPtr<FJsonValue>>::TConstIterator JsonValueIter = RootObject->Values.CreateConstIterator(); JsonValueIter; ++JsonValueIter)
		{
			const FString& PresetName = JsonValueIter.Key();
			const FString TangentsString = (*JsonValueIter).Value->AsString();

			FAvaEaseCurveTangents Tangents;
			if (!FAvaEaseCurveTangents::FromString(TangentsString, Tangents))
			{
				continue;
			}

			const bool bExists = PresetCategoryRef.ContainsByPredicate([&PresetName](const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
				{
					return InPreset->Name.Equals(PresetName);
				});
			if (!bExists)
			{
				PresetCategoryRef.Add(MakeShared<FAvaEaseCurvePreset>(PresetName, CategoryName, MoveTemp(Tangents)));
			}
		}

		PresetCategoryRef.Sort([](const TSharedPtr<FAvaEaseCurvePreset>& InPresetA, const TSharedPtr<FAvaEaseCurvePreset>& InPresetB)
			{
				return *InPresetA < *InPresetB;
			});
	}
}

TArray<FString> UAvaEaseCurveSubsystem::GetEaseCurveCategories() const
{
	TArray<FString> OutCategories;
	Presets.GenerateKeyArray(OutCategories);
	return OutCategories;
}

TArray<TSharedPtr<FAvaEaseCurvePreset>> UAvaEaseCurveSubsystem::GetEaseCurvePresets() const
{
	TArray<TSharedPtr<FAvaEaseCurvePreset>> OutPresets;

	for (const TPair<FString, TArray<TSharedPtr<FAvaEaseCurvePreset>>>& Preset : Presets)
	{
		OutPresets.Append(Preset.Value);
	}

	return OutPresets;
}

TArray<TSharedPtr<FAvaEaseCurvePreset>> UAvaEaseCurveSubsystem::GetEaseCurvePresets(const FString& InCategory) const
{
	if (!Presets.Contains(InCategory))
	{
		return {};
	}
	return Presets[InCategory];
}

bool UAvaEaseCurveSubsystem::DoesPresetExist(const FString& InName) const
{
	for (const TPair<FString, TArray<TSharedPtr<FAvaEaseCurvePreset>>>& Preset : Presets)
	{
		const bool bExists = Preset.Value.ContainsByPredicate([&InName](const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
			{
				return InPreset->Name.Equals(InName);
			});
		if (bExists)
		{
			return true;
		}
	}
	return false;
}

bool UAvaEaseCurveSubsystem::DoesPresetExist(const FString& InCategory, const FString& InName) const
{
	if (!Presets.Contains(InCategory))
	{
		return false;
	}

	return Presets[InCategory].ContainsByPredicate([&InName](const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
		{
			return InPreset->Name.Equals(InName);
		});
}

TSharedPtr<FAvaEaseCurvePreset> UAvaEaseCurveSubsystem::AddPreset(FAvaEaseCurvePreset InPreset)
{
	if (DoesPresetExist(InPreset.Category, InPreset.Name))
	{
		UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::AddPreset() - Preset already exists: %s"), *InPreset.Name);
		return nullptr;
	}

	if (!UE::EaseCurveTool::Json::Private::AddPreset(InPreset.Category, InPreset.Name, InPreset.Tangents))
	{
		UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::AddPreset() - Unable to add preset %s to category %s Json file"), *InPreset.Name, *InPreset.Category);
		return nullptr;
	}

	TSharedPtr<FAvaEaseCurvePreset> NewPresetPtr = MakeShared<FAvaEaseCurvePreset>(InPreset);
	Presets.Add(InPreset.Category, { NewPresetPtr });

	return NewPresetPtr;
}

TSharedPtr<FAvaEaseCurvePreset> UAvaEaseCurveSubsystem::AddPreset(const FString& InName, const FAvaEaseCurveTangents& InTangents)
{
	const FString CategoryName = GetDefault<UAvaEaseCurveToolSettings>()->GetNewPresetCategory();
	if (CategoryName.IsEmpty())
	{
		return nullptr;
	}

	return AddPreset(FAvaEaseCurvePreset(InName, CategoryName, InTangents));
}

bool UAvaEaseCurveSubsystem::RemovePreset(const FAvaEaseCurvePreset& InPreset)
{
	if (!DoesPresetExist(InPreset.Name))
	{
		UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::RemovePreset() - Preset already exists: %s"), *InPreset.Name);
		return false;
	}

	if (!UE::EaseCurveTool::Json::Private::RemovePreset(InPreset.Category, InPreset.Name))
	{
		UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::RemovePreset() - Failed to remove preset %s from category %s Json file"), *InPreset.Name, *InPreset.Category);
		return false;
	}

	if (Presets.Contains(InPreset.Category))
	{
		TSharedPtr<FAvaEaseCurvePreset> PresetToRemove;
		for (const TSharedPtr<FAvaEaseCurvePreset>& PresetPtr : Presets[InPreset.Category])
		{
			if (PresetPtr->Name.Equals(InPreset.Name, ESearchCase::IgnoreCase))
			{
				PresetToRemove = PresetPtr;
				break;
			}
		}
		if (PresetToRemove.IsValid())
		{
			Presets[InPreset.Category].Remove(PresetToRemove);
		}
	}

	return true;
}

bool UAvaEaseCurveSubsystem::RemovePreset(const FString& InCategory, const FString& InName)
{
	return RemovePreset(FAvaEaseCurvePreset(InName, InCategory, FAvaEaseCurveTangents()));
}

bool UAvaEaseCurveSubsystem::ChangePresetCategory(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategory) const
{
	if (!Presets.Contains(InPreset->Category) || !Presets.Contains(InNewCategory))
	{
		return false;
	}

	// Verify old category exists and new category does not
	if (!DoesPresetExist(InPreset->Category, InPreset->Name) || DoesPresetExist(InNewCategory, InPreset->Name))
	{
		return false;
	}

	if (!UAvaEaseCurveSubsystem::Get().RemovePreset(InPreset->Category, InPreset->Name))
	{
		return false;
	}

	InPreset->Category = InNewCategory;

	return UAvaEaseCurveSubsystem::Get().AddPreset(*InPreset).IsValid();
}

TSharedPtr<FAvaEaseCurvePreset> UAvaEaseCurveSubsystem::FindPreset(const FString& InName)
{
	for (const TPair<FString, TArray<TSharedPtr<FAvaEaseCurvePreset>>>& Preset : Presets)
	{
		const TSharedPtr<FAvaEaseCurvePreset>* PresetPtr = Preset.Value.FindByPredicate([&InName](const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
			{
				return InPreset->Name == InName;
			});
		if (PresetPtr)
		{
			return *PresetPtr;
		}
	}
	return nullptr;
}

TSharedPtr<FAvaEaseCurvePreset> UAvaEaseCurveSubsystem::FindPresetByTangents(const FAvaEaseCurveTangents& InTangents)
{
	for (const TPair<FString, TArray<TSharedPtr<FAvaEaseCurvePreset>>>& Preset : Presets)
	{
		const TSharedPtr<FAvaEaseCurvePreset>* PresetPtr = Preset.Value.FindByPredicate([&InTangents](const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
			{
				return InPreset->Tangents == InTangents;
			});
		if (PresetPtr)
		{
			return *PresetPtr;
		}
	}
	return nullptr;
}

bool UAvaEaseCurveSubsystem::DoesPresetCategoryExist(const FString& InCategory)
{
	return UE::EaseCurveTool::Json::Private::DoesPresetCategoryExist(InCategory);
}

bool UAvaEaseCurveSubsystem::RenamePresetCategory(const FString& InCategory, const FString& InNewCategoryName)
{
	if (!UE::EaseCurveTool::Json::Private::RenamePresetCategory(InCategory, InNewCategoryName))
	{
		return false;
	}

	if (Presets.Remove(InCategory) > 0)
	{
		Presets.Add(InNewCategoryName, Presets[InCategory]);
	}

	return true;
}

bool UAvaEaseCurveSubsystem::AddNewPresetCategory()
{
	const FString NewCategoryName = UE::EaseCurveTool::Json::Private::AddNewPresetCategory();

	if (NewCategoryName.IsEmpty())
	{
		return false;
	}

	Presets.Add(NewCategoryName);

	return true;
}

bool UAvaEaseCurveSubsystem::RemovePresetCategory(const FString& InCategory)
{
	if (!UE::EaseCurveTool::Json::Private::RemovePresetCategory(InCategory))
	{
		return false;
	}

	return Presets.Remove(InCategory) > 0;
}

bool UAvaEaseCurveSubsystem::RenamePreset(const FString& InCategory, const FString& InPreset, const FString& InNewPresetName)
{
	if (!UE::EaseCurveTool::Json::Private::RenamePreset(InCategory, InPreset, InNewPresetName))
	{
		UE_LOG(LogAvaEaseCurveSubsystem, Warning, TEXT("UAvaEaseCurveSubsystem::RenamePreset() - Failed to rename preset %s from category %s Json file"), *InPreset, *InCategory);
		return false;
	}

	if (Presets.Contains(InCategory))
	{
		TSharedPtr<FAvaEaseCurvePreset> PresetToRename;
		for (const TSharedPtr<FAvaEaseCurvePreset>& PresetPtr : Presets[InCategory])
		{
			if (PresetPtr->Name.Equals(InPreset, ESearchCase::IgnoreCase))
			{
				PresetToRename = PresetPtr;
				break;
			}
		}
		if (PresetToRename.IsValid())
		{
			Presets[InCategory].Remove(PresetToRename);

			PresetToRename->Name = InNewPresetName;

			Presets[InCategory].Add(PresetToRename);
		}
	}

	return true;
}
