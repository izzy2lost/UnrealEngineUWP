// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITraceFilterPreset.h"

#include "TraceFilterPresets.generated.h"

namespace UE::TraceTools
{
struct FFilterPresetHelpers
{
	/** Creates a new filtering preset according to the specific object names */
	static void CreateNewPreset(const TArray<TSharedPtr<ITraceObject>>& InObjects);
	/** Creates a set of strings, corresponding to set of non-filtered out object as part of InObjects */
	static void ExtractEnabledObjectNames(const TArray<TSharedPtr<ITraceObject>>& InObjects, TArray<FString>& OutNames);
	/** Returns whether or not shared presets can be modified, requires write-flag on default confing files */
	static bool CanModifySharedPreset();
};
}

/** Structure representing an individual preset in configuration (ini) files */
USTRUCT()
struct FTraceFilterData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FString> AllowlistedNames;

	bool operator==(const FTraceFilterData& Other) const
	{
		return Name == Other.Name && AllowlistedNames == Other.AllowlistedNames;
	}
};

/** UObject containers for the preset data */
UCLASS(Config = Trace)
class ULocalTraceFilterPresetContainer : public UObject
{
	GENERATED_BODY()

	friend struct UE::TraceTools::FFilterPresetHelpers;
public:
	void GetUserPresets(TArray<TSharedPtr<UE::TraceTools::ITraceFilterPreset>>& OutPresets);

	static void AddFilterData(const FTraceFilterData& InFilterData);
	static bool RemoveFilterData(const FTraceFilterData& InFilterData);
	static void Save();
protected:
	UPROPERTY(Config)
	TArray<FTraceFilterData> UserPresets;
};

UCLASS(Config = Trace, DefaultConfig)
class USharedTraceFilterPresetContainer : public UObject
{
	GENERATED_BODY()

	friend struct UE::TraceTools::FFilterPresetHelpers;
public:
	void GetSharedUserPresets(TArray<TSharedPtr<UE::TraceTools::ITraceFilterPreset>>& OutPresets);

	static void AddFilterData(const FTraceFilterData& InFilterData);
	static bool RemoveFilterData(const FTraceFilterData& InFilterData);
	static void Save();
protected:
	UPROPERTY(Config)
	TArray<FTraceFilterData> SharedPresets;
};

UCLASS(Config = Trace, DefaultConfig)
class UEngineTraceFilterPresetContainer : public UObject
{
	GENERATED_BODY()

	friend struct UE::TraceTools::FFilterPresetHelpers;
public:
	void GetEnginePresets(TArray<TSharedPtr<UE::TraceTools::ITraceFilterPreset>>& OutPresets);
protected:
	UPROPERTY(Config)
	TArray<FTraceFilterData> EnginePresets;
};

namespace UE::TraceTools
{

/** Base implementation of a filter preset */
struct FFilterPreset : public TraceTools::ITraceFilterPreset
{
public:
	FFilterPreset(const FString& InName, FTraceFilterData& InFilterData) : Name(InName), FilterData(InFilterData) {}

	/** Begin ITraceFilterPreset overrides */
	virtual FString GetName() const override;
	virtual FText GetDisplayText() const;
	virtual FText GetDescription() const;
	virtual void GetAllowlistedNames(TArray<FString>& OutNames) const override;
	virtual bool CanDelete() const override;
	virtual void Rename(const FString& InNewName) override;
	virtual bool Delete() override;
	virtual bool MakeShared() override;
	virtual bool MakeLocal() override;
	virtual bool IsLocal() const override;
	virtual void Save(const TArray<TSharedPtr<TraceTools::ITraceObject>>& InObjects) override {}
	virtual void Save() override {}
	/** End ITraceFilterPreset overrides */
protected:
	FString Name;
	FTraceFilterData& FilterData;
};

/** Engine level preset is simply a basic one */
typedef FFilterPreset FEngineFilterPreset;

/** User filter preset, allows for deletion / transitioning INI ownership */
struct FUserFilterPreset : public FFilterPreset
{
public:
	FUserFilterPreset(const FString& InName, FTraceFilterData& InFilterData, bool bInLocal = false) : FFilterPreset(InName, InFilterData), bIsLocalPreset(bInLocal) {}

	/** Begin ITraceFilterPreset overrides */
	virtual bool CanDelete() const override;
	virtual bool Delete() override;
	virtual bool MakeShared() override;
	virtual bool MakeLocal() override;
	virtual bool IsLocal() const override;
	virtual void Save(const TArray<TSharedPtr<TraceTools::ITraceObject>>& InObjects) override;
	virtual void Save() override;
	/** End ITraceFilterPreset overrides */
protected:
	bool bIsLocalPreset;
};

} // namespace UE::TraceTools