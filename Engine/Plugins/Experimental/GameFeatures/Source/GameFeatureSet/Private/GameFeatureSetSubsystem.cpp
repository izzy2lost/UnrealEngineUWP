// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureSetSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesProjectPolicies.h"
#include "GameFeaturePluginOperationResult.h"
#include "Containers/Queue.h"
#include "Dom/JsonObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureSetSubsystem)

DEFINE_LOG_CATEGORY(LogGameFeatureSet);

void UGameFeatureSetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGameFeaturesSubsystem::Get().AddObserver(this);
}

void UGameFeatureSetSubsystem::Deinitialize()
{
	UGameFeaturesSubsystem::Get().RemoveObserver(this);
}

UGameFeatureSetSubsystem::FIsFeatureSet UGameFeatureSetSubsystem::GetIsFeatureSetPlugin(const FString& PluginName) const
{
	FIsFeatureSet Result;
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		Result.bUnknownPlugin = false;
		FString PluginURL;
		FGameFeaturePluginDetails PluginDetails;
		if (UGameFeaturesSubsystem::Get().GetBuiltInGameFeaturePluginDetails(Plugin.ToSharedRef(), PluginURL, PluginDetails))
		{
			Result.bIsFeatureSet = GetIsFeatureSetPlugin(PluginDetails);
			Result.bCanAccess = Result.bIsFeatureSet && UGameFeaturesSubsystem::Get().GetPolicy().IsPluginAllowed(PluginURL);
		}
	}
	return Result;
}

bool UGameFeatureSetSubsystem::GetIsFeatureSetPlugin(const FGameFeaturePluginDetails& PluginDetails)
{
	static const FString FeatureSetKey = TEXT("IsGameFeatureSet");
	if (TSharedPtr<class FJsonValue> FeatureSetField = PluginDetails.AdditionalMetadata.FindRef(FeatureSetKey))
	{
		bool bIsFeatureSet;
		if (FeatureSetField->TryGetBool(bIsFeatureSet))
		{
			return bIsFeatureSet;
		}
	}
	return false;
}

void UGameFeatureSetSubsystem::LoadFeatureSetPlugin(const FString& PluginName, const FGameFeatureSetLoadComplete& CompleteDelegate)
{
	// already loaded?
	if (const TSharedRef<FFeatureSet>* FeatureSet = FeatureSets.Find(PluginName))
	{
		(*FeatureSet)->RefCount += 1;
		// (re-) loading feature set while loading still in progress?
		if (ensure(!(*FeatureSet)->bIsLoading))
		{
			UE_LOG(LogGameFeatureSet, Log, TEXT("Feature set '%s' is already loaded."), *PluginName);
			CompleteDelegate.ExecuteIfBound(true);
		}
		else
		{
			UE_LOG(LogGameFeatureSet, Log, TEXT("Feature set '%s' is currently loading - postponing complete callback."), *PluginName);
			(*FeatureSet)->OnLoadComplete.Add(CompleteDelegate);
		}
		return;
	}

	UE_LOG(LogGameFeatureSet, Log, TEXT("Loading game feature set '%s'."), *PluginName);

#if !UE_BUILD_SHIPPING
	VerifyFeatureSet(PluginName);
#endif

	TSharedRef<FFeatureSet> FeatureSet = MakeShared<FFeatureSet>();
	FeatureSet->PluginName = PluginName;
	FeatureSet->bIsLoading = true;
	FeatureSet->RefCount = 1;
	FeatureSet->OnLoadComplete.Add(CompleteDelegate);
	// is feature set a known game feature plugin
	if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, FeatureSet->PluginURL))
	{
		UE_LOG(LogGameFeatureSet, Error, TEXT("Unknown game feature set '%s'."), *PluginName);
		CompleteDelegate.ExecuteIfBound(false);
		return;
	}
	// collect dependencies of plugin
	TArray<UGameFeatureSetSubsystem::FFeatureSetPlugin> Dependecies = CollectDependencies(PluginName);
	// any of the dependencies missing (e.g. no entitlements)?
	TArray<FString> MissingPlugins;
	Algo::TransformIf(Dependecies, MissingPlugins, [](const FFeatureSetPlugin& In) { return In.bIsMissing; }, [](const FFeatureSetPlugin& In) { return In.PluginName; });
	if (!MissingPlugins.IsEmpty())
	{
		UE_LOG(LogGameFeatureSet, Error, TEXT("Game feature set '%s' is missing access to '%s'"), *PluginName, *FString::Join(MissingPlugins, TEXT("', '")));
		CompleteDelegate.ExecuteIfBound(false);
		return;
	}
	Algo::Transform(Dependecies, FeatureSet->DependentPlugins, [](const FFeatureSetPlugin& In) { return In.PluginName; });

	FeatureSets.Emplace(PluginName, FeatureSet);

	// add/increase reference count of dependencies
	for (const FFeatureSetPlugin& Plugin : Dependecies)
	{
		if (const TSharedRef<FFeatureSetPlugin>* FeatureSetPlugin = FeatureSetPlugins.Find(Plugin.PluginName))
		{
			(*FeatureSetPlugin)->RefCount += 1;
			continue;
		}
		// new plugin
		FeatureSetPlugins.Emplace(Plugin.PluginName, MakeShared<FFeatureSetPlugin>(Plugin))->RefCount += 1;
	}

	UGameFeaturesSubsystem::Get().ChangeGameFeatureTargetState(FeatureSet->PluginURL, EGameFeatureTargetState::Active,
		FGameFeaturePluginChangeStateComplete::CreateWeakLambda(this, [this, FeatureSet, CompleteDelegate](const UE::GameFeatures::FResult& Result)
		{
			TArray<FGameFeatureSetLoadComplete> OnLoadComplete = MoveTemp(FeatureSet->OnLoadComplete);
			const bool bSuccess = !Result.HasError();
			if (ensure(FeatureSet->bIsLoading))
			{
				FeatureSet->bIsLoading = false;
				FeatureSet->DependentPlugins.Sort([this](const FString& A, const FString& B)
					{
						const TSharedRef<FFeatureSetPlugin>* PluginA = FeatureSetPlugins.Find(A);
						const TSharedRef<FFeatureSetPlugin>* PluginB = FeatureSetPlugins.Find(B);
						if (ensure(PluginA != nullptr && PluginB != nullptr))
						{
							return (*PluginA)->RegisterOrder > (*PluginB)->RegisterOrder;
						}
						return false;
					});
			}
			if (bSuccess)
			{
				UE_LOG(LogGameFeatureSet, Warning, TEXT("Completed loading GameFeatureSet '%s': '%s'"), *FeatureSet->PluginName, Result.HasError() ? *Result.GetError() : TEXT("ok"));
			}
			else
			{
				UE_LOG(LogGameFeatureSet, Warning, TEXT("Failed to load GameFeatureSet '%s': '%s'"), *FeatureSet->PluginName, Result.HasError() ? *Result.GetError() : TEXT("ok"));
			}
			for (const FGameFeatureSetLoadComplete& Delegate : OnLoadComplete)
			{
				Delegate.ExecuteIfBound(bSuccess);
			}
		}));
}


void UGameFeatureSetSubsystem::UnloadFeatureSetPlugin(const FString& PluginName, const FUnloadParam& Param)
{
	if (!FeatureSets.Contains(PluginName))
	{
		UE_LOG(LogGameFeatureSet, Error, TEXT("Game feature set '%s' is not loaded"), *PluginName);
		Param.UnloadComplete.ExecuteIfBound(false);
		return;
	}
	TSharedRef<FFeatureSet> FeatureSet = FeatureSets[PluginName];
	if (--FeatureSet->RefCount > 0)
	{
		Param.UnloadComplete.ExecuteIfBound(true);
		return;
	}

	IPluginManager& PluginManager = IPluginManager::Get();
	UGameFeaturesSubsystem& GameFeatures = UGameFeaturesSubsystem::Get();

	struct FRestorePluginStateContext : public TSharedFromThis<FRestorePluginStateContext>
	{
		explicit FRestorePluginStateContext(TSharedRef<FFeatureSet> InFeatureSet) : FeatureSet(InFeatureSet) {}

		TSharedRef<FFeatureSet> FeatureSet;
		FUnloadParam Param;
		// TArray<TPair<FString, EGameFeatureTargetState>> ChangeGameFeatureStates;
		TArray<FGameFeatureSetPluginChange> PluginStateChanges;

		int32 ChangeGameFeatureStateIndex = 0;
		bool bAnyErrors = false;

		void RestoreNext()
		{
			if (ChangeGameFeatureStateIndex < PluginStateChanges.Num())
			{
				const FString PluginURL = PluginStateChanges[ChangeGameFeatureStateIndex].PluginURL;
				const EGameFeatureTargetState TargetState = PluginStateChanges[ChangeGameFeatureStateIndex].TargetState;
				++ChangeGameFeatureStateIndex;
				UGameFeaturesSubsystem::Get().ChangeGameFeatureTargetState(PluginURL, TargetState,
					FGameFeaturePluginChangeStateComplete::CreateLambda([Self = AsShared(), PluginURL](const UE::GameFeatures::FResult& Result)
					{
						if (Result.HasError())
						{
							UE_LOG(LogGameFeatureSet, Warning, TEXT("Failed to unload GameFeature '%s' ('%s'): '%s'"), *Self->FeatureSet->PluginName, *PluginURL, Result.HasError() ? *Result.GetError() : TEXT("ok"));
							Self->bAnyErrors = true;
						}
						else
						{
							UE_LOG(LogGameFeatureSet, Warning, TEXT("Unloaded plugin '%s' for GameFeatureSet '%s'"), *PluginURL, *Self->FeatureSet->PluginName);
						}
						Self->RestoreNext();
					}));
				return;
			}
			Param.UnloadComplete.ExecuteIfBound(!bAnyErrors);
		}
	};

	TSharedRef<FRestorePluginStateContext> RestoreContext = MakeShared<FRestorePluginStateContext>(FeatureSet);
	RestoreContext->Param = Param;

	for(const FString& FeatureSetPluginName : FeatureSet->DependentPlugins)
	{
		if (!ensure(FeatureSetPlugins.Contains(FeatureSetPluginName)))
		{
			continue;
		}
		TSharedRef<FFeatureSetPlugin> Plugin = FeatureSetPlugins[FeatureSetPluginName];
		if (--Plugin->RefCount > 0)
		{
			continue;
		}

		if (Plugin->bIsGameFeaturePlugin)
		{
			FString PluginURL;
			if (GameFeatures.GetPluginURLByName(Plugin->PluginName, PluginURL))
			{
				if (Plugin->PreviousState.IsSet())
				{
					TOptional<EGameFeatureTargetState> CurrentState = GetGameFeatureState(PluginURL);
					if (ensure(CurrentState.IsSet()) && CurrentState != Plugin->PreviousState)
					{
						FGameFeatureSetPluginChange& Change = RestoreContext->PluginStateChanges.Emplace_GetRef();
						Change.PluginName = Plugin->PluginName;
						Change.PluginURL = PluginURL;
						Change.bIsGameFeaturePlugin = true;
						Change.TargetState = Plugin->PreviousState.GetValue();
					}
				}
			}
		}
		else
		{
			// todo: regular plugin
		}
		FeatureSetPlugins.Remove(FeatureSetPluginName);
	}

	Param.UnloadBegin.ExecuteIfBound(RestoreContext->PluginStateChanges);
	FeatureSets.Remove(PluginName);

	RestoreContext->RestoreNext();
}


TArray<UGameFeatureSetSubsystem::FFeatureSetPlugin> UGameFeatureSetSubsystem::CollectDependencies(const FString& FeatureSetPluginName) const
{
	TArray<FFeatureSetPlugin> Result;

	IPluginManager& PluginManager = IPluginManager::Get();
	UGameFeaturesSubsystem& GameFeatures = UGameFeaturesSubsystem::Get();
	TQueue<FString> PluginsToCheck;
	TSet<FString> PluginsChecked;
	PluginsToCheck.Enqueue(FeatureSetPluginName);
	FString PluginName;
	while (PluginsToCheck.Dequeue(PluginName))
	{
		if (PluginsChecked.Contains(PluginName))
		{
			continue;
		}
		PluginsChecked.Add(PluginName);
		FFeatureSetPlugin& NewPlugin = Result.Emplace_GetRef();
		NewPlugin.PluginName = PluginName;
		if (TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName))
		{
			FString PluginURL;
			NewPlugin.bIsGameFeaturePlugin = GameFeatures.GetPluginURLByName(Plugin->GetName(), PluginURL);
			if (NewPlugin.bIsGameFeaturePlugin)
			{
				NewPlugin.PreviousState = GetGameFeatureState(PluginURL);
			}
			else
			{
			}

			for (const FPluginReferenceDescriptor& DepDescriptor : Plugin->GetDescriptor().Plugins)
			{
				if (DepDescriptor.bEnabled)
				{
					if (!PluginsChecked.Contains(DepDescriptor.Name))
					{
						PluginsToCheck.Enqueue(DepDescriptor.Name);
					}
				}
			}
		}
		else
		{
			NewPlugin.bIsMissing = true;
			// todo: error
			ensureAlwaysMsgf(false, TEXT("Plugin '%s' was not found!"), *PluginName);
		}
	}
	return Result;
}

TOptional<EGameFeatureTargetState> UGameFeatureSetSubsystem::GetGameFeatureState(const FString& PluginURL)
{
	UGameFeaturesSubsystem& GameFeatures = UGameFeaturesSubsystem::Get();
	if (GameFeatures.IsGameFeaturePluginActive(PluginURL))
	{
		return EGameFeatureTargetState::Active;
	}
	else if (GameFeatures.IsGameFeaturePluginLoaded(PluginURL))
	{
		return EGameFeatureTargetState::Loaded;
	}
	else if (GameFeatures.IsGameFeaturePluginRegistered(PluginURL))
	{
		return EGameFeatureTargetState::Registered;
	}
	else
	{
		return EGameFeatureTargetState::Installed;
	}
	return TOptional<EGameFeatureTargetState>();
}

#if !UE_BUILD_SHIPPING

void UGameFeatureSetSubsystem::VerifyFeatureSet(const FString& PluginName) const
{
	FString PluginURL;
	UGameFeaturesSubsystem& GameFeatures = UGameFeaturesSubsystem::Get();
	if(ensureAlwaysMsgf(GameFeatures.GetPluginURLByName(PluginName, PluginURL), TEXT("Plugin '%s' for feature set is unknown"), *PluginName))
	{
		TOptional<EGameFeatureTargetState> PluginState = GetGameFeatureState(PluginURL);
		ensureAlwaysMsgf(PluginState.IsSet() && PluginState.GetValue() == EGameFeatureTargetState::Installed, TEXT("Expected plugin '%s' for feature set to be in 'Installed' state."), *PluginName);

		IPluginManager& PluginManager = IPluginManager::Get();
		if (TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName))
		{
			for (const FPluginReferenceDescriptor& DepDescriptor : Plugin->GetDescriptor().Plugins)
			{
				if (!DepDescriptor.bEnabled)
				{
					continue;
				}
				if (FeatureSetPlugins.Contains(DepDescriptor.Name))
				{
					continue;
				}
				if (GameFeatures.GetPluginURLByName(DepDescriptor.Name, PluginURL))
				{
					PluginState = GetGameFeatureState(PluginURL);
					ensureAlwaysMsgf(PluginState.IsSet() && PluginState.GetValue() == EGameFeatureTargetState::Installed, TEXT("Expected plugin '%s' for feature set '%s' to be in 'Installed' state."), *DepDescriptor.Name, *PluginName);
				}
			}
		}
	}
}

#endif

void UGameFeatureSetSubsystem::OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	if (TSharedRef<FFeatureSetPlugin>* Plugin = FeatureSetPlugins.Find(PluginName))
	{
		(*Plugin)->RegisterOrder = ++NextRegisterOrder;
	}
}
