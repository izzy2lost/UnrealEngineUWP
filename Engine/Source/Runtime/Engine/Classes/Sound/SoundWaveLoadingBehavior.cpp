// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundWaveLoadingBehavior.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Interfaces/ITargetPlatform.h"
#include "SoundClass.h"
#include "SoundCue.h"
#include "SoundWave.h"

const TCHAR* EnumToString(ESoundWaveLoadingBehavior InCurrentState)
{
	switch (InCurrentState)
	{
	case ESoundWaveLoadingBehavior::Inherited:
		return TEXT("Inherited");
	case ESoundWaveLoadingBehavior::RetainOnLoad:
		return TEXT("RetainOnLoad");
	case ESoundWaveLoadingBehavior::PrimeOnLoad:
		return TEXT("PrimeOnLoad");
	case ESoundWaveLoadingBehavior::LoadOnDemand:
		return TEXT("LoadOnDemand");
	case ESoundWaveLoadingBehavior::ForceInline:
		return TEXT("ForceInline");
	case ESoundWaveLoadingBehavior::Uninitialized:
		return TEXT("Uninitialized");
	}
	ensure(false);
	return TEXT("Unknown");
}


#if WITH_EDITOR

static int32 SoundWaveLoadingBehaviorUtil_CacheAllOnStartup = 0;
FAutoConsoleVariableRef CVAR_SoundWaveLoadingBehaviorUtil_CacheAllOnStartup(
	TEXT("au.editor.SoundWaveOwnerLoadingBehaviorCacheOnStartup"),
	SoundWaveLoadingBehaviorUtil_CacheAllOnStartup,
	TEXT("Disables searching the asset registry on startup of the singleton. Otherwise it will incrementally fill cache"),
	ECVF_Default
);

static int32 SoundWaveLoadingBehaviorUtil_Enable = 1;
FAutoConsoleVariableRef CVAR_SoundWaveLoadingBehaviorUtil_Enable(
	TEXT("au.editor.SoundWaveOwnerLoadingBehaviorEnable"),
	SoundWaveLoadingBehaviorUtil_Enable,
	TEXT("Enables or disables the Soundwave owner loading behavior tagging"),
	ECVF_Default
);

class FSoundWaveLoadingBehaviorUtil : public ISoundWaveLoadingBehaviorUtil
{
public:
	FSoundWaveLoadingBehaviorUtil()	
		: AssetRegistry(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	{
		if (SoundWaveLoadingBehaviorUtil_CacheAllOnStartup)
		{
			CacheAllClassLoadingBehaviors();
		}
	}
	virtual ~FSoundWaveLoadingBehaviorUtil() override = default;

private:
	void CacheAllClassLoadingBehaviors()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LoadAndCacheAllSoundClassLoadingBehaviors);

		ensureMsgf(!AssetRegistry.IsSearchAsync() || !AssetRegistry.IsLoadingAssets(), 
		           TEXT("Function must not be called until after cook has started and waited on the AssetRegistry already."));
		
		AssetRegistry.SearchAllAssets(true);

		TArray<FAssetData> SoundClasses;
		AssetRegistry.GetAssetsByClass(USoundClass::StaticClass()->GetClassPathName(), SoundClasses, true);
		
		for (const FAssetData& i : SoundClasses)
		{
			LoadAndCacheClass(i);
		}
	}

	FClassData WalkClassHierarchy(USoundClass* InClass) const
	{
		FClassData Behavior(InClass);
		while (InClass->ParentClass && InClass->Properties.LoadingBehavior == ESoundWaveLoadingBehavior::Inherited)
		{
			InClass = InClass->ParentClass;
			if (InClass->HasAnyFlags(RF_NeedLoad))
			{
				InClass->GetLinker()->Preload(InClass);
			}
			if (InClass->HasAnyFlags(RF_NeedPostLoad))
			{
				InClass->ConditionalPostLoad();
			}
			Behavior = FClassData(InClass);
		}
		
		// If we failed to find anything other than inherited, use the default which is cvar'd.
		if (Behavior.LoadingBehavior == ESoundWaveLoadingBehavior::Inherited ||
			Behavior.LoadingBehavior == ESoundWaveLoadingBehavior::Uninitialized )
		{
			Behavior.LoadingBehavior = USoundWave::GetDefaultLoadingBehavior();
			Behavior.LengthOfFirstChunkInSeconds = 0;
		}
		return Behavior;
	}

	FClassData LoadAndCacheClass(const FAssetData& InAssetData) const
	{
		if (USoundClass* SoundClass = Cast<USoundClass>(InAssetData.GetAsset()))
		{
			FClassData Result = WalkClassHierarchy(SoundClass);
			CacheClassLoadingBehaviors.Add(InAssetData.PackageName,Result);
			return Result;
		}
		return {};
	}
	
	virtual FClassData FindOwningLoadingBehavior(const USoundWave* InWave, const ITargetPlatform* InTargetPlatform) const 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOwningLoadingBehavior);

		// This code: Given a wave, finds all cues that references it. (reverse lookup)
		// Then finds the SoundClasses those cues use, traverses the heirarchy (from lookup) to determine the loading behavior.
		// Then stack ranks the most important behavior. (RetainOnLoad (Highest), PrimeOnLoad (Medium), LoadOnDemand (Lowest))
		// Which ever wins, we also capture the "SizeOfFirstChunk" to use for that wave.

		if (!InWave)
		{
			return {};
		}
		
		const UPackage* WavePackage = InWave->GetPackage();
		if (!WavePackage)
		{
			return {};
		}

		TArray<FName> SoundWaveReferencerNames;
		if (!AssetRegistry.GetReferencers(WavePackage->GetFName(), SoundWaveReferencerNames))
		{
			return {};
		}

		if (SoundWaveReferencerNames.IsEmpty())
		{
			return {};
		}

		// Filter on SoundCues.
		FARFilter Filter;
		Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackageNames = SoundWaveReferencerNames;
		TArray<FAssetData> ReferencingSoundCueAssetDataArray;
		if (!AssetRegistry.GetAssets(Filter, ReferencingSoundCueAssetDataArray))
		{
			return {};
		}

		if (ReferencingSoundCueAssetDataArray.IsEmpty())
		{
			return {};
		}

		FClassData MostImportantLoadingBehavior;
		for (const FAssetData& CueAsset : ReferencingSoundCueAssetDataArray)
		{
			// Query for class references from the Cue instead of loading and opening it.
			TArray<FName> SoundCueReferences;
			if (!AssetRegistry.GetDependencies(CueAsset.PackageName, SoundCueReferences))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to query SoundCue '%s' for it's dependencies."), 
				       *CueAsset.PackagePath.ToString() );
				continue;
			}

			if (SoundCueReferences.Num() == 0)
			{
				continue;
			}

			// Filter for Classes.
			FARFilter ClassFilter;
			ClassFilter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
			ClassFilter.PackageNames = SoundCueReferences;
			TArray<FAssetData> ReferencedSoundClasses;
			if (!AssetRegistry.GetAssets(ClassFilter, ReferencedSoundClasses))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to filter for Soundclasses from the SoundCue dependencies for '%s'"), *CueAsset.PackagePath.ToString());
				continue;
			}			

			// Should have a single SoundClass, we hope, otherwise ignore.
			if (ReferencedSoundClasses.Num() != 1)
			{
				UE_CLOG(ReferencedSoundClasses.Num() > 1, LogAudio, Warning, 
				        TEXT("More than one Soundclass refereneced from this cue '%s'"), *CueAsset.PackagePath.ToString() );
				continue;
			}
			
			// Look up this classes loading behavior in our cache.
			const FAssetData& Class = ReferencedSoundClasses[0];
			FClassData CacheLoadingBehavior;
			{
				FScopeLock Lock(&CacheCS);
				if (const FClassData* Found = CacheClassLoadingBehaviors.Find(Class.PackageName))
				{
					CacheLoadingBehavior = *Found;
				}
				else
				{
					CacheLoadingBehavior = LoadAndCacheClass(Class);
				}
			}

			// Compare if this is more important
			if (MostImportantLoadingBehavior.CompareGreater(CacheLoadingBehavior, InTargetPlatform))
			{
				MostImportantLoadingBehavior = CacheLoadingBehavior;
			}
		}
		
		// Return the most important one we found.
		return MostImportantLoadingBehavior;
	}
	
	// Make cache here.
	IAssetRegistry& AssetRegistry;
	mutable TMap<FName, FClassData> CacheClassLoadingBehaviors;
	mutable FCriticalSection CacheCS;
};

ISoundWaveLoadingBehaviorUtil* ISoundWaveLoadingBehaviorUtil::Get()
{
	// Cvar disable system if necessary
	if (!SoundWaveLoadingBehaviorUtil_Enable)
	{
		return nullptr;
	}
	
	static bool bAllowOutsideOfCookCommandlet = FParse::Param(FCommandLine::Get(), TEXT("AllowSoundWaveOwnerLoadingBehaviorInEditor"));
	
	// Only run while the cooker is active.
	if (!IsRunningCookCommandlet() && !bAllowOutsideOfCookCommandlet)
	{
		return nullptr;
	}
	
	static FSoundWaveLoadingBehaviorUtil Instance;
	return &Instance;
}

ISoundWaveLoadingBehaviorUtil::FClassData::FClassData(const USoundClass* InClass)
	: LoadingBehavior(InClass->Properties.LoadingBehavior)
	, LengthOfFirstChunkInSeconds(InClass->Properties.SizeOfFirstAudioChunkInSeconds)
{
}

bool ISoundWaveLoadingBehaviorUtil::FClassData::CompareGreater(const FClassData& InOther, const ITargetPlatform* InPlatform) const
{
	if (InOther.LoadingBehavior != LoadingBehavior)
	{
		// If loading behavior enum is less, it's more important.
		return InOther.LoadingBehavior < LoadingBehavior;
	}
	else 
	{
		// If we are using Retain, use one with the higher Length.
		if (LoadingBehavior == ESoundWaveLoadingBehavior::RetainOnLoad)
		{
			const float Length = LengthOfFirstChunkInSeconds.GetValueForPlatform(*InPlatform->PlatformName());
			const float OtherLength = InOther.LengthOfFirstChunkInSeconds.GetValueForPlatform(*InPlatform->PlatformName());
				
			if (OtherLength > Length)
			{
				return true;
			}
		}
		return false;
	}
}

#endif //WITH_EDITOR
