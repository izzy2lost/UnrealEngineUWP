// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Union.h"
#include "Engine/Engine.h"
#include "Logging/LogMacros.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureStateChangeObserver.h"

#include "GameFeatureSetSubsystem.generated.h"

GAMEFEATURESET_API DECLARE_LOG_CATEGORY_EXTERN(LogGameFeatureSet, Log, All);

/** Notification that a game feature plugin install/register/load/unload has finished */
DECLARE_DELEGATE_OneParam(FGameFeatureSetLoadComplete, bool);

/** structure to inform unload process about an upcoming game feature plugin change */
struct FGameFeatureSetPluginChange
{
	FString PluginName;
	FString PluginURL;
	bool bIsGameFeaturePlugin = false;
	EGameFeatureTargetState TargetState = EGameFeatureTargetState::Installed;
};

/** delegate that will be called when feature set plugin changes are about to begin. */
DECLARE_DELEGATE_OneParam(FGameFeatureSetUnloadBegin, const TConstArrayView<FGameFeatureSetPluginChange>& /* PluginChanges */);
/** delegate that will be called once GFS unload is completed. */
DECLARE_DELEGATE_OneParam(FGameFeatureSetUnloadComplete, bool /* bSuccess */);

/** The manager subsystem for game feature sets */
UCLASS()
class GAMEFEATURESET_API UGameFeatureSetSubsystem : public UEngineSubsystem, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	//~UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of UEngineSubsystem interface

	//~IGameFeatureStateChangeObserver interface
	virtual void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL) override;
	//~End of IGameFeatureStateChangeObserver interface

	// singleton access
	static UGameFeatureSetSubsystem& Get() { return *GEngine->GetEngineSubsystem<UGameFeatureSetSubsystem>(); }

	struct FIsFeatureSet
	{
		explicit operator bool() const
		{
			return bIsFeatureSet;
		}

		bool bUnknownPlugin = true;
		bool bIsFeatureSet = false;
		bool bCanAccess = false;
	};

	// check whether or not (builtin) plugin is a feature set plugin
	FIsFeatureSet GetIsFeatureSetPlugin(const FString& PluginName) const;
	// check whether or not (builtin) plugin is a feature set plugin
	static bool GetIsFeatureSetPlugin(const FGameFeaturePluginDetails& PluginDetails);

	// load (and activate) a game feature set
	void LoadFeatureSetPlugin(const FString& PluginName, const FGameFeatureSetLoadComplete& CompleteDelegate);

	// parameters for feature set unload call
	struct FUnloadParam
	{
		FGameFeatureSetUnloadBegin UnloadBegin;
		FGameFeatureSetUnloadComplete UnloadComplete;
	};
	// unload a previously loaded feature set
	void UnloadFeatureSetPlugin(const FString& PluginName, const FUnloadParam& Param);

private:

#if !UE_BUILD_SHIPPING
	void VerifyFeatureSet(const FString& PluginName) const;
#endif

	struct FFeatureSetPlugin
	{
		uint32 RefCount = 0;
		FString PluginName;
		uint32 RegisterOrder = 0;
		bool bIsGameFeaturePlugin = false;
		bool bIsMissing = false;
		TOptional<EGameFeatureTargetState> PreviousState;

	};

	TArray<FFeatureSetPlugin> CollectDependencies(const FString& PluginName) const;
	static TOptional<EGameFeatureTargetState> GetGameFeatureState(const FString& PluginURL);

	struct FFeatureSet
	{
		uint32 RefCount = 0;
		FString PluginName;
		FString PluginURL;
		bool bIsLoading = false;
		TArray<FString> DependentPlugins;
		TArray<FGameFeatureSetLoadComplete> OnLoadComplete;
	};

	TMap<FString, TSharedRef<FFeatureSet>> FeatureSets;
	TMap<FString, TSharedRef<FFeatureSetPlugin>> FeatureSetPlugins;
	uint32 NextRegisterOrder = 0;
};
