// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalObjectLocators/ActorLocatorFragment.h"
#include "UniversalObjectLocators/IActorLocatorFragmentResolver.h"
#include "UniversalObjectLocatorFragmentTypeHandle.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "UniversalObjectLocatorStringParams.h"
#include "UniversalObjectLocatorInitializeParams.h"
#include "UniversalObjectLocatorInitializeResult.h"

#include "UObject/UnrealNames.h"
#include "LevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Misc/EditorPathHelper.h"

UE::UniversalObjectLocator::TFragmentTypeHandle<FActorLocatorFragment> FActorLocatorFragment::FragmentType;

ULevel* GetLevelFromContext(const UObject* InContext)
{
	ULevel* Level = Cast<ULevel>(const_cast<UObject*>(InContext));
	if (!Level && InContext)
	{
		Level = InContext->GetTypedOuter<ULevel>();
	}
	return Level;
}

UObject* ResolveActorWithinLevel(const FActorLocatorFragment& Payload, ULevel* Level)
{
	// Default to owning world (to resolve AlwaysLoaded actors not part of a Streaming Level and Disabled Streaming World Partitions)
	UWorld* StreamingWorld = nullptr;

	// Construct the path to the level asset that the streamed level relates to
	ULevelStreaming* LevelStreaming = ULevelStreaming::FindStreamingLevel(Level);
	if (LevelStreaming)
	{
		// If we're loading a world partition runtime cell, we need to find the streaming world that is responsible for resolving those actors
		if (Level->IsWorldPartitionRuntimeCell())
		{
			StreamingWorld = LevelStreaming->GetStreamingWorld();
			check(StreamingWorld);
			LevelStreaming = ULevelStreaming::FindStreamingLevel(StreamingWorld->PersistentLevel);
		}
		else
		{
			StreamingWorld = Level->GetTypedOuter<UWorld>();
		}
	}

	if (LevelStreaming && StreamingWorld)
	{
		// StreamedLevelPackage is a package name of the form /Game/Folder/MapName, not a full asset path
		FName StreamedPackageName = (LevelStreaming->PackageNameToLoad == NAME_None) ? LevelStreaming->GetWorldAssetPackageFName() : LevelStreaming->PackageNameToLoad;

		// @todo: we're only checking package name here - to be 100% correct we should really check the asset name as well,
		//        but that is probably not necessary because multiple level assets in a single package are not supported
		if (Payload.Path.GetAssetPath().GetPackageName() == StreamedPackageName)
		{
			// Payload.Path.GetSubPathString() specifies the path from the package (so includes PersistentLevel.) so we must do a ResolveSubObject from its outer

			UObject* ResolvedObject = nullptr;
			StreamingWorld->ResolveSubobject(*Payload.Path.GetSubPathString(), ResolvedObject, /*bLoadIfExists*/false);
			return ResolvedObject;
		}
	}

	return nullptr;
}

UE::UniversalObjectLocator::FResolveResult FActorLocatorFragment::Resolve(const UE::UniversalObjectLocator::FResolveParams& Params) const
{
	using namespace UE::UniversalObjectLocator;

	UObject* Result = nullptr;

	// Handle the context being a IActorLocatorFragmentResolver object - this is the first port of call
	if (const IActorLocatorFragmentResolver* Resolver = Cast<const IActorLocatorFragmentResolver>(Params.Context))
	{
		if (Resolver && Resolver->ResolveActorLocatorPayload(*this, Result))
		{
			return FResolveResultData(Result);
		}
	}

	// Next handle default level streaming and partition worlds behavior
	if (ULevel* Level = GetLevelFromContext(Params.Context))
	{
		Result = ResolveActorWithinLevel(*this, Level);
	}

	// Finally fallback to just trying to resolve the path directly
	if (!Result)
	{
		Result = Path.ResolveObject();
	}

	return FResolveResultData(Result);
}

void FActorLocatorFragment::ToString(FStringBuilderBase& OutStringBuilder) const
{
	Path.AppendString(OutStringBuilder);
}

UE::UniversalObjectLocator::FParseStringResult FActorLocatorFragment::TryParseString(FStringView InString, const UE::UniversalObjectLocator::FParseStringParams& Params)
{
	Path = InString;
	return UE::UniversalObjectLocator::FParseStringResult().Success();
}

UE::UniversalObjectLocator::FInitializeResult FActorLocatorFragment::Initialize(const UE::UniversalObjectLocator::FInitializeParams& InParams)
{
	using namespace UE::UniversalObjectLocator;

#if WITH_EDITOR
	if (InParams.Context)
	{
		Path = FEditorPathHelper::GetEditorPathFromReferencer(InParams.Object, InParams.Context);
	}
	else
	{
		Path = FEditorPathHelper::GetEditorPath(InParams.Object);
	}
#else
	Path = InParams.Object;
#endif

	// Really, actors should be relative to their level in order to support streaming within level instances, but
	//   world partition makes that impossible
	return FInitializeResult::Absolute();
}

uint32 FActorLocatorFragment::ComputePriority(const UObject* ObjectToReference, const UObject* Context)
{
	// Can only reference actors and components
	if (ObjectToReference->IsA<AActor>())
	{
		// Let internal references be used first if possible
		return 100;
	}
	return 0;
}
