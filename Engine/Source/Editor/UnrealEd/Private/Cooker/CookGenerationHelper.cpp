// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookGenerationHelper.h"

#include "Algo/Unique.h"
#include "AssetRegistry/AssetData.h"
#include "Cooker/CookDirector.h"
#include "Cooker/IWorkerRequests.h"
#include "Cooker/PackageTracker.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "UObject/ReferenceChainSearch.h"

namespace UE::Cook
{

//////////////////////////////////////////////////////////////////////////
// FGenerationHelper

FGenerationHelper::FGenerationHelper(UE::Cook::FPackageData& InOwner)
: OwnerInfo(InOwner, true /* bInGenerator */)
{
}

FGenerationHelper::~FGenerationHelper()
{
	NotifyCompletion(ICookPackageSplitter::ETeardown::Complete);
	GetOwner().OnGenerationHelperDestroyed(*this);
}

void FGenerationHelper::NotifyCompletion(ICookPackageSplitter::ETeardown Status)
{
	if (IsInitialized() && IsValid() && CookPackageSplitterInstance)
	{
		CookPackageSplitterInstance->Teardown(Status);
		CookPackageSplitterInstance.Reset();
	}
}

void FGenerationHelper::Initialize()
{
	if (InitializeStatus != EInitializeStatus::Uninitialized)
	{
		return;
	}

	FPackageData& OwnerPackageData = GetOwner();
	FName OwnerPackageName = OwnerPackageData.GetPackageName();
	UCookOnTheFlyServer& COTFS = OwnerPackageData.GetPackageDatas().GetCookOnTheFlyServer();
	UPackage* LocalOwnerPackage = FindOrLoadPackage(COTFS, OwnerPackageData);
	if (!LocalOwnerPackage)
	{
		InitializeStatus = EInitializeStatus::Invalid;
		return;
	}

	UObject* LocalSplitDataObject;
	UE::Cook::Private::FRegisteredCookPackageSplitter* LocalRegisteredSplitterType = nullptr;
	TUniquePtr<ICookPackageSplitter> LocalSplitter;
	SearchForRegisteredSplitDataObject(COTFS, OwnerPackageName, LocalOwnerPackage,
		TOptional<TConstArrayView<FCachedObjectInOuter>>(), LocalSplitDataObject, LocalRegisteredSplitterType,
		LocalSplitter);
	if (!LocalSplitDataObject || !LocalSplitter)
	{
		InitializeStatus = EInitializeStatus::Invalid;
		return;
	}

	Initialize(LocalSplitDataObject, LocalRegisteredSplitterType, MoveTemp(LocalSplitter));
}

void FGenerationHelper::Initialize(const UObject* InSplitDataObject,
	UE::Cook::Private::FRegisteredCookPackageSplitter* InRegisteredSplitterType,
	TUniquePtr<ICookPackageSplitter>&& InCookPackageSplitterInstance)
{
	check(InSplitDataObject);
	if (InitializeStatus != EInitializeStatus::Uninitialized)
	{
		// If we already have a splitter, keep the old and throw out the new. The old one
		// still contains some state.
		return;
	}

	RegisteredSplitterType = InRegisteredSplitterType;
	CookPackageSplitterInstance = MoveTemp(InCookPackageSplitterInstance);
	InitializeStatus = EInitializeStatus::Valid;

	SplitDataObject = InSplitDataObject;
	SplitDataObjectName = FName(FStringView(InSplitDataObject->GetFullName()));
	bUseInternalReferenceToAvoidGarbageCollect =
		CookPackageSplitterInstance->UseInternalReferenceToAvoidGarbageCollect();
	bNeedCachedPlatformDataBeforeSplit =
		CookPackageSplitterInstance->NeedCachedPlatformDataBeforeSplit();
}

void FGenerationHelper::InitializeAsInvalid()
{
	if (InitializeStatus != EInitializeStatus::Uninitialized)
	{
		return;
	}
	InitializeStatus = EInitializeStatus::Invalid;
}

UPackage* FGenerationHelper::FindOrLoadPackage(UCookOnTheFlyServer& COTFS, FPackageData& OwnerPackageData)
{
	// This is the static helper function on FGenerationHelper that loads the package for any FPackageData; for the
	// member variable function that uses the cached pointer, see FindOrLoadOwnerPackage.
	FName OwnerPackageName = OwnerPackageData.GetPackageName();
	UPackage* Result = FindObjectFast<UPackage>(nullptr, OwnerPackageName);

	if (!Result || !Result->IsFullyLoaded())
	{
		COTFS.LoadPackageForCooking(OwnerPackageData, Result);
		if (!Result || !Result->IsFullyLoaded())
		{
			return nullptr;
		}
	}
	return Result;
}

void FGenerationHelper::SearchForRegisteredSplitDataObject(UCookOnTheFlyServer& COTFS,
	FName PackageName, UPackage* Package,
	TOptional<TConstArrayView<UE::Cook::FCachedObjectInOuter>> CachedObjectsInOuter,
	UObject*& OutSplitDataObject, UE::Cook::Private::FRegisteredCookPackageSplitter*& OutRegisteredSplitter,
	TUniquePtr<ICookPackageSplitter>& OutSplitterInstance)
{
	OutSplitDataObject = nullptr;
	OutRegisteredSplitter = nullptr;
	OutSplitterInstance = nullptr;
	check(Package != nullptr || CachedObjectsInOuter.IsSet());

	UObject* LocalSplitDataObject = nullptr;
	Private::FRegisteredCookPackageSplitter* SplitterType = nullptr;
	TArray<Private::FRegisteredCookPackageSplitter*> FoundRegisteredSplitters;
	auto TryLookForSplitterOfObject =
		[&COTFS, PackageName, &FoundRegisteredSplitters, &SplitterType, &LocalSplitDataObject](UObject* Obj)
		{
			FoundRegisteredSplitters.Reset();
			COTFS.RegisteredSplitDataClasses.MultiFind(Obj->GetClass(), FoundRegisteredSplitters);

			for (Private::FRegisteredCookPackageSplitter* SplitterForObject : FoundRegisteredSplitters)
			{
				if (SplitterForObject && SplitterForObject->ShouldSplitPackage(Obj))
				{
					if (!Obj->HasAnyFlags(RF_Public))
					{
						UE_LOG(LogCook, Error,
							TEXT("SplitterData object %s must be publicly referenceable so we can keep them from being garbage collected"),
							*Obj->GetFullName());
						return false;
					}

					if (SplitterType)
					{
						UE_LOG(LogCook, Error,
							TEXT("Found more than one registered Cook Package Splitter for package %s."),
							*PackageName.ToString());
						return false;
					}

					SplitterType = SplitterForObject;
					LocalSplitDataObject = Obj;
				}
			}
			return true;
		};

	if (CachedObjectsInOuter.IsSet())
	{
		// CachedObjectsInOuter might be set but empty for e.g. a generated package that has not been populated
		for (const FCachedObjectInOuter& CachedObjectInOuter : *CachedObjectsInOuter)
		{
			UObject* Obj = CachedObjectInOuter.Object.Get();
			if (!Obj)
			{
				continue;
			}
			if (!TryLookForSplitterOfObject(Obj))
			{
				return; // error condition, exit the entire search function
			}
		}
	}
	else
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true /* bIncludeNestedObjects */,
			RF_NoFlags, EInternalObjectFlags::Garbage);
		for (UObject* Obj : ObjectsInPackage)
		{
			if (!TryLookForSplitterOfObject(Obj))
			{
				return; // error condition, exit the entire search function
			}
		}
	}

	if (!SplitterType)
	{
		return;
	}

	// Create instance of CookPackageSplitter class
	ICookPackageSplitter* SplitterInstance = SplitterType->CreateInstance(LocalSplitDataObject);
	if (!SplitterInstance)
	{
		UE_LOG(LogCook, Error, TEXT("Error instantiating Cook Package Splitter %s for object %s."),
			*SplitterType->GetSplitterDebugName(), *LocalSplitDataObject->GetFullName());
		return;
	}

	OutSplitDataObject = LocalSplitDataObject;
	OutRegisteredSplitter = SplitterType;
	OutSplitterInstance.Reset(SplitterInstance);
}

void FGenerationHelper::ClearSelfReferences()
{
	// Any references we release might be the last reference and cause *this to be deleted,
	// so create a local reference to keep it alive until the end of the function.
	TRefCountPtr<FGenerationHelper> LocalRef(this);
	ClearKeepForIterative();
	ClearKeepForGeneratorSave();
	ClearKeepForQueueResults();
}

FCookGenerationInfo* FGenerationHelper::FindInfo(const FPackageData& PackageData)
{
	ConditionalInitialize();

	if (&PackageData == &GetOwner())
	{
		return &OwnerInfo;
	}
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		if (Info.PackageData == &PackageData)
		{
			return &Info;
		}
	}
	return nullptr;
}

const FCookGenerationInfo* FGenerationHelper::FindInfo(const FPackageData& PackageData) const
{
	return const_cast<FGenerationHelper*>(this)->FindInfo(PackageData);
}

UObject* FGenerationHelper::FindOrLoadSplitDataObject()
{
	if (!IsValid())
	{
		return nullptr;
	}
	UObject* Result = SplitDataObject.Get();
	if (Result)
	{
		return Result;
	}

	FString ObjectPath = GetSplitDataObjectName().ToString();
	// SplitDataObjectName is a FullObjectPath; strip off the leading <ClassName> in
	// "<ClassName> <Package>.<Object>:<SubObject>"
	int32 ClassDelimiterIndex = -1;
	if (ObjectPath.FindChar(' ', ClassDelimiterIndex))
	{
		ObjectPath.RightChopInline(ClassDelimiterIndex + 1);
	}

	Result = FindObject<UObject>(nullptr, *ObjectPath);
	if (!Result)
	{
		FPackageData& OwnerPackageData = GetOwner();
		FName OwnerPackageName = OwnerPackageData.GetPackageName();
		UCookOnTheFlyServer& COTFS = OwnerPackageData.GetPackageDatas().GetCookOnTheFlyServer();
		UPackage* LocalOwnerPackage;
		COTFS.LoadPackageForCooking(OwnerPackageData, LocalOwnerPackage);

		Result = FindObject<UObject>(nullptr, *ObjectPath);
		if (!Result)
		{
			return nullptr;
		}
	}

	SplitDataObject = Result;
	return Result;
}

UPackage* FGenerationHelper::GetOwnerPackage()
{
	UPackage* Result = OwnerPackage.Get();
	if (!Result && !OwnerPackage.GetEvenIfUnreachable())
	{
		OwnerPackage = FindObjectFast<UPackage>(nullptr, GetOwner().GetPackageName());
		Result = OwnerPackage.Get();
	}
	return Result;
}

UPackage* FGenerationHelper::FindOrLoadOwnerPackage(UCookOnTheFlyServer& COTFS)
{
	UPackage* Result = GetOwnerPackage();
	if (!Result)
	{
		Result = FindOrLoadPackage(COTFS, GetOwner());
	}
	return Result;
}

bool FGenerationHelper::TryGenerateList()
{
	if (bGeneratedList)
	{
		return true;
	}
	FPackageData& OwnerPackageData = GetOwner();
	FName OwnerPackageName = OwnerPackageData.GetPackageName();
	if (!IsValid())
	{
		// Unexpected, caller should not call in this case
		UE_LOG(LogCook, Error, TEXT("TryGenerateList failed for package %s: Called on an invalid FGenerationHelper."),
			*OwnerPackageName.ToString());
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
		return false;
	}

	FPackageDatas& PackageDatas = OwnerPackageData.GetPackageDatas();
	UCookOnTheFlyServer& COTFS = PackageDatas.GetCookOnTheFlyServer();
	UObject* OwnerObject = FindOrLoadSplitDataObject();
	if (!OwnerObject)
	{
		// Unexpected, we found it earlier when we marked valid.
		UE_LOG(LogCook, Error, TEXT("TryGenerateList failed for package %s: Valid GenerationHelper but could not find OwnerObject."),
			*OwnerPackageName.ToString());
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
		return false;
	}

	UPackage* LocalOwnerPackage = OwnerObject->GetPackage();

	TArray<ICookPackageSplitter::FGeneratedPackage> GeneratorDatas;
	{
		UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, OwnerPackageName,
			PackageAccessTrackingOps::NAME_CookerBuildObject);
		GeneratorDatas = GetCookPackageSplitterInstance()->GetGenerateList(LocalOwnerPackage, OwnerObject);
	}
	PackagesToGenerate.Reset(GeneratorDatas.Num());
	TArray<const ITargetPlatform*, TInlineAllocator<1>> PlatformsToCook;
	OwnerPackageData.GetPlatformsNeedingCooking(PlatformsToCook);

	for (ICookPackageSplitter::FGeneratedPackage& SplitterData : GeneratorDatas)
	{
		if (!SplitterData.GetCreateAsMap().IsSet())
		{
			UE_LOG(LogCook, Error,
				TEXT("PackageSplitter did not specify whether CreateAsMap is true for generated package. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *OwnerPackageName.ToString());
			return false;
		}
		bool bCreateAsMap = *SplitterData.GetCreateAsMap();

		FString PackageRoot = SplitterData.GeneratedRootPath.IsEmpty() ?
			OwnerPackageName.ToString() : SplitterData.GeneratedRootPath;

		FString PackageName = FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("/%s/%s/%s"),
			*PackageRoot, GeneratedPackageSubPath, *SplitterData.RelativePath));
		const FName PackageFName(*PackageName);
		UE::Cook::FPackageData* PackageData = PackageDatas.TryAddPackageDataByPackageName(PackageFName,
			false /* bRequireExists */, bCreateAsMap);
		if (!PackageData)
		{
			UE_LOG(LogCook, Error,
				TEXT("PackageSplitter could not find mounted filename for generated packagepath. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *PackageName);
			return false;
		}
		// No package should be generated by two different splitters.
		check(PackageData->GetParentGenerator().IsNone() ||
			PackageData->GetParentGenerator() == OwnerPackageName);
		PackageData->SetGenerated(OwnerPackageName);
		PackageData->SetGeneratedNeedCachedPlatformDataBeforeSplit(bNeedCachedPlatformDataBeforeSplit);
		if (IFileManager::Get().FileExists(*PackageData->GetFileName().ToString()))
		{
			UE_LOG(LogCook, Warning,
				TEXT("PackageSplitter specified a generated package that already exists in the workspace domain. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *PackageName);
			return false;
		}

		FCookGenerationInfo& GeneratedInfo = PackagesToGenerate.Emplace_GetRef(*PackageData, false /* bInGenerator */);
		GeneratedInfo.RelativePath = MoveTemp(SplitterData.RelativePath);
		GeneratedInfo.GeneratedRootPath = MoveTemp(SplitterData.GeneratedRootPath);
		GeneratedInfo.PackageDependencies = MoveTemp(SplitterData.PackageDependencies);
		for (TArray<FAssetDependency>::TIterator Iter(GeneratedInfo.PackageDependencies); Iter; ++Iter)
		{
			if (Iter->Category != UE::AssetRegistry::EDependencyCategory::Package)
			{
				UE_LOG(LogCook, Error,
					TEXT("PackageSplitter specified a dependency with category %d rather than category Package. Dependency will be ignored. Splitter=%s, Generated=%s."),
					(int32)Iter->Category, *this->GetSplitDataObjectName().ToString(), *PackageName);
				Iter.RemoveCurrent();
			}
		}
		Algo::Sort(GeneratedInfo.PackageDependencies,
			[](const FAssetDependency& A, const FAssetDependency& B) { return A.LexicalLess(B); });
		GeneratedInfo.PackageDependencies.SetNum(Algo::Unique(GeneratedInfo.PackageDependencies));
		GeneratedInfo.SetIsCreateAsMap(bCreateAsMap);
		if (bNeedCachedPlatformDataBeforeSplit ||
			COTFS.MPCookGeneratorSplit == UE::Cook::EMPCookGeneratorSplit::AllOnSameWorker)
		{
			PackageData->SetWorkerAssignmentConstraint(FWorkerId::Local());
		}

		// Create the Hash from the GenerationHash and Dependencies
		GeneratedInfo.CreatePackageHash();
	}

	bGeneratedList = true;
	return true;
}

void FGenerationHelper::StartOwnerSave()
{
	if (!IsValid())
	{
		return;
	}
	UE_LOG(LogCook, Display, TEXT("Splitting Package %s with splitter %s acting on object %s."),
		*WriteToString<256>(GetOwner().GetPackageName()),
		*GetRegisteredSplitterType()->GetSplitterDebugName(),
		*WriteToString<256>(GetSplitDataObjectName()));
	SetKeepForGeneratorSave();
}

void FGenerationHelper::StartQueueGeneratedPackages(UCookOnTheFlyServer& COTFS)
{
	if (!IsValid())
	{
		return;
	}
	NotifyStartQueueGeneratedPackages(COTFS, FWorkerId::Local());

	bool bHybridIterativeEnabled = COTFS.bHybridIterativeEnabled;
	if (!PreviousGeneratedPackages.IsEmpty())
	{
		FPackageData& OwnerPackageData = GetOwner();
		TArray<const ITargetPlatform*, TInlineAllocator<1>> PlatformsToCook;
		OwnerPackageData.GetPlatformsNeedingCooking(PlatformsToCook);

		int32 NumIterativeUnmodified = 0;
		int32 NumIterativeModified = 0;
		int32 NumIterativeRemoved = 0;
		int32 NumIterativePrevious = PreviousGeneratedPackages.Num();

		for (FCookGenerationInfo& GeneratedInfo : PackagesToGenerate)
		{
			if (!GeneratedInfo.PackageData)
			{
				continue;
			}
			FIoHash PreviousHash;
			if (PreviousGeneratedPackages.RemoveAndCopyValue(GeneratedInfo.PackageData->GetPackageName(), PreviousHash)
				&& !bHybridIterativeEnabled)
			{
				bool bIterativelyUnmodified;
				GeneratedInfo.IterativeCookValidateOrClear(*this, PlatformsToCook, PreviousHash, bIterativelyUnmodified);
				++(bIterativelyUnmodified ? NumIterativeUnmodified : NumIterativeModified);
			}
		}
		if (!PreviousGeneratedPackages.IsEmpty())
		{
			NumIterativeRemoved = PreviousGeneratedPackages.Num();
			for (TPair<FName, FIoHash>& Pair : PreviousGeneratedPackages)
			{
				for (const ITargetPlatform* TargetPlatform : PlatformsToCook)
				{
					COTFS.DeleteOutputForPackage(Pair.Key, TargetPlatform);
				}
			}
			PreviousGeneratedPackages.Empty();
		}
		ClearKeepForIterative();

		if (NumIterativePrevious > 0 && !bHybridIterativeEnabled)
		{
			UE_LOG(LogCook, Display, TEXT("Found %d cooked package(s) in package store for generator package %s."),
				NumIterativePrevious, *WriteToString<256>(GetOwner().GetPackageName()));
			UE_LOG(LogCook, Display, TEXT("Keeping %d. Recooking %d. Removing %d."),
				NumIterativeUnmodified, NumIterativeModified, NumIterativeRemoved);
		}
	}
}

void FGenerationHelper::NotifyStartQueueGeneratedPackages(UCookOnTheFlyServer& COTFS, FWorkerId SourceWorkerId)
{
	// Note this function can be called on an uninitialized Generator; the generator is only needed
	// on the director so it can serve as the passer of messages. We have to keep ourselves referenced after
	// this call, until after we send EGeneratorEvent::QueuedGeneratedPackagesFencePassed, so that we don't destruct
	// and lose the WorkerIdThatSavedGenerator information.
	if (COTFS.CookDirector)
	{
		COTFS.PackageDatas->GetRequestQueue().AddRequestFenceListener(GetOwner().GetPackageName());
		WorkerIdThatSavedGenerator = SourceWorkerId;
	}
	SetKeepForQueueResults();
}

void FGenerationHelper::EndQueueGeneratedPackages(UCookOnTheFlyServer& COTFS)
{
	SetKeepForQueueResults();
	COTFS.WorkerRequests->EndQueueGeneratedPackages(COTFS, *this);
}

void FGenerationHelper::EndQueueGeneratedPackagesOnDirector(UCookOnTheFlyServer& COTFS, FWorkerId SourceWorkerId)
{
	// Note this function can be called on an uninitialized Generator; the generator is only needed
	// on the director so it can serve as the passer of messages.
	COTFS.PackageDatas->GetRequestQueue().AddRequestFenceListener(GetOwner().GetPackageName());
	SetKeepForQueueResults();

	// Setting the WorkerIdThatSavedGenerator in response to this event is usually not needed because it is also set
	// from the reported GeneratedPackages, but we set it anyway in case there is an edge condition that skips those
	// notifications.
	WorkerIdThatSavedGenerator = SourceWorkerId;
}

void FGenerationHelper::OnRequestFencePassedBroadcast(UCookOnTheFlyServer& COTFS)
{
	if (COTFS.CookDirector)
	{
		// Broadcast GenerationMessage API
		COTFS.CookDirector->BroadcastGeneratorFencePassed(*this);
	}
	OnRequestFencePassed(COTFS);
}

void FGenerationHelper::OnRequestFencePassed(UCookOnTheFlyServer& COTFS)
{
	ClearKeepForQueueResults();
	// We no longer need PreviousGeneratedPackages or KeepForIterative, because they are used (and cleared) in
	// StartQueueGeneratedPackages. Clear them as well on the director and any CookWorkers that received it to
	// free memory.
	ClearKeepForIterative();
	PreviousGeneratedPackages.Empty();
}

UPackage* FGenerationHelper::CreateGeneratedUPackage(FCookGenerationInfo& GeneratedInfo,
	const UPackage* InOwnerPackage, const TCHAR* GeneratedPackageName)
{
	if (!IsValid())
	{
		return nullptr;
	}

#if ENABLE_COOK_STATS
	++DetailedCookStats::NumRequestedLoads;
#endif
	UPackage* GeneratedPackage = CreatePackage(GeneratedPackageName);
	GeneratedPackage->SetSavedHash(GeneratedInfo.PackageHash);
	GeneratedPackage->SetPersistentGuid(InOwnerPackage->GetPersistentGuid());
	GeneratedPackage->SetPackageFlags(PKG_CookGenerated);
	GeneratedInfo.SetHasCreatedPackage(true);
	if (!InOwnerPackage->IsLoadedByEditorPropertiesOnly())
	{
		GeneratedPackage->SetLoadedByEditorPropertiesOnly(false);
	}

	return GeneratedPackage;
}

void FGenerationHelper::ResetSaveState(FCookGenerationInfo& Info, UPackage* Package,
	EStateChangeReason ReleaseSaveReason, EPackageState NewState)
{
	ConditionalInitialize();

	// We release references to *this in this function so keep a local reference to avoid deletion during the function.
	TRefCountPtr<FGenerationHelper> LocalRefCount = this;

	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		UObject* SplitObject = GetWeakSplitDataObject();
		UPackage* LocalOwnerPackage = Info.IsGenerator() ? Package : GetOwnerPackage();
		if (!SplitObject || !Package || !LocalOwnerPackage)
		{
			UE_LOG(LogCook, Warning,
				TEXT("PackageSplitter: %s on %s was GarbageCollected before we finished saving it. This prevents us from calling PostSave and may corrupt other packages that it altered during Populate. Splitter=%s."),
				(!Package ? TEXT("UPackage") :
					(!LocalOwnerPackage ? TEXT("ParentGenerator UPackage") : TEXT("SplitDataObject"))),
				Info.PackageData ? *Info.PackageData->GetPackageName().ToString() : *Info.RelativePath,
				*GetSplitDataObjectName().ToString());
		}
		else
		{
			UCookOnTheFlyServer& COTFS = GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
			UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, GetOwner().GetPackageName(),
				PackageAccessTrackingOps::NAME_CookerBuildObject);
			if (Info.IsGenerator())
			{
				GetCookPackageSplitterInstance()->PostSaveGeneratorPackage(Package, SplitObject);
			}
			else
			{
				ICookPackageSplitter::FGeneratedPackageForPopulate PopulateInfo;
				PopulateInfo.RelativePath = Info.RelativePath;
				PopulateInfo.GeneratedRootPath = Info.GeneratedRootPath;
				PopulateInfo.bCreatedAsMap = Info.IsCreateAsMap();
				PopulateInfo.Package = Package;
				GetCookPackageSplitterInstance()->PostSaveGeneratedPackage(LocalOwnerPackage, SplitObject,
					PopulateInfo);
			}
		}
	}

	if (ReleaseSaveReason == EStateChangeReason::RecreateObjectCache ||
		ReleaseSaveReason == EStateChangeReason::DoneForNow)
	{
		if (Info.IsGenerator())
		{
			if (Info.GetSaveState() >= FCookGenerationInfo::ESaveState::StartPopulate)
			{
				Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
			}
			else
			{
				// Redo all the steps since we didn't make it to the FinishCachePreObjectsToMove.
				// Restarting in the middle of that flow is not robust
				Info.SetSaveState(FCookGenerationInfo::ESaveState::StartSave);
			}
		}
		else
		{
			Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
		}
	}
	else
	{
		// The save is completed and we will not come back to it; set state back to initial
		// state and drop our reference keeping this GenerationHelper in memory for the save.
		if (ReleaseSaveReason == EStateChangeReason::Completed)
		{
			Info.SetHasSaved(true);
		}

		if (Info.IsGenerator())
		{
			ClearKeepForGeneratorSave();
			Info.SetSaveState(FCookGenerationInfo::ESaveState::StartSave);
		}
		else
		{
			Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
			if (Info.PackageData)
			{
				Info.PackageData->SetParentGenerationHelper(nullptr);
			}
		}
	}

	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		if (NewState != EPackageState::Idle && Info.PackageData &&
			Info.PackageData->GetCachedObjectsInOuter().Num() != 0 && IsUseInternalReferenceToAvoidGarbageCollect() &&
			(ReleaseSaveReason != EStateChangeReason::Completed && ReleaseSaveReason != EStateChangeReason::DoneForNow
				&& ReleaseSaveReason != EStateChangeReason::SaveError
				&& ReleaseSaveReason != EStateChangeReason::CookerShutdown))
		{
			UE_LOG(LogCook, Error,
				TEXT("CookPackageSplitter failure: We are demoting a %s package from save and removing our references that keep its objects loaded.\n")
				TEXT("This will allow the objects to be garbage collected and cause failures in the splitter which expects them to remain loaded.\n")
				TEXT("Package=%s, Splitter=%s, ReleaseSaveReason=%s"),
				Info.IsGenerator() ? TEXT("generator") : TEXT("generated"),
				Info.PackageData ? *Info.PackageData->GetPackageName().ToString() : *Info.RelativePath,
				*GetSplitDataObjectName().ToString(), LexToString(ReleaseSaveReason));
		}
		Info.CachedObjectsInOuterInfo.Empty();
		Info.SetHasTakenOverCachedCookedPlatformData(false);
	}
	Info.SetHasIssuedUndeclaredMovedObjectsWarning(false);
	Info.KeepReferencedPackages.Reset();
}

void FGenerationHelper::FetchExternalActorDependencies()
{
	if (!IsValid())
	{
		return;
	}

	// The Generator package declares all its ExternalActor dependencies in its AssetRegistry dependencies
	// The Generator's generated packages can also include ExternalActors from other maps due to level instancing,
	// these are included in the dependencies reported by the Generator for each GeneratedPackage in the data
	// returned from GetGenerateList. These sets will overlap; take the union.
	ExternalActorDependencies.Reset();
	IAssetRegistry::GetChecked().GetDependencies(GetOwner().GetPackageName(), ExternalActorDependencies,
		UE::AssetRegistry::EDependencyCategory::Package);
	for (const FCookGenerationInfo& Info : PackagesToGenerate)
	{
		ExternalActorDependencies.Reserve(Info.GetDependencies().Num() + ExternalActorDependencies.Num());
		for (const FAssetDependency& Dependency : Info.GetDependencies())
		{
			ExternalActorDependencies.Add(Dependency.AssetId.PackageName);
		}
	}
	Algo::Sort(ExternalActorDependencies, FNameFastLess());
	ExternalActorDependencies.SetNum(Algo::Unique(ExternalActorDependencies));
	FPackageDatas& PackageDatas = this->GetOwner().GetPackageDatas();
	FThreadSafeSet<FName>& NeverCookPackageList =
		GetOwner().GetPackageDatas().GetCookOnTheFlyServer().PackageTracker->NeverCookPackageList;

	// We are supposed to collect only ExternalActor dependencies, but we collected every dependency from the
	// generated packages. Remove the packages that are not external actors, which we detect by being on-disk
	// PackageDatas that are marked as NeverCook
	ExternalActorDependencies.RemoveAll([&PackageDatas, &NeverCookPackageList](FName PackageName)
		{
			FPackageData* PackageData = PackageDatas.TryAddPackageDataByPackageName(PackageName);
			if (!PackageData)
			{
				return true;
			}
			bool bIsNeverCook = NeverCookPackageList.Contains(PackageData->GetFileName());
			return !bIsNeverCook;
		});
	ExternalActorDependencies.Shrink();
}

void FGenerationHelper::SetPreviousGeneratedPackages(TMap<FName, FIoHash>&& Packages)
{
	SetKeepForIterative();
	PreviousGeneratedPackages = MoveTemp(Packages);
}

void FGenerationHelper::PreGarbageCollect(FCookGenerationInfo& Info, TArray<TObjectPtr<UObject>>& GCKeepObjects,
	TArray<UPackage*>& GCKeepPackages, TArray<FPackageData*>& GCKeepPackageDatas, bool& bOutShouldDemote)
{
	if (!IsInitialized())
	{
		return;
	}

	bOutShouldDemote = false;
	check(Info.PackageData); // Caller validates this is non-null
	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		if (IsUseInternalReferenceToAvoidGarbageCollect() || Info.PackageData->GetIsCookLast())
		{
			UPackage* Package = Info.PackageData->GetPackage();
			if (Package)
			{
				GCKeepPackages.Add(Package);
				GCKeepPackageDatas.Add(Info.PackageData);
			}
		}
		else
		{
			bOutShouldDemote = true;
		}
	}
	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		if (IsUseInternalReferenceToAvoidGarbageCollect())
		{
			// For the UseInternalReferenceToAvoidGarbageCollect case, part of the CookPackageSplitter contract is that
			// the Cooker will keep referenced the package and all objects returned from GetObjectsToMove* functions
			// until the PostSave function is called
			UPackage* Package = Info.PackageData->GetPackage();
			if (Package)
			{
				GCKeepPackages.Add(Package);
				GCKeepPackageDatas.Add(Info.PackageData);
			}
			GCKeepPackages.Append(Info.KeepReferencedPackages);
			for (FCachedObjectInOuter& CachedObjectInOuter : Info.PackageData->GetCachedObjectsInOuter())
			{
				UObject* Object = CachedObjectInOuter.Object.Get();
				if (Object)
				{
					GCKeepObjects.Add(Object);
				}
			}
		}
	}
}

void FGenerationHelper::PostGarbageCollect()
{
	if (!IsInitialized())
	{
		return;
	}

	FPackageData& Owner = GetOwner();
	if (Owner.GetState() == EPackageState::Save)
	{
		// UCookOnTheFlyServer::PreGarbageCollect adds references for the Generator package and all its public
		// objects, so it should still be loaded
		if (!Owner.GetPackage() || !GetWeakSplitDataObject())
		{
			UE_LOG(LogCook, Error,
				TEXT("PackageSplitter object was deleted by garbage collection while generation was still ongoing. This will break the generation.")
				TEXT("\n\tSplitter=%s."), *GetSplitDataObjectName().ToString());
		}
	}
	else
	{
		// After the Generator Package is saved, we drop our references to it and it can be garbage collected
		// If we have any packages left to populate, our splitter contract requires that it be garbage collected
		// because we promise that the package is not partially GC'd during calls to TryPopulateGeneratedPackage
		// The splitter can opt-out of this contract and keep it referenced itself if it desires.
		UPackage* LocalOwnerPackage = FindObject<UPackage>(nullptr, *Owner.GetPackageName().ToString());
		if (LocalOwnerPackage)
		{
			if (!Owner.IsInProgress() && !Owner.IsKeepReferencedDuringGC() &&
				!IsUseInternalReferenceToAvoidGarbageCollect())
			{
				UE_LOG(LogCook, Error,
					TEXT("PackageSplitter found the Generator package still in memory after it should have been deleted by GC.")
					TEXT("\n\tThis is unexpected since garbage has been collected and the package should have been unreferenced so it should have been collected, and will break population of Generated packages.")
					TEXT("\n\tSplitter=%s"), *GetSplitDataObjectName().ToString());
				EReferenceChainSearchMode SearchMode = EReferenceChainSearchMode::Shortest
					| EReferenceChainSearchMode::PrintAllResults
					| EReferenceChainSearchMode::FullChain;
				FReferenceChainSearch RefChainSearch(LocalOwnerPackage, SearchMode);
			}
		}
	}

	bool bHasIssuedWarning = false;
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		if (FindObject<UPackage>(nullptr, *Info.PackageData->GetPackageName().ToString()))
		{
			if (!Info.PackageData->IsKeepReferencedDuringGC() && !Info.HasSaved() && !bHasIssuedWarning)
			{
				UE_LOG(LogCook, Warning,
					TEXT("PackageSplitter found a package it generated that was not removed from memory during garbage collection. This will cause errors later during population.")
					TEXT("\n\tSplitter=%s, Generated=%s."), *GetSplitDataObjectName().ToString(),
					*Info.PackageData->GetPackageName().ToString());
				
				{
					// Compute UCookOnTheFlyServer's references so they are gathered by OBJ REFS below 
					UCookOnTheFlyServer::FScopeFindCookReferences(Owner.GetPackageDatas().GetCookOnTheFlyServer());

					StaticExec(nullptr, *FString::Printf(TEXT("OBJ REFS NAME=%s"),
						*Info.PackageData->GetPackageName().ToString()));
				}
				
				bHasIssuedWarning = true; // Only issue the warning once per GC
			}
		}
		else
		{
			Info.SetHasCreatedPackage(false);
		}
	}
}

void FGenerationHelper::UpdateSaveAfterGarbageCollect(const FPackageData& PackageData, bool& bInOutDemote)
{
	if (!IsInitialized())
	{
		return;
	}
	FCookGenerationInfo* Info = FindInfo(PackageData);
	if (!Info)
	{
		bInOutDemote = true;
		return;
	}

	if (!Info->IsGenerator())
	{
		UPackage* LocalPackage = OwnerPackage.Get();
		if (!LocalPackage || !LocalPackage->IsFullyLoaded())
		{
			bInOutDemote = true;
			return;
		}
	}

	if (bInOutDemote && IsUseInternalReferenceToAvoidGarbageCollect() && Info->HasTakenOverCachedCookedPlatformData())
	{
		// No public objects should have been deleted; we are supposed to keep them referenced by keeping the package
		// referenced in UCookOnTheFlyServer::PreGarbageCollect, and the package keeping its public objects referenced
		// by UPackage::AddReferencedObjects. Since no public objects were deleted, our caller should not have
		// set bInOutDemote=true.
		// Allowing demotion after the splitter has started moving objects breaks our contract with the splitter
		// and can cause a crash. So log this as an error.
		// For better feedback, look in our extra data to identify the name of the public UObject that was deleted.
		FString DeletedObject;
		if (!PackageData.GetPackage())
		{
			DeletedObject = FString::Printf(TEXT("UPackage %s"), *PackageData.GetPackageName().ToString());
		}
		else
		{
			TSet<UObject*> ExistingObjectsAfterSave;
			for (const FCachedObjectInOuter& CachedObjectInOuter : PackageData.GetCachedObjectsInOuter())
			{
				UObject* Ptr = CachedObjectInOuter.Object.Get();
				if (Ptr)
				{
					ExistingObjectsAfterSave.Add(Ptr);
				}
			}

			for (const TPair<UObject*, FCachedObjectInOuterGeneratorInfo>& Pair : Info->CachedObjectsInOuterInfo)
			{
				if (Pair.Value.bPublic && !ExistingObjectsAfterSave.Contains(Pair.Key))
				{
					DeletedObject = Pair.Value.FullName;
					break;
				}
			}
			if (DeletedObject.IsEmpty())
			{
				if (!PackageData.GetPackage()->IsFullyLoaded())
				{
					DeletedObject = FString::Printf(TEXT("UPackage %s is no longer FullyLoaded"),
						*PackageData.GetPackageName().ToString());
				}
				else
				{
					DeletedObject = TEXT("<Unknown>");
				}
			}
		}
		UE_LOG(LogCook, Error,
			TEXT("A %s package had some of its UObjects deleted during garbage collection after it started generating. This will cause errors during save of the package.")
			TEXT("\n\tDeleted object: %s")
			TEXT("\n\tSplitter=%s%s"),
			Info->IsGenerator() ? TEXT("Generator") : TEXT("Generated"),
			*DeletedObject,
			*GetSplitDataObjectName().ToString(),
			Info->IsGenerator() ? TEXT(".") : *FString::Printf(TEXT(", Generated=%s."),
				*Info->PackageData->GetPackageName().ToString()));
	}

	// Remove raw pointers from RootMovedObjects if they no longer exist in the weakpointers in CachedObjectsInOuter
	TSet<UObject*> CachedObjectsInOuterSet;
	for (FCachedObjectInOuter& CachedObjectInOuter : Info->PackageData->GetCachedObjectsInOuter())
	{
		UObject* Object = CachedObjectInOuter.Object.Get();
		if (Object)
		{
			CachedObjectsInOuterSet.Add(Object);
		}
	}
	for (TMap<UObject*, FCachedObjectInOuterGeneratorInfo>::TIterator Iter(Info->CachedObjectsInOuterInfo);
		Iter; ++Iter)
	{
		if (!CachedObjectsInOuterSet.Contains(Iter->Key))
		{
			Iter.RemoveCurrent();
		}
	}
}

FCookGenerationInfo::FCookGenerationInfo(FPackageData& InPackageData, bool bInGenerator)
	: PackageData(&InPackageData)
	, GeneratorSaveState(bInGenerator ? ESaveState::StartSave : ESaveState::StartPopulate)
	, bCreateAsMap(false), bHasCreatedPackage(false), bHasSaved(false), bTakenOverCachedCookedPlatformData(false)
	, bIssuedUndeclaredMovedObjectsWarning(false), bGenerator(bInGenerator)
{
}

void FCookGenerationInfo::SetSaveStateComplete(ESaveState CompletedState)
{
	GeneratorSaveState = CompletedState;
	if (GeneratorSaveState < ESaveState::Last)
	{
		GeneratorSaveState = static_cast<ESaveState>(static_cast<uint8>(GeneratorSaveState) + 1);
	}
}

void FCachedObjectInOuterGeneratorInfo::Initialize(UObject* Object)
{
	if (Object)
	{
		FullName = Object->GetFullName();
		bPublic = Object->HasAnyFlags(RF_Public);
	}
	else
	{
		FullName.Empty();
		bPublic = false;
	}

	bInitialized = true;
}

void FCookGenerationInfo::TakeOverCachedObjectsAndAddMoved(FGenerationHelper& GenerationHelper,
	TArray<FCachedObjectInOuter>& CachedObjectsInOuter, TArray<UObject*>& MovedObjects)
{
	CachedObjectsInOuterInfo.Reset();

	for (FCachedObjectInOuter& ObjectInOuter : CachedObjectsInOuter)
	{
		UObject* Object = ObjectInOuter.Object.Get();
		if (Object)
		{
			CachedObjectsInOuterInfo.FindOrAdd(Object).Initialize(Object);
		}
	}

	TArray<UObject*> ChildrenOfMovedObjects;
	for (UObject* Object : MovedObjects)
	{
		if (!IsValid(Object))
		{
			UE_LOG(LogCook, Warning,
				TEXT("CookPackageSplitter found non-valid object %s returned from %s on Splitter %s%s. Ignoring it."),
				Object ? *Object->GetFullName() : TEXT("<null>"),
				IsGenerator() ? TEXT("PopulateGeneratorPackage") : TEXT("PopulateGeneratedPackage"),
				*GenerationHelper.GetSplitDataObjectName().ToString(),
				IsGenerator() ? TEXT("") : *FString::Printf(TEXT(", Package %s"),
					*PackageData->GetPackageName().ToString()));
			continue;
		}
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			Info.bMoved = true;
			Info.bMovedRoot = true;
			CachedObjectsInOuter.Emplace(Object);
			GetObjectsWithOuter(Object, ChildrenOfMovedObjects, true /* bIncludeNestedObjects */, RF_NoFlags,
				EInternalObjectFlags::Garbage);
		}
	}

	for (UObject* Object : ChildrenOfMovedObjects)
	{
		check(IsValid(Object));
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			Info.bMoved = true;
			CachedObjectsInOuter.Emplace(Object);
		}
	}

	SetHasTakenOverCachedCookedPlatformData(true);
}

EPollStatus FCookGenerationInfo::RefreshPackageObjects(FGenerationHelper& GenerationHelper, UPackage* Package,
	bool& bOutFoundNewObjects, ESaveState DemotionState)
{
	bOutFoundNewObjects = false;
	TArray<UObject*> CurrentObjectsInOuter;
	GetObjectsWithOuter(Package, CurrentObjectsInOuter, true /* bIncludeNestedObjects */, RF_NoFlags,
		EInternalObjectFlags::Garbage);

	check(PackageData); // RefreshPackageObjects is only called when there is a PackageData
	TArray<FCachedObjectInOuter>& CachedObjectsInOuter = PackageData->GetCachedObjectsInOuter();
	UObject* FirstNewObject = nullptr;
	for (UObject* Object : CurrentObjectsInOuter)
	{
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			CachedObjectsInOuter.Emplace(Object);
			if (!FirstNewObject)
			{
				FirstNewObject = Object;
			}
		}
	}
	bOutFoundNewObjects = FirstNewObject != nullptr;

	if (FirstNewObject != nullptr && DemotionState != ESaveState::Last)
	{
		SetSaveState(DemotionState);
		if (++PackageData->GetNumRetriesBeginCacheOnObjects() > FPackageData::GetMaxNumRetriesBeginCacheOnObjects())
		{
			UE_LOG(LogCook, Error,
				TEXT("Cooker has repeatedly tried to call BeginCacheForCookedPlatformData on all objects in a generated package, but keeps finding new objects.\n")
				TEXT("Aborting the save of the package; programmer needs to debug why objects keep getting added to the package.\n")
				TEXT("Splitter: %s%s. Most recent created object: %s."),
				*GenerationHelper.GetSplitDataObjectName().ToString(),
				IsGenerator() ? TEXT("") : *FString::Printf(TEXT(", Package: %s"),
					*PackageData->GetPackageName().ToString()),
				*FirstNewObject->GetFullName());
			return EPollStatus::Error;
		}
	}
	return EPollStatus::Success;
}

void FCookGenerationInfo::CreatePackageHash()
{
	FBlake3 Blake3;
	Blake3.Update(&GenerationHash, sizeof(GenerationHash));
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	for (const FAssetDependency& Dependency : PackageDependencies)
	{
		TOptional<FAssetPackageData> DependencyData =
			AssetRegistry.GetAssetPackageDataCopy(Dependency.AssetId.PackageName);
		if (DependencyData)
		{
			Blake3.Update(&DependencyData->GetPackageSavedHash().GetBytes(),
				sizeof(decltype(DependencyData->GetPackageSavedHash().GetBytes())));
		}
	}
	PackageHash = FIoHash(Blake3.Finalize());
	// We store the PackageHash as a FIoHash, but UPackage and FAssetPackageData store it as a FGuid, which is smaller,
	// so we have to remove any data which doesn't fit into FGuid. This can be removed when we remove the deprecated
	// Guid storage on UPackage.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	constexpr int SizeDifference = sizeof(PackageHash) - sizeof(decltype(DeclVal<UPackage>().GetGuid()));
	if (SizeDifference > 0)
	{
		FMemory::Memset(((uint8*)&PackageHash.GetBytes())
			+ (sizeof(decltype(PackageHash.GetBytes())) - SizeDifference),
			0, SizeDifference);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void FCookGenerationInfo::IterativeCookValidateOrClear(FGenerationHelper& GenerationHelper,
	TConstArrayView<const ITargetPlatform*> RequestedPlatforms, const FIoHash& PreviousPackageHash,
	bool& bOutIterativelyUnmodified)
{
	UCookOnTheFlyServer& COTFS = GenerationHelper.GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
	bOutIterativelyUnmodified = PreviousPackageHash == this->PackageHash;
	if (bOutIterativelyUnmodified)
	{
		// If not directly modified, mark it as indirectly modified if any of its dependencies
		// were detected as modified during PopulateCookedPackages.
		for (const FAssetDependency& Dependency : this->PackageDependencies)
		{
			FPackageData* DependencyData =
				COTFS.PackageDatas->FindPackageDataByPackageName(Dependency.AssetId.PackageName);
			if (!DependencyData)
			{
				bOutIterativelyUnmodified = false;
				break;
			}
			for (const ITargetPlatform* TargetPlatform : RequestedPlatforms)
			{
				FPackagePlatformData* DependencyPlatformData = DependencyData->FindPlatformData(TargetPlatform);
				if (!DependencyPlatformData || !DependencyPlatformData->IsIterativelyUnmodified())
				{
					bOutIterativelyUnmodified = false;
					break;
				}
			}
			if (!bOutIterativelyUnmodified)
			{
				break;
			}
		}
	}

	bool bFirstPlatform = true;
	for (const ITargetPlatform* TargetPlatform : RequestedPlatforms)
	{
		if (bOutIterativelyUnmodified)
		{
			PackageData->FindOrAddPlatformData(TargetPlatform).SetIterativelyUnmodified(true);
		}
		bool bShouldIterativelySkip = bOutIterativelyUnmodified;
		ICookedPackageWriter& PackageWriter = COTFS.FindOrCreatePackageWriter(TargetPlatform);
		PackageWriter.UpdatePackageModificationStatus(PackageData->GetPackageName(), bOutIterativelyUnmodified,
			bShouldIterativelySkip);
		if (bShouldIterativelySkip)
		{
			PackageData->SetPlatformCooked(TargetPlatform, ECookResult::Succeeded);
			if (bFirstPlatform)
			{
				COOK_STAT(++DetailedCookStats::NumPackagesIterativelySkipped);
			}
			// Declare the package to the EDLCookInfo verification so we don't warn about missing exports from it
			UE::SavePackageUtilities::EDLCookInfoAddIterativelySkippedPackage(PackageData->GetPackageName());
		}
		else
		{
			COTFS.DeleteOutputForPackage(PackageData->GetPackageName(), TargetPlatform);
		}
		bFirstPlatform = false;
	}
}

}