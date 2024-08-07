// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookGenerationHelper.h"

#include "Algo/Unique.h"
#include "AssetRegistry/AssetData.h"
#include "Cooker/CookDirector.h"
#include "Cooker/CookPlatformManager.h"
#include "Cooker/CookWorkerServer.h"
#include "Cooker/IWorkerRequests.h"
#include "Cooker/PackageTracker.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/Parse.h"
#include "TargetDomain/TargetDomainUtils.h"
#include "UObject/ReferenceChainSearch.h"

namespace UE::Cook
{

//////////////////////////////////////////////////////////////////////////
// FGenerationHelper

FGenerationHelper::FGenerationHelper(FPackageData& InOwner)
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
	bRequiresGeneratorPackageDestructBeforeResplit =
		CookPackageSplitterInstance->RequiresGeneratorPackageDestructBeforeResplit();
	DoesGeneratedRequireGeneratorValue =
		CookPackageSplitterInstance->DoesGeneratedRequireGenerator();
}

void FGenerationHelper::InitializeAsInvalid()
{
	if (InitializeStatus != EInitializeStatus::Uninitialized)
	{
		return;
	}
	InitializeStatus = EInitializeStatus::Invalid;
}

void FGenerationHelper::Uninitialize()
{
	if (InitializeStatus != EInitializeStatus::Valid)
	{
		return;
	}

	// Demote stalled packages; we will be garbage collecting so they no longer need to be preserved.
	// And we need to demote them so that they drop their references to the generation helper and allow it to be
	// deleted if no longer referenced
	DemoteStalledPackages(OwnerInfo.PackageData->GetPackageDatas().GetCookOnTheFlyServer());

	NotifyCompletion(ICookPackageSplitter::ETeardown::Complete);
	check(!CookPackageSplitterInstance);

	InitializeStatus = EInitializeStatus::Uninitialized;

	OwnerInfo.Uninitialize();
	SplitDataObject.Reset();
	SplitDataObjectName = NAME_None;
	RegisteredSplitterType = nullptr;
	// CookPackageSplitterInstance was set to null above
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		Info.Uninitialize();
	}
	OwnerPackage.Reset();
	// Keep PreviousGeneratedPackages; they are allowed in the uninitialized state
	ExternalActorDependencies.Empty();
	check(OwnerObjectsToMove.IsEmpty()); // We can not still be in the save state, so this should be empty
	// Do not modify the reference tracking variables
	// ReferenceFromKeepForIterative
	// ReferenceFromKeepForQueueResults
	// ReferenceFromKeepForGeneratorSave
	// ReferenceFromKeepForAllSavedOrGC
	// Keep MPCookNextAssignmentIndex; it is allowed in the uninitialized state
	// Keep NumSaved; it is allowed in the uninitialized state
	// InitializeStatus was modified above
	// Keep DoesGeneratedRequireGeneratorValue; it is allowed in the uninitialized state
	// Keep bUseInternalReferenceToAvoidGarbageCollect; it is allowed in the uninitialized state
	// Keep bRequiresGeneratorPackageDestructBeforeResplit; it is allowed in the uninitialized state
	bGeneratedList = false;
	bCurrentGCHasKeptGeneratorPackage = false;
	bCurrentGCHasKeptGeneratorKeepPackages = false;
	// Keep bKeepForAllSavedOrGC ; it is allowed in the uninitialized state
	// Keep bKeepForCompletedAllSavesMessage; it is allowed in the uninitialized state
	// Keep bNeedConfirmGeneratorPackageDestroyed; it is allowed in the uninitialized state
}

void FGenerationHelper::ModifyNumSaved(int32 Delta)
{
	NumSaved += Delta;
	int32 NumAllSaved = PackagesToGenerate.Num() + 1;
	check(0 <= NumSaved && NumSaved <= NumAllSaved);
	if (NumSaved == NumAllSaved)
	{
		UCookOnTheFlyServer& COTFS = GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
		// Only send OnAllSavesCompleted from director; clients have incomplete information and
		// could send it spuriously.
		// Additionally, only send it if we have completed queueing, to avoid sending it prematurely.
		// ModifyNumSaved(1 == 1) will occur when the generator package is iteratively skipped,
		// and ModifyNumSaved(k == k, k < expectednumber) can occur if we save some generated packages
		// (or mark them iteratively skippable) before getting the full list of packages from the worker
		// that called QueueGeneratedPackages.
		if (!COTFS.CookWorkerClient && bHasFinishedQueueGeneratedPackages)
		{
			if (COTFS.CookDirector)
			{
				FName PackageName = GetOwner().GetPackageName();
				FGeneratorEventMessage Message(EGeneratorEvent::AllSavesCompleted, PackageName);
				COTFS.CookDirector->BroadcastGeneratorMessage(MoveTemp(Message));
			}
			OnAllSavesCompleted(COTFS);
		}
	}
}

void FGenerationHelper::OnAllSavesCompleted(UCookOnTheFlyServer& COTFS)
{
	// Caller is responsible for holding a reference that keeps *this from destructing if it clears
	// these references
	ClearKeepForCompletedAllSavesMessage();
	ClearKeepForAllSavedOrGC();

	// Demote stalled packages; we will no longer need to come back to them
	DemoteStalledPackages(COTFS);
}

void FGenerationHelper::DemoteStalledPackages(UCookOnTheFlyServer& COTFS)
{
	// For any packages that we stalled because they were retracted and assigned to another worker,
	// demote them now. But don't demote non-stalled packages, because doing so could demote the final
	// package that we just saved locally and still needs to finish its work in PumpSaves.
	auto ConditionalDemote = [&COTFS](FCookGenerationInfo& Info)
		{
			if (Info.PackageData->IsStalled())
			{
				COTFS.DemoteToIdle(*Info.PackageData, ESendFlags::QueueAddAndRemove, ESuppressCookReason::RetractedByCookDirector);
			}
		};
	ConditionalDemote(OwnerInfo);
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		ConditionalDemote(Info);
	}
}

void FGenerationHelper::DiagnoseWhyNotShutdown()
{
	TStringBuilder<256> Lines;
	int32 ExpectedNumSaved = PackagesToGenerate.Num() + 1;
	if (NumSaved != ExpectedNumSaved)
	{
		Lines.Appendf(TEXT("\tNumSaved == %d, ExpectedNumSaved == %d.\n"), NumSaved, ExpectedNumSaved);
	}
	UCookOnTheFlyServer& COTFS = GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
	uint32 ExpectedRefCount = 1;
	auto TestInfo = [this, &Lines, &COTFS, &ExpectedRefCount](FCookGenerationInfo& Info)
		{
			if (Info.PackageData->GetState() != EPackageState::Idle)
			{
				Lines.Appendf(TEXT("\t%s%s is not idle; it is in state %d.\n"),
					Info.IsGenerator() ? TEXT("OwnerInfo") : TEXT("GeneratedPackage "),
					Info.IsGenerator() ? TEXT("") : *Info.GetPackageName(),
					static_cast<int32>(Info.PackageData->GetState()));
			}
			else
			{
				bool bMissingPlatform = false;
				for (const ITargetPlatform* TargetPlatform : COTFS.PlatformManager->GetSessionPlatforms())
				{
					const FPackagePlatformData* PlatformData = Info.PackageData->GetPlatformDatas().Find(TargetPlatform);
					if (!PlatformData || PlatformData->GetCookResults() == ECookResult::NotAttempted)
					{
						bMissingPlatform = true;
					}
				}
				if (bMissingPlatform)
				{
					Lines.Appendf(TEXT("\t%s%s was not cooked.\n"),
						Info.IsGenerator() ? TEXT("OwnerInfo") : TEXT("GeneratedPackage "),
						Info.IsGenerator() ? TEXT("") : *Info.GetPackageName());
				}
			}
			if (!Info.HasSaved())
			{
				Lines.Appendf(TEXT("\t%s%s has not marked saved.\n"),
					Info.IsGenerator() ? TEXT("OwnerInfo") : TEXT("GeneratedPackage "),
					Info.IsGenerator() ? TEXT("") : *Info.GetPackageName());
			}
			if (!Info.IsGenerator() && Info.PackageData->GetParentGenerationHelper())
			{
				Lines.Appendf(TEXT("\tGeneratedPackage %s has ParentGenerationHelper set.\n"), *Info.GetPackageName());
				++ExpectedRefCount;
			}
		};
	TestInfo(GetOwnerInfo());
	for (FCookGenerationInfo& Info : GetPackagesToGenerate())
	{
		TestInfo(Info);
	}

	if (ReferenceFromKeepForIterative)
	{
		Lines.Append(TEXT("\tReferenceFromKeepForIterative is set.\n"));
		++ExpectedRefCount;
	}
	if (ReferenceFromKeepForQueueResults)
	{
		Lines.Append(TEXT("\tReferenceFromKeepForQueueResults is set.\n"));
		++ExpectedRefCount;
	}
	if (ReferenceFromKeepForGeneratorSave)
	{
		Lines.Append(TEXT("\tReferenceFromKeepForGeneratorSave is set.\n"));
		++ExpectedRefCount;
	}
	if (bKeepForAllSavedOrGC)
	{
		Lines.Append(TEXT("\tbKeepForAllSavedOrGC is true.\n"));
	}
	if (bKeepForCompletedAllSavesMessage)
	{
		Lines.Append(TEXT("\tbKeepForCompletedAllSavesMessage is true.\n"));
	}
	if (ReferenceFromKeepForAllSavedOrGC)
	{
		if (!bKeepForAllSavedOrGC && !bKeepForCompletedAllSavesMessage)
		{
			Lines.Append(TEXT("\tReferenceFromKeepForAllSavedOrGC is set, despite bKeepForAllSavedOrGC and bKeepForCompletedAllSavesMessage being false.\n"));
		}
		++ExpectedRefCount;
	}
	if (GetRefCount() > ExpectedRefCount)
	{
		GetOwner().GetPackageDatas().LockAndEnumeratePackageDatas(
			[this, &ExpectedRefCount, &Lines](FPackageData* PackageData)
			{
				if (PackageData->GetParentGenerationHelper().GetReference() == this &&
					FindInfo(*PackageData) == nullptr)
				{
					Lines.Appendf(TEXT("\tGenerated package %s has ParentGenerationHelper set, but is not listed as a PackageToGenerate from the GenerationHelper.\n"),
						*PackageData->GetPackageName().ToString());
					++ExpectedRefCount;
				}
			});
	}
	if (GetRefCount() > ExpectedRefCount)
	{
		Lines.Appendf(TEXT("\tGetRefCount() has references from unknown sources. GetRefCount() == %u, ExpectedRefCount == %u.\n"),
			GetRefCount(), ExpectedRefCount);
	}

	if (Lines.Len() != 0)
	{
		FWorkerId WorkerId = GetWorkerIdThatSavedGenerator();
		Lines.Appendf(TEXT("\tGenerator: Saved on %s.\n"), *GetOwnerInfo().SavedOnWorker.ToString());
		for (FCookGenerationInfo& Info : PackagesToGenerate)
		{
			Lines.Appendf(TEXT("\tGeneratedPackage %s: Saved on %s.\n"), *Info.GetPackageName(),
				*Info.SavedOnWorker.ToString());
		}
	}
	else
	{
		Lines.Appendf(TEXT("\tDiagnoseWhyNotShutdown was called unexpectedly; GetRefCount() == 1 so this GenerationHelper should be shut down.\n"));
	}
	if (Lines.ToView().EndsWith(TEXT("\n")))
	{
		Lines.RemoveSuffix(1);
	}

	UE_LOG(LogCook, Error,
		TEXT("GenerationHelper for package %s is still allocated at end of cooksession. This is unexpected and could indicate some generated packages are missing."),
		*GetOwner().GetPackageName().ToString());
	UE_LOG(LogCook, Display, TEXT("Diagnostics:\n%s"), *Lines);
}

void FGenerationHelper::ForceUninitialize()
{
	TArray<FPackageData*> PackagesToDemote;
	auto TestInfo = [&PackagesToDemote](FCookGenerationInfo& Info)
		{
			if (Info.PackageData->GetState() != EPackageState::Idle)
			{
				PackagesToDemote.Add(Info.PackageData);
			}
		};
	TestInfo(GetOwnerInfo());
	for (FCookGenerationInfo& Info : GetPackagesToGenerate())
	{
		TestInfo(Info);
	}

	UCookOnTheFlyServer& COTFS = GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
	for (FPackageData* PackageData : PackagesToDemote)
	{
		COTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAddAndRemove, ESuppressCookReason::CookCanceled);
	}
	Uninitialize();
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
	TOptional<TConstArrayView<FCachedObjectInOuter>> CachedObjectsInOuter,
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
	ClearKeepForAllSavedOrGC();
	ClearKeepForCompletedAllSavesMessage();
}

FCookGenerationInfo* FGenerationHelper::FindInfo(const FPackageData& PackageData)
{
	ConditionalInitialize();
	return FindInfoNoInitialize(PackageData);
}

FCookGenerationInfo* FGenerationHelper::FindInfoNoInitialize(const FPackageData& PackageData)
{
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

	TMap<FPackageData*, int32> AlreadyExistingInfoPackageToIndex;
	for (int32 ExistingIndex = 0; ExistingIndex < PackagesToGenerate.Num(); ++ExistingIndex)
	{
		AlreadyExistingInfoPackageToIndex.Add(PackagesToGenerate[ExistingIndex].PackageData, ExistingIndex);
	}
	PackagesToGenerate.Reserve(GeneratorDatas.Num());

	TArray<const ITargetPlatform*, TInlineAllocator<1>> PlatformsToCook;
	OwnerPackageData.GetPlatformsNeedingCooking(PlatformsToCook);

	NumSaved = 0;
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

		FString PackageName = ICookPackageSplitter::ConstructGeneratedPackageName(OwnerPackageName,
			SplitterData.RelativePath, SplitterData.GeneratedRootPath);
		const FName PackageFName(*PackageName);
		FPackageData* PackageData = PackageDatas.TryAddPackageDataByPackageName(PackageFName,
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
		PackageData->SetDoesGeneratedRequireGenerator(DoesGeneratedRequireGeneratorValue);
		if (IFileManager::Get().FileExists(*PackageData->GetFileName().ToString()))
		{
			UE_LOG(LogCook, Warning,
				TEXT("PackageSplitter specified a generated package that already exists in the workspace domain. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *PackageName);
			return false;
		}

		FCookGenerationInfo* GeneratedInfo = nullptr;
		if (!AlreadyExistingInfoPackageToIndex.IsEmpty())
		{
			int32 ExistingIndex;
			if (AlreadyExistingInfoPackageToIndex.RemoveAndCopyValue(PackageData, ExistingIndex))
			{
				GeneratedInfo = &PackagesToGenerate[ExistingIndex];
			}
		}
		if (!GeneratedInfo)
		{
			GeneratedInfo = &PackagesToGenerate.Emplace_GetRef(*PackageData, false /* bInGenerator */);
		}
		GeneratedInfo->RelativePath = MoveTemp(SplitterData.RelativePath);
		GeneratedInfo->GeneratedRootPath = MoveTemp(SplitterData.GeneratedRootPath);
		GeneratedInfo->PackageDependencies = MoveTemp(SplitterData.PackageDependencies);
		for (TArray<FAssetDependency>::TIterator Iter(GeneratedInfo->PackageDependencies); Iter; ++Iter)
		{
			if (Iter->Category != UE::AssetRegistry::EDependencyCategory::Package)
			{
				UE_LOG(LogCook, Error,
					TEXT("PackageSplitter specified a dependency with category %d rather than category Package. Dependency will be ignored. Splitter=%s, Generated=%s."),
					(int32)Iter->Category, *this->GetSplitDataObjectName().ToString(), *PackageName);
				Iter.RemoveCurrent();
			}
			TStringBuilder<256> DependencyPackageName(InPlace, Iter->AssetId.PackageName);
			if (ICookPackageSplitter::IsUnderGeneratedPackageSubPath(DependencyPackageName))
			{
				UE_LOG(LogCook, Error,
					TEXT("PackageSplitter specified a dependency for one generated package on another generated package. Only dependencies on non-generated packages are allowed. Dependency will be ignored. Splitter=%s, Generated=%s, Dependency=%s."),
					*this->GetSplitDataObjectName().ToString(), *PackageName, *DependencyPackageName);
				Iter.RemoveCurrent();
			}
		}
		Algo::Sort(GeneratedInfo->PackageDependencies,
			[](const FAssetDependency& A, const FAssetDependency& B) { return A.LexicalLess(B); });
		GeneratedInfo->PackageDependencies.SetNum(Algo::Unique(GeneratedInfo->PackageDependencies));
		GeneratedInfo->SetIsCreateAsMap(bCreateAsMap);
		if (DoesGeneratedRequireGenerator() >= ICookPackageSplitter::EGeneratedRequiresGenerator::Save ||
			COTFS.MPCookGeneratorSplit == EMPCookGeneratorSplit::AllOnSameWorker)
		{
			PackageData->SetWorkerAssignmentConstraint(FWorkerId::Local());
		}

		// Create the Hash from the GenerationHash and Dependencies
		GeneratedInfo->CreatePackageHash();

		NumSaved += GeneratedInfo->HasSaved() ? 1 : 0;
	}
	NumSaved += OwnerInfo.HasSaved() ? 1 : 0;

	if (!AlreadyExistingInfoPackageToIndex.IsEmpty())
	{
		TArray<int32> UnusedExistingIndexes;
		for (TPair<FPackageData*, int32>& Pair : AlreadyExistingInfoPackageToIndex)
		{
			UnusedExistingIndexes.Add(Pair.Value);
			UE_LOG(LogCook, Warning, TEXT("Unexpected generated package (local TryGenerateList). A remote cookworker reported generated package %s for generator %s,")
				TEXT(" but when TryGenerateList was called on the CookDirector, this package was not listed.")
				TEXT(" This is unexpected and causes minor performance problems in the cook."),
				*Pair.Key->GetPackageName().ToString(), *OwnerPackageData.GetPackageName().ToString());
		}
		Algo::Sort(UnusedExistingIndexes);
		for (int32 UnusedIndex : ReverseIterate(UnusedExistingIndexes))
		{
			PackagesToGenerate.RemoveAt(UnusedIndex);
		}
	}
	ModifyNumSaved(0);

	bGeneratedList = true;
	return true;
}

bool FGenerationHelper::TryCallPopulateGeneratorPackage(
	TArray<ICookPackageSplitter::FGeneratedPackageForPreSave>& InOutGeneratedPackagesForPresave)
{
	if (OwnerInfo.HasCalledPopulate())
	{
		return true;
	}
	FPackageData& OwnerPackageData = GetOwner();
	FName OwnerPackageName = OwnerPackageData.GetPackageName();
	UCookOnTheFlyServer& COTFS = OwnerPackageData.GetPackageDatas().GetCookOnTheFlyServer();
	if (!bGeneratedList)
	{
		// Unexpected, caller should not call in this case
		UE_LOG(LogCook, Error, TEXT("TryCallPopulateGeneratorPackage called for package %s without a previous successful call to TryGenerateList."),
			*OwnerPackageName.ToString());
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
		return false;
	}
	check(IsValid()); // Could not have set bGeneratedList=true without being valid. 
	UObject* LocalSplitDataObject = this->FindOrLoadSplitDataObject();
	if (!LocalSplitDataObject)
	{
		UE_LOG(LogCook, Error,
			TEXT("Failed to call PopulateGeneratorPackage, CookPackageSplitter missing. Splitter=%s"),
			*GetSplitDataObjectName().ToString());
		return false;
	}
	UPackage* LocalOwnerPackage = LocalSplitDataObject->GetPackage();
	if (!COTFS.TryConstructGeneratedPackagesForPresave(OwnerPackageData, *this, InOutGeneratedPackagesForPresave))
	{
		UE_LOG(LogCook, Error,
			TEXT("PackageSplitter unexpected failure: could not ConstructGeneratedPackagesForPreSave. Splitter=%s"),
			*GetSplitDataObjectName().ToString());
		return false;
	}
	UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, OwnerPackageName,
		PackageAccessTrackingOps::NAME_CookerBuildObject);

	TArray<UPackage*> KeepReferencedPackages;
	TArray<UObject*> ObjectsToMove;
	bool bPopulateSucceeded = CookPackageSplitterInstance->PopulateGeneratorPackage(LocalOwnerPackage,
		LocalSplitDataObject, InOutGeneratedPackagesForPresave, ObjectsToMove, KeepReferencedPackages);
	if (!bPopulateSucceeded)
	{
		UE_LOG(LogCook, Error, TEXT("CookPackageSplitter returned false from PopulateGeneratorPackage. Splitter=%s"),
			*GetSplitDataObjectName().ToString());
		return false;
	}
	OwnerInfo.AddKeepReferencedPackages(*this, KeepReferencedPackages);
	OwnerObjectsToMove.Reserve(ObjectsToMove.Num());
	for (UObject* Object : ObjectsToMove)
	{
		if (Object)
		{
			OwnerObjectsToMove.Emplace(Object);
		}
	}

	// Contract Point 1: We will not call populate again until the splitter has been destroyed
	// Contract Point 2: We will not call populate again without garbage collecting the generator package
	OwnerInfo.SetHasCalledPopulate(true);
	SetKeepForAllSavedOrGC();
	return true;
}

bool FGenerationHelper::TryCallPopulateGeneratedPackage(UE::Cook::FCookGenerationInfo& Info,
	TArray<UObject*>& OutObjectsToMove)
{
	if (Info.HasCalledPopulate())
	{
		return true;
	}
	FPackageData& OwnerPackageData = GetOwner();
	FName OwnerPackageName = OwnerPackageData.GetPackageName();
	UCookOnTheFlyServer& COTFS = OwnerPackageData.GetPackageDatas().GetCookOnTheFlyServer();

	if (!bGeneratedList)
	{
		// Unexpected, caller should not call in this case
		UE_LOG(LogCook, Error, TEXT("TryCallPopulateGeneratedPackage called for package %s without a previous successful call to TryGenerateList."),
			*Info.GetPackageName());
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
		return false;
	}
	check(IsValid()); // Could not have set bGeneratedList=true without being valid. 
	UObject* LocalSplitDataObject = this->FindOrLoadSplitDataObject();
	if (!LocalSplitDataObject)
	{
		UE_LOG(LogCook, Error,
			TEXT("Failed to call TryCallPopulateGeneratedPackage, CookPackageSplitter missing. Splitter=%s"),
			*GetSplitDataObjectName().ToString());
		return false;
	}

	UPackage* Package = Info.PackageData->GetPackage();
	check(Package); // Caller checked this
	ICookPackageSplitter::FGeneratedPackageForPopulate SplitterInfo{ Info.RelativePath,
		Info.GeneratedRootPath, Package, Info.IsCreateAsMap() };

	UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, OwnerPackageName,
		PackageAccessTrackingOps::NAME_CookerBuildObject);

	TArray<UPackage*> KeepReferencedPackages;
	bool bPopulateSucceeded = CookPackageSplitterInstance->PopulateGeneratedPackage(Package,
		LocalSplitDataObject, SplitterInfo, OutObjectsToMove, KeepReferencedPackages);
	if (!bPopulateSucceeded)
	{
		UE_LOG(LogCook, Error,
			TEXT("CookPackageSplitter returned false from PopulateGeneratedPackage. Splitter=%s")
			TEXT("\nGeneratedPackage: %s"),
			*GetSplitDataObjectName().ToString(), *Info.GetPackageName());
		return false;
	}

	Info.AddKeepReferencedPackages(*this, KeepReferencedPackages);

	// Contract Point 1: We will not call populate again until the splitter has been destroyed
	// Contract Point 2: We will not call populate again without garbage collecting the generator package
	Info.SetHasCalledPopulate(true);
	SetKeepForAllSavedOrGC();
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
		TSet<FName> RemainingPreviousPackages;
		RemainingPreviousPackages.Reserve(PreviousGeneratedPackages.Num());
		for (const TPair<FName, FAssetPackageData>& Pair : PreviousGeneratedPackages)
		{
			RemainingPreviousPackages.Add(Pair.Key);
		}

		FPackageData& OwnerPackageData = GetOwner();
		TArray<const ITargetPlatform*, TInlineAllocator<1>> PlatformsToCook;
		OwnerPackageData.GetPlatformsNeedingCooking(PlatformsToCook);

		int32 NumIterativeUnmodified = 0;
		int32 NumIterativeModified = 0;
		int32 NumIterativeRemoved = 0;
		int32 NumIterativePrevious = RemainingPreviousPackages.Num();

		for (FCookGenerationInfo& GeneratedInfo : PackagesToGenerate)
		{
			FName GeneratedPackageName = GeneratedInfo.PackageData->GetPackageName();
			FAssetPackageData* PreviousAssetData = PreviousGeneratedPackages.Find(GeneratedPackageName);
			RemainingPreviousPackages.Remove(GeneratedPackageName);
			if (PreviousAssetData)
			{
				if (!bHybridIterativeEnabled)
				{
					bool bIterativelyUnmodified;
					GeneratedInfo.IterativeCookValidateOrClear(*this, PlatformsToCook, PreviousAssetData->GetPackageSavedHash(),
						bIterativelyUnmodified);
					++(bIterativelyUnmodified ? NumIterativeUnmodified : NumIterativeModified);
				}
				else
				{
					// Copy the current value for the package's hash into the PreviousPackageData, for use by incremental cook's
					// calculation in FRequestCluster::TryCalculateIterativelyUnmodified
					PreviousAssetData->SetPackageSavedHash(GeneratedInfo.PackageHash);
				}
			}
		}
		if (!RemainingPreviousPackages.IsEmpty())
		{
			NumIterativeRemoved = RemainingPreviousPackages.Num();
			for (FName PreviousPackageName : RemainingPreviousPackages)
			{
				for (const ITargetPlatform* TargetPlatform : PlatformsToCook)
				{
					COTFS.DeleteOutputForPackage(PreviousPackageName, TargetPlatform);
				}
			}
		}

		if (!bHybridIterativeEnabled)
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
	// and lose the information from SavedOnWorker or TryGenerateList.
	if (!COTFS.CookWorkerClient)
	{
		GetOwnerInfo().SavedOnWorker = SourceWorkerId;
		SetKeepForCompletedAllSavesMessage();
	}
	SetKeepForQueueResults();
}

void FGenerationHelper::EndQueueGeneratedPackages(UCookOnTheFlyServer& COTFS)
{
	bHasFinishedQueueGeneratedPackages = true;
	SetKeepForQueueResults();
	COTFS.WorkerRequests->EndQueueGeneratedPackages(COTFS, *this);
}

void FGenerationHelper::EndQueueGeneratedPackagesOnDirector(UCookOnTheFlyServer& COTFS, FWorkerId SourceWorkerId)
{
	// Note this function can be called on an uninitialized Generator; the generator is only needed
	// on the director so it can serve as the passer of messages.
	bHasFinishedQueueGeneratedPackages = true;
	// When we queued locally, this function is called after QueueDiscoveredPackage was called for each package.
	// When we queued on a remote CookWorker, the replication system from cookworker guarantees that all discovered
	// packages have been reported via TrackGeneratedPackageListedRemotely before we receive this function call
	// via the EGeneratorEvent::QueuedGeneratedPackages message (the package discovery messages are replicated
	// before the EGeneratorEvent). We therefore know that all generated packages have already been requested
	// or are in the discovery queue, so we can add a request fence listener now and know that when it is called
	// all generated packages have been queued and assigned.
	COTFS.PackageDatas->GetRequestQueue().AddRequestFenceListener(GetOwner().GetPackageName());
	SetKeepForQueueResults();

	// Setting OwnerInfo.SavedOnWorker and KeepForCompletedAllSavesMessage in response to this event is usually not
	// needed because they are set from NotifyStartQueueGeneratedPackages, but we set them anyway in case there is an
	// edge condition that skips those notifications.
	SetKeepForCompletedAllSavesMessage();
	GetOwnerInfo().SavedOnWorker = SourceWorkerId;

	// The save message for the owner may have come in before this GenerationHelper was created and thus
	// MarkPackageSavedRemotely was not called. Check for that case now and marked saved if so.
	if (GetOwner().HasAnyCookedPlatform())
	{
		GetOwnerInfo().SetHasSaved(*this, true, SourceWorkerId);
	}
}

void FGenerationHelper::OnRequestFencePassed(UCookOnTheFlyServer& COTFS)
{
	// This function should only be called in response to a subscription that is sent from the cook director
	check(!COTFS.CookWorkerClient);

	if (OwnerInfo.IsIterativelySkipped())
	{
		// PumpRequests has completed and we marked ourselves and all generated packages as iteratively skipped,
		// so we no longer need the PreviouslyCookedData or this entire GenerationHelper
		ClearKeepForIterative();
		PreviousGeneratedPackages.Empty();
	}

	if (bHasFinishedQueueGeneratedPackages)
	{
		// We have finished EndQueueGeneratedPackagesOnDirector, so all generated packages have been requested
		// and assigned to local ReadyRequests or to a CookWorker. Send OnQueuedGeneratedPackagesFencePassed
		// to ourselves and all cookworkers.

		// Call ModifyNumSaved to check for whether all packages have already been saved by the time we reach
		// the request fence. This can happen in iterative cooks, or in race conditions if we sent all packages
		// out for saving before receiving the EndQueueGeneratedPackagesOnDirector message.
		ModifyNumSaved(0);

		if (COTFS.CookDirector)
		{
			FName PackageName = GetOwner().GetPackageName();
			FGeneratorEventMessage Message(EGeneratorEvent::QueuedGeneratedPackagesFencePassed, PackageName);
			COTFS.CookDirector->BroadcastGeneratorMessage(MoveTemp(Message));
		}
		OnQueuedGeneratedPackagesFencePassed(COTFS);
	}
}

void FGenerationHelper::OnQueuedGeneratedPackagesFencePassed(UCookOnTheFlyServer& COTFS)
{
	ClearKeepForQueueResults();
	// We no longer need PreviousGeneratedPackages or KeepForIterative, because they are used only in 
	// StartQueueGeneratedPackages or the request cluster that they end up in in PumpRequests, both of which
	// are now finished. Clear them on the director and any CookWorkers that received them to free memory.
	ClearKeepForIterative();
	PreviousGeneratedPackages.Empty();
}

UPackage* FGenerationHelper::TryCreateGeneratedPackage(FCookGenerationInfo& GeneratedInfo, bool bResetToEmpty)
{
	if (!IsValid())
	{
		return nullptr;
	}

	FPackageData& GeneratedPackageData = *GeneratedInfo.PackageData;
	const FString GeneratedPackageName = GeneratedPackageData.GetPackageName().ToString();
	UPackage* LocalOwnerPackage = FindOrLoadOwnerPackage(GetOwner().GetPackageDatas().GetCookOnTheFlyServer());
	if (!LocalOwnerPackage)
	{
		UE_LOG(LogCook, Error,
			TEXT("TryCreateGeneratedPackage: could not load ParentGeneratorPackage %s for GeneratedPackage %s"),
			*GetOwner().GetPackageName().ToString(), *GeneratedPackageName);
		return nullptr;
	}

	UPackage* GeneratedPackage = FindObject<UPackage>(nullptr, *GeneratedPackageName);
	if (GeneratedPackage)
	{
		// The package might have been created for the generator's presave, or it might have been created and abandoned
		// by an earlier save attempt of the generated package.
		// If bResetToEmpty then we are starting the populate of the generated package and we need to remove all objects
		// from the package. Generated packages are created empty and it is the job of the CookPackageSplitter to populate
		// them during save.
		if (bResetToEmpty)
		{
			TArray<UObject*> ExistingObjects;
			GetObjectsWithPackage(GeneratedPackage, ExistingObjects, false /* bIncludeNestedObjects */);
			if (!ExistingObjects.IsEmpty())
			{
				UObject* TransientPackage = GetTransientPackage();
				for (UObject* Existing : ExistingObjects)
				{
					Existing->Rename(nullptr, TransientPackage, REN_DontCreateRedirectors);
				}
			}
		}
	}
	else
	{
#if ENABLE_COOK_STATS
		++DetailedCookStats::NumRequestedLoads;
#endif
		GeneratedPackage = CreatePackage(*GeneratedPackageName);
	}
	GeneratedPackage->SetSavedHash(GeneratedInfo.PackageHash);
	GeneratedPackage->SetPersistentGuid(LocalOwnerPackage->GetPersistentGuid());
	GeneratedPackage->SetPackageFlags(PKG_CookGenerated);
	GeneratedPackage->SetLoadedByEditorPropertiesOnly(LocalOwnerPackage->IsLoadedByEditorPropertiesOnly());
	GeneratedInfo.SetHasCreatedPackage(true);

	return GeneratedPackage;
}

void FGenerationHelper::FinishGeneratorPlatformSave(FPackageData& PackageData, bool bFirstPlatform,
	TArray<FAssetDependency>& OutPackageDependencies)
{
	ConditionalInitialize();

	FCookGenerationInfo* Info = &GetOwnerInfo();
	UCookOnTheFlyServer& COTFS = Info->PackageData->GetPackageDatas().GetCookOnTheFlyServer();

	// Set dependencies equal to the global AssetRegistry dependencies plus a dependency on
	// each generated package.
	COTFS.AssetRegistry->GetDependencies(PackageData.GetPackageName(), OutPackageDependencies,
		UE::AssetRegistry::EDependencyCategory::Package);
	OutPackageDependencies.Reserve(PackagesToGenerate.Num());
	for (FCookGenerationInfo& GeneratedInfo : GetPackagesToGenerate())
	{
		FAssetDependency& Dependency = OutPackageDependencies.Emplace_GetRef();
		Dependency.AssetId = FAssetIdentifier(GeneratedInfo.PackageData->GetPackageName());
		Dependency.Category = UE::AssetRegistry::EDependencyCategory::Package;
		Dependency.Properties = UE::AssetRegistry::EDependencyProperty::Game;
	}

	if (bFirstPlatform)
	{
		FetchExternalActorDependencies();
		COTFS.RecordExternalActorDependencies(GetExternalActorDependencies());
	}
}

void FGenerationHelper::FinishGeneratedPlatformSave(FPackageData& PackageData,
	UE::TargetDomain::FGeneratedPackageResultStruct& OutGeneratedResult)
{
	ConditionalInitialize();

	FCookGenerationInfo* Info = FindInfo(PackageData);
	if (!Info)
	{
		UE_LOG(LogCook, Error, TEXT("GeneratedInfo missing for package %s."),
			*PackageData.GetPackageName().ToString());
		return;
	}
	UCookOnTheFlyServer& COTFS = Info->PackageData->GetPackageDatas().GetCookOnTheFlyServer();

	// There should be no package dependencies present for the package from the global assetregistry
	// because it is newly created. Add on the dependencies declared for it from the CookPackageSplitter.
	OutGeneratedResult.PackageDependencies = Info->PackageDependencies;

	// Update the AssetPackageData for each requested platform with Guid and ImportedClasses
	TSet<UClass*> PackageClasses;
	UPackage* Package = PackageData.GetPackage();
	check(Package);
	ForEachObjectWithPackage(Package, [&PackageClasses, Package](UObject* Object)
		{
			UClass* Class = Object->GetClass();
			if (!Class->IsInPackage(Package)) // Imported classes list does not include classes in the package
			{
				PackageClasses.Add(Object->GetClass());
			}
			return true;
		});
	TArray<FName> ImportedClasses;
	ImportedClasses.Reserve(PackageClasses.Num());
	for (UClass* Class : PackageClasses)
	{
		TStringBuilder<256> ClassPath;
		Class->GetPathName(nullptr, ClassPath);
		ImportedClasses.Add(FName(ClassPath));
	}
	ImportedClasses.Sort(FNameLexicalLess());

	OutGeneratedResult.AssetPackageData.FileVersionUE = GPackageFileUEVersion;
	OutGeneratedResult.AssetPackageData.FileVersionLicenseeUE = GPackageFileLicenseeUEVersion;
	OutGeneratedResult.AssetPackageData.SetIsLicenseeVersion(FEngineVersion::Current().IsLicenseeVersion());
	OutGeneratedResult.AssetPackageData.Extension = FPackagePath::ParseExtension(
		WriteToString<256>(PackageData.GetFileName()));
	OutGeneratedResult.AssetPackageData.SetPackageSavedHash(Info->PackageHash);
	OutGeneratedResult.AssetPackageData.ImportedClasses = MoveTemp(ImportedClasses);
}

const FAssetPackageData* FGenerationHelper::GetIncrementalCookAssetPackageData(FPackageData& PackageData)
{
	return PreviousGeneratedPackages.Find(PackageData.GetPackageName());
}

const FAssetPackageData* FGenerationHelper::GetIncrementalCookAssetPackageData(FName PackageName)
{
	return PreviousGeneratedPackages.Find(PackageName);
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
				*Info.GetPackageName(),
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

	if (ReleaseSaveReason != EStateChangeReason::RecreateObjectCache &&
		ReleaseSaveReason != EStateChangeReason::DoneForNow)
	{
		// The save is completed and we will not come back to it; set state back to initial
		// state and drop our reference keeping this GenerationHelper in memory for the save.
		Info.SetHasSaved(*this, true, FWorkerId::Local());

		if (Info.IsGenerator())
		{
			// Now that we've finished saving, we know that we will not call QueueGeneratedPackages again, so we can
			// teardown iterative results as well
			ClearKeepForIterative();
			PreviousGeneratedPackages.Empty();

			// And also teardown data needed during save
			ClearKeepForGeneratorSave();
		}
		else
		{
			Info.PackageData->SetParentGenerationHelper(nullptr);
		}
	}
	Info.SetSaveState(Info.IsGenerator() ? FCookGenerationInfo::ESaveState::StartSave
		: FCookGenerationInfo::ESaveState::StartPopulate);

	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		if (NewState != EPackageState::Idle &&
			Info.PackageData->GetCachedObjectsInOuter().Num() != 0 && IsUseInternalReferenceToAvoidGarbageCollect() &&
			(ReleaseSaveReason != EStateChangeReason::Completed && ReleaseSaveReason != EStateChangeReason::DoneForNow
				&& ReleaseSaveReason != EStateChangeReason::SaveError
				&& ReleaseSaveReason != EStateChangeReason::CookerShutdown
				&& ReleaseSaveReason != EStateChangeReason::Retraction))
		{
			UE_LOG(LogCook, Error,
				TEXT("CookPackageSplitter failure: We are demoting a %s package from save and removing our references that keep its objects loaded.\n")
				TEXT("This will allow the objects to be garbage collected and cause failures in the splitter which expects them to remain loaded.\n")
				TEXT("Package=%s, Splitter=%s, ReleaseSaveReason=%s, NewState=%s"),
				Info.IsGenerator() ? TEXT("generator") : TEXT("generated"),
				*Info.GetPackageName(),
				*GetSplitDataObjectName().ToString(), LexToString(ReleaseSaveReason), LexToString(NewState));
			FDebug::DumpStackTraceToLog(ELogVerbosity::Display);
		}
		Info.CachedObjectsInOuterInfo.Empty();
		Info.SetHasTakenOverCachedCookedPlatformData(false);
	}
	Info.SetHasIssuedUndeclaredMovedObjectsWarning(false);

	// Clear KeepReferencedPackages; we no longer have a contract that we keep them referenced, except for the
	// generator. If the splitter requires EGeneratedRequiresGenerator::Populate, then we are required to keep them
	// referenced until all packages have saved as well, so we keep them referenced for the lifetime of the
	// GenerationHelper.
	if (!Info.IsGenerator() || DoesGeneratedRequireGenerator() <
		ICookPackageSplitter::EGeneratedRequiresGenerator::Populate)
	{
		Info.KeepReferencedPackages.Reset();
	}
	if (Info.IsGenerator())
	{
		OwnerObjectsToMove.Empty();
	}
}

bool FGenerationHelper::ShouldRetractionStallRatherThanDemote(FPackageData& PackageData)
{
	FCookGenerationInfo* Info = FindInfo(PackageData);
	if (Info)
	{
		if (PackageData.IsInStateProperty(EPackageStateProperty::Saving))
		{
			if (Info->GetSaveState() > FCookGenerationInfo::ESaveState::StartPopulate)
			{
				return true;
			}
		}
	}
	return false;
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
			bool bIsNeverCook = NeverCookPackageList.Contains(PackageData->GetPackageName());
			return !bIsNeverCook;
		});
	ExternalActorDependencies.Shrink();
}

void FGenerationHelper::SetPreviousGeneratedPackages(TMap<FName, FAssetPackageData>&& Packages)
{
	SetKeepForIterative();
	PreviousGeneratedPackages = MoveTemp(Packages);
}

template <typename T>
static void AppendWeakPtrsToObjectPtrArray(TArray<T*>& Out, TArray<TWeakObjectPtr<T>>& In)
{
	Out.Reserve(Out.Num() + In.Num());
	for (const TWeakObjectPtr<T>& WeakPtr : In)
	{
		T* Object = WeakPtr.Get();
		if (Object)
		{
			Out.Add(Object);
		}
	}
}

void FGenerationHelper::PreGarbageCollect(const TRefCountPtr<FGenerationHelper>& RefcountHeldByCaller,
	FPackageData& PackageData, TArray<TObjectPtr<UObject>>& GCKeepObjects,
	TArray<UPackage*>& GCKeepPackages, TArray<FPackageData*>& GCKeepPackageDatas, bool& bOutShouldDemote)
{
	bOutShouldDemote = false;
	if (&PackageData == &GetOwner())
	{
		PreGarbageCollectGCLifetimeData();
	}
	if (!IsInitialized())
	{
		return;
	}

	FCookGenerationInfo* InfoPtr = FindInfo(PackageData);
	if (!InfoPtr)
	{
		return;
	}
	FCookGenerationInfo& Info = *InfoPtr;

	if (!IsUseInternalReferenceToAvoidGarbageCollect() && !Info.PackageData->GetIsCookLast())
	{
		// If we don't have a contract to keep the packagedata referenced during GC, don't report
		// anything to garbage collection, and demote the package if it has progressed too far
		if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
		{
			bOutShouldDemote = true;
		}
		return;
	}

	// When we have a contract to keep the packagedata referenced, keep its various object pointers referenced.

	// We have a contract that KeepReferencedPackages in any Info are kept referenced.
	bool bKeepingAnyObjects = false;
	bool bNeedsGeneratorPackage = false;
	if (&Info == &OwnerInfo)
	{
		// Handled by bCurrentGCHasKeptGeneratorKeepPackages
	}
	else if (!Info.KeepReferencedPackages.IsEmpty())
	{
		bKeepingAnyObjects = true;
		AppendWeakPtrsToObjectPtrArray(GCKeepPackages, Info.KeepReferencedPackages);
	}
	if (!bCurrentGCHasKeptGeneratorKeepPackages)
	{
		bCurrentGCHasKeptGeneratorKeepPackages = true;
		if (!OwnerInfo.KeepReferencedPackages.IsEmpty())
		{
			bNeedsGeneratorPackage = true;
			AppendWeakPtrsToObjectPtrArray(GCKeepPackages, OwnerInfo.KeepReferencedPackages);
		}
	}

	// Keep the objects returned from GetObjectsToMove* functions referenced
	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		bKeepingAnyObjects = true;
		for (FCachedObjectInOuter& CachedObjectInOuter : Info.PackageData->GetCachedObjectsInOuter())
		{
			UObject* Object = CachedObjectInOuter.Object.Get();
			if (Object)
			{
				GCKeepObjects.Add(Object);
			}
		}
	}

	// Keep the generator and generated package referenced if we've passed the call to populate, or if we are keeping
	// any other objects referenced
	if (bKeepingAnyObjects || Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		bNeedsGeneratorPackage = true;
		if (&Info != &OwnerInfo)
		{
			UPackage* Package = Info.PackageData->GetPackage();
			if (Package)
			{
				GCKeepPackages.Add(Package);
				GCKeepPackageDatas.Add(Info.PackageData);
			}
		}
	}

	if (bNeedsGeneratorPackage && !bCurrentGCHasKeptGeneratorPackage)
	{
		bCurrentGCHasKeptGeneratorPackage = true;
		UPackage* Package = OwnerInfo.PackageData->GetPackage();
		if (Package)
		{
			GCKeepPackages.Add(Package);
			GCKeepPackageDatas.Add(Info.PackageData);
		}
	}
}

void FGenerationHelper::PreGarbageCollectGCLifetimeData()
{
	// Starts at one because the caller of PreGarbageCollect has a ref
	uint32 HoldForGCRefCounts = 1;
	HoldForGCRefCounts += ReferenceFromKeepForAllSavedOrGC.IsValid() ? 1 : 0;
	// Every stalled package will be holding a refcount, but is allowed to be demoted and released
	// if we are going to garbage collect.
	// If the generator package is stalled, that's a complex case that we don't need to handle optimally;
	// just keep the entire generation helper referenced.
	if (OwnerInfo.PackageData->IsStalled())
	{
		return;
	}
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		if (Info.PackageData->GetParentGenerationHelper() && Info.PackageData->IsStalled())
		{
			HoldForGCRefCounts += 1;
		}
	}

	check(GetRefCount() >= HoldForGCRefCounts);
	if (GetRefCount() > HoldForGCRefCounts)
	{
		// Something else (generator save or generated package save, etc) is keeping us referenced
		// and we need to not allow destruction. Nothing further to do.
		return;
	}

	// We should either uninitialize or destroy after the garbage collect.
	// We should not uninitialize unless the Generator package is going to be collected, but we are in a state
	// where nothing in the cooker is depending on the package anymore (all generator and generated packages are
	// not in the save state or are stalled) so we do expect the generator package to be garbage collected by the
	// upcoming GC.
	// But for that to happen we have to drop our references to it from this FGenerationHelper, so we need
	// to uninitialize. Also mark that we should check for generator garbage collect in PostGarbageCollect.
	// Depending on the Splitter class, it may tolerate failure to GC the Generator package, in which case we
	// should not log this error.
	Uninitialize();
	bNeedConfirmGeneratorPackageDestroyed = IsRequiresGeneratorPackageDestructBeforeResplit();
}

void FGenerationHelper::PostGarbageCollectGCLifetimeData()
{
	if (bNeedConfirmGeneratorPackageDestroyed)
	{
		VerifyGeneratorPackageGarbageCollected();
		bNeedConfirmGeneratorPackageDestroyed = false;
	}

	if (!IsInitialized())
	{
		// ClearKeepForAllSavedOrGC is no longer required when Uninitialized after a GC
		// Note that this keep flag might be the last persistent reference to *this and *this will be deleted when the
		// caller of PostGarbageCollect drops its reference.
		ClearKeepForAllSavedOrGC();
	}
}

void FGenerationHelper::TrackGeneratedPackageListedRemotely(UCookOnTheFlyServer& COTFS, FPackageData& PackageData,
	const FIoHash& CurrentPackageHash)
{
	if (bGeneratedList)
	{
		if (!FindInfo(PackageData))
		{
			UE_LOG(LogCook, Warning, TEXT("Unexpected generated package (discovery replication). A remote cookworker reported generated package %s for generator %s,")
				TEXT(" but when TryGenerateList was called on the CookDirector, this package was not listed.")
				TEXT(" This is unexpected and causes minor performance problems in the cook."),
				*PackageData.GetPackageName().ToString(), *GetOwner().GetPackageName().ToString());
		}
	}
	else
	{
		if (!FindInfoNoInitialize(PackageData))
		{
			bool bGenerator = false; // Cannot be the generator, if it were we would have found it
			PackagesToGenerate.Emplace(PackageData, bGenerator);
		}
	}
	FAssetPackageData* PreviousAssetData = PreviousGeneratedPackages.Find(PackageData.GetPackageName());
	if (PreviousAssetData)
	{
		// Copy the current value for the package's hash into the PreviousPackageData, for use by incremental cook's
		// calculation in FRequestCluster::TryCalculateIterativelyUnmodified
		PreviousAssetData->SetPackageSavedHash(CurrentPackageHash);
	}
}

void FGenerationHelper::MarkPackageSavedRemotely(UCookOnTheFlyServer& COTFS, FPackageData& PackageData,
	FWorkerId SourceWorkerId)
{
	FCookGenerationInfo* Info = FindInfoNoInitialize(PackageData);
	if (Info)
	{
		Info->SetHasSaved(*this, true, SourceWorkerId);
	}
}

void FGenerationHelper::MarkPackageIterativelySkipped(FPackageData& PackageData)
{
	FCookGenerationInfo* Info = FindInfoNoInitialize(PackageData);
	if (Info)
	{
		Info->SetHasSaved(*this, true, FWorkerId::Local());
		Info->SetIterativelySkipped(true);
	}

	if (&PackageData == &GetOwner())
	{
		// The entire generator package has been skipped. Wait for the current cluster to complete
		// so we can mark all of our generated packages as skipped, but then clear the iterative data;
		// it will no longer be needed.
		GetOwner().GetPackageDatas().GetRequestQueue().AddRequestFenceListener(GetOwner().GetPackageName());
	}
}

void FGenerationHelper::PostGarbageCollect(const TRefCountPtr<FGenerationHelper>& RefcountHeldByCaller)
{
	PostGarbageCollectGCLifetimeData();
	if (!IsInitialized())
	{
		return;
	}
	bCurrentGCHasKeptGeneratorPackage = false;
	bCurrentGCHasKeptGeneratorKeepPackages = false;

	FPackageData& Owner = GetOwner();
	if (Owner.IsInStateProperty(EPackageStateProperty::Saving))
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
	else if (!IsUseInternalReferenceToAvoidGarbageCollect())
	{
		// After the Generator Package is saved, we drop our references to it and it can be garbage collected
		// If we have any packages left to populate, our splitter contract requires that it be garbage collected
		// because we promise that the package is not partially GC'd during calls to TryPopulateGeneratedPackage
		// The splitter can opt-out of this contract and keep it referenced itself if it desires.
		if (!Owner.IsInProgress() && !Owner.IsKeepReferencedDuringGC())
		{
			VerifyGeneratorPackageGarbageCollected();
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
					*Info.GetPackageName());
				
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
		for (TArray<TWeakObjectPtr<UPackage>>::TIterator Iter(Info.KeepReferencedPackages); Iter; ++Iter)
		{
			const TWeakObjectPtr<UPackage>& KeepPtr = *Iter;
			if (!KeepPtr.Get())
			{
				UE_LOG(LogCook, Warning,
					TEXT("PackageSplitter returned a package in OutKeepReferencedPackages that the cooker tried to keep referenced, but it was removed by garbage collection anyway.")
					TEXT(" This might cause errors during save of the generated packages.")
					TEXT("\n\tSplitter=%s, Generated=%s."),
					*GetSplitDataObjectName().ToString(),
					*Info.GetPackageName());
				Iter.RemoveCurrentSwap();
			}
		}
	}
}

void FGenerationHelper::VerifyGeneratorPackageGarbageCollected()
{
	FString GeneratorPackageName = GetOwner().GetPackageName().ToString();
	UPackage* LocalOwnerPackage = FindObject<UPackage>(nullptr, *GeneratorPackageName);
	if (LocalOwnerPackage)
	{
		// Might be called when uninitialized, so do not call GetSplitDataObjectNameIfAvailable
		FString Identifier;
		if (!SplitDataObjectName.IsNone())
		{
			Identifier = FString::Printf(TEXT("Splitter=%s"), *SplitDataObjectName.ToString());
		}
		else
		{
			Identifier = FString::Printf(TEXT("GeneratorPackage=%s"), *GeneratorPackageName);
		}
		UE_LOG(LogCook, Error,
			TEXT("PackageSplitter found the Generator package still in memory after it should have been deleted by GC.")
			TEXT("\n\tThis is unexpected since garbage has been collected and the package should have been unreferenced so it should have been collected, and will break population of Generated packages.")
			TEXT("\n\tSplitter=%s"), *Identifier);
		EReferenceChainSearchMode SearchMode = EReferenceChainSearchMode::Shortest
			| EReferenceChainSearchMode::PrintAllResults
			| EReferenceChainSearchMode::FullChain;
		FReferenceChainSearch RefChainSearch(LocalOwnerPackage, SearchMode);
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

	// Remove raw pointers from CachedObjectsInOuterInfo if they no longer exist in the weakpointers 
	// in CachedObjectsInOuter
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
	, bIssuedUndeclaredMovedObjectsWarning(false), bGenerator(bInGenerator), bHasCalledPopulate(false)
	, bIterativelySkipped(false)
{
}

void FCookGenerationInfo::Uninitialize()
{
	// Check that we have left the save state first, since other assertions assume we have left the save state
	checkf(GeneratorSaveState == (bGenerator ? ESaveState::StartSave : ESaveState::StartPopulate),
		TEXT("Cooker bug: Expected FCookGenerationInfo::Uninitialize to not be called for a package still in the save state, ")
		TEXT("but %s package %s has SaveState %d."),
		bGenerator ? TEXT("generator") : TEXT("generated"), *GetPackageName(), static_cast<int32>(GeneratorSaveState));

	PackageHash.Reset();
	RelativePath.Empty();
	GeneratedRootPath.Empty();
	GenerationHash.Reset();
	PackageDependencies.Empty();
	// Keep PackageData; it is allowed in the uninitialized state
	KeepReferencedPackages.Empty();
	check(CachedObjectsInOuterInfo.IsEmpty()); // We can not still be in the save state, so this should be empty
	// Keep SavedOnWorker; it is allowed in the uninitialized state
	bCreateAsMap = false;
	bHasCreatedPackage = false;
	// Keep bHasSaved; it is allowed in the uninitialized state
	bTakenOverCachedCookedPlatformData = false;
	bIssuedUndeclaredMovedObjectsWarning = false;
	// Keep bGenerator; it is allowed in the uninitialized state
	bHasCalledPopulate = false;
	// Keep bIterativelySkipped; it is allowed in the uninitialized state
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

void FCookGenerationInfo::AddKeepReferencedPackages(FGenerationHelper& GenerationHelper,
	TArray<UPackage*>& InKeepReferencedPackages)
{
	KeepReferencedPackages.Reserve(KeepReferencedPackages.Num() + InKeepReferencedPackages.Num());
	for (UPackage* Package : InKeepReferencedPackages)
	{
		TWeakObjectPtr<UPackage>& WeakPtr = KeepReferencedPackages.Emplace_GetRef(Package);
		if (!WeakPtr.Get())
		{
			UE_LOG(LogCook, Warning,
				TEXT("PackageSplitter returned a package in OutKeepReferencedPackages that is already marked as garbage.")
				TEXT(" This might cause errors during save of the generated packages.")
				TEXT("\n\tSplitter=%s, Generated=%s."),
				*GenerationHelper.GetSplitDataObjectName().ToString(),
				*GetPackageName());

			KeepReferencedPackages.Pop(EAllowShrinking::No);
		}
	}
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

namespace GenerationHelperPrivate
{

enum class ERequiredSaveOrder
{
	None,
	GeneratorFirst,
	GeneratedFirst,
};

ERequiredSaveOrder RequiredSaveOrder = ERequiredSaveOrder::None;

}

void FGenerationHelper::SetBeginCookConfigSettings()
{
	const TCHAR* CommandLine = FCommandLine::Get();

	FString SaveOrder;
	GConfig->GetString(TEXT("CookSettings"), TEXT("MPCookGeneratorSaveOrder"), SaveOrder, GEditorIni);
	FParse::Value(FCommandLine::Get(), TEXT("-MPCookGeneratorSaveOrder="), SaveOrder);
	if (SaveOrder == TEXT("GeneratorFirst"))
	{
		GenerationHelperPrivate::RequiredSaveOrder = GenerationHelperPrivate::ERequiredSaveOrder::GeneratorFirst;
	}
	else if (SaveOrder == TEXT("GeneratedFirst"))
	{
		GenerationHelperPrivate::RequiredSaveOrder = GenerationHelperPrivate::ERequiredSaveOrder::GeneratedFirst;
	}
	else
	{
		if (!SaveOrder.IsEmpty() && SaveOrder != TEXT("None"))
		{
			UE_LOG(LogCook, Error,
				TEXT("Invalid setting Editor:[CookSettings]:MPCookGeneratorSaveOrder=%s. Expected values are 'GeneratorFirst', 'GeneratedFirst', or 'None'. Falling back to default 'None'."),
				*SaveOrder);
		}
		GenerationHelperPrivate::RequiredSaveOrder = GenerationHelperPrivate::ERequiredSaveOrder::None;
	}
}

bool FGenerationHelper::IsGeneratorSavedFirst()
{
	return GenerationHelperPrivate::RequiredSaveOrder == GenerationHelperPrivate::ERequiredSaveOrder::GeneratorFirst;
}

bool FGenerationHelper::IsGeneratedSavedFirst()
{
	return GenerationHelperPrivate::RequiredSaveOrder == GenerationHelperPrivate::ERequiredSaveOrder::GeneratedFirst;
}

}