// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBindingReferences.h"
#include "IMovieSceneBoundObjectProxy.h"
#include "ILocatorSpawnedCache.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "Engine/World.h"
#include "Evaluation/MovieSceneEvaluationState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingReferences)

namespace UE::MovieScene
{

UObject* FindBoundObjectProxy(UObject* BoundObject)
{
	if (!BoundObject)
	{
		return nullptr;
	}

	IMovieSceneBoundObjectProxy* RawInterface = Cast<IMovieSceneBoundObjectProxy>(BoundObject);
	if (RawInterface)
	{
		return RawInterface->NativeGetBoundObjectForSequencer(BoundObject);
	}
	else if (BoundObject->GetClass()->ImplementsInterface(UMovieSceneBoundObjectProxy::StaticClass()))
	{
		return IMovieSceneBoundObjectProxy::Execute_BP_GetBoundObjectForSequencer(BoundObject, BoundObject);
	}
	return BoundObject;
}

} // namespace UE::MovieScene

TArrayView<const FMovieSceneBindingReference> FMovieSceneBindingReferences::GetAllReferences() const
{
	return SortedReferences;
}

TArrayView<const FMovieSceneBindingReference> FMovieSceneBindingReferences::GetReferences(const FGuid& ObjectId) const
{
	const int32 Num   = SortedReferences.Num();
	const int32 Index = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	// Could also use a binary search here, but typically we are only dealing with a single binding
	int32 MatchNum = 0;
	while (Index + MatchNum < Num && SortedReferences[Index + MatchNum].ID == ObjectId)
	{
		++MatchNum;
	}

	return TArrayView<const FMovieSceneBindingReference>(SortedReferences.GetData() + Index, MatchNum);
}

bool FMovieSceneBindingReferences::HasBinding(const FGuid& ObjectId) const
{
	const int32 Index = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);
	return SortedReferences.IsValidIndex(Index) && SortedReferences[Index].ID == ObjectId;
}

const FMovieSceneBindingReference* FMovieSceneBindingReferences::AddBinding(const FGuid& ObjectId, FUniversalObjectLocator&& NewLocator)
{
	const int32 Index = Algo::UpperBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	FMovieSceneBindingReference& NewBinding = SortedReferences.Insert_GetRef(FMovieSceneBindingReference{ ObjectId, MoveTemp(NewLocator) }, Index);
	NewBinding.InitializeLocatorResolveFlags();
	return &NewBinding;
}

const FMovieSceneBindingReference* FMovieSceneBindingReferences::AddBinding(const FGuid& ObjectId, FUniversalObjectLocator&& NewLocator, ELocatorResolveFlags InEditorResolveFlags, ELocatorResolveFlags InRuntimeResolveFlags)
{
	const int32 Index = Algo::UpperBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	FMovieSceneBindingReference& NewBinding = SortedReferences.Insert_GetRef(FMovieSceneBindingReference{ ObjectId, MoveTemp(NewLocator) }, Index);
#if WITH_EDITOR
	NewBinding.EditorResolveFlags = InEditorResolveFlags;
#endif
	NewBinding.RuntimeResolveFlags = InRuntimeResolveFlags;

	return &NewBinding;
}


void FMovieSceneBindingReferences::RemoveBinding(const FGuid& ObjectId)
{
	const int32 StartIndex = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	if (SortedReferences.IsValidIndex(StartIndex) && SortedReferences[StartIndex].ID == ObjectId)
	{
		const int32 EndIndex = Algo::UpperBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);
		SortedReferences.RemoveAt(StartIndex, EndIndex-StartIndex);
	}
}

void FMovieSceneBindingReferences::ResolveBinding(const FGuid& ObjectId, const UE::UniversalObjectLocator::FResolveParams& ResolveParams, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	const int32 StartIndex = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);
	const int32 Num = SortedReferences.Num();

	for (int32 Index = StartIndex; Index < Num && SortedReferences[Index].ID == ObjectId; ++Index)
	{
		// If we have cache params in the ResolveParams, update the index
		if (const FLocatorSpawnedCacheResolveParameter* CacheParameter = ResolveParams.FindParameter<FLocatorSpawnedCacheResolveParameter>())
		{
			if (FMovieSceneObjectCache* Cache = static_cast<FMovieSceneObjectCache*>(CacheParameter->Cache))
			{
				FMovieSceneLocatorSpawnedCacheKey CacheKey = Cache->GetResolvingBindingCacheKey();
				if (CacheKey.BindingID == ObjectId && CacheKey.BindingIndex != Index)
				{
					CacheKey.BindingIndex = Index;
					Cache->SetResolvingBindingCacheKey(CacheKey);
				}
			}
		}

		// Add our resolve param flags
		if (ResolveParams.Context)
		{
			if (UWorld* World = ResolveParams.Context->GetWorld())
			{
#if WITH_EDITOR
				if (World->IsEditorWorld())
				{
					EnumAddFlags(const_cast<UE::UniversalObjectLocator::FResolveParams&>(ResolveParams).Flags, SortedReferences[Index].EditorResolveFlags);
				}
				else
#endif
				{
					EnumAddFlags(const_cast<UE::UniversalObjectLocator::FResolveParams&>(ResolveParams).Flags, SortedReferences[Index].RuntimeResolveFlags);
				}
			}
		}

		UObject* ResolvedObject = SortedReferences[Index].Locator.Resolve(ResolveParams).SyncGet().Object;
		ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);
		if (ResolvedObject)
		{
			OutObjects.Add(ResolvedObject);
		}
	}
}

void FMovieSceneBindingReferences::RemoveObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* InContext)
{
	const int32 StartIndex = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	for (int32 Index = StartIndex; Index < SortedReferences.Num() && SortedReferences[Index].ID == ObjectId; ++Index)
	{
		UObject* ResolvedObject = SortedReferences[Index].Locator.SyncFind(InContext);
		ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);

		if (InObjects.Contains(ResolvedObject))
		{
			SortedReferences.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}

void FMovieSceneBindingReferences::RemoveInvalidObjects(const FGuid& ObjectId, UObject* InContext)
{
	const int32 StartIndex = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	for (int32 Index = StartIndex; Index < SortedReferences.Num() && SortedReferences[Index].ID == ObjectId; ++Index)
	{
		UObject* ResolvedObject = SortedReferences[Index].Locator.SyncFind(InContext);
		ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);

		if (!IsValid(ResolvedObject))
		{
			SortedReferences.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}

FGuid FMovieSceneBindingReferences::FindBindingFromObject(UObject* InObject, UObject* InContext) const
{
	FUniversalObjectLocator Locator(InObject, InContext);

	for (const FMovieSceneBindingReference& Ref : SortedReferences)
	{
		if (Ref.Locator == Locator)
		{
			return Ref.ID;
		}
	}

	return FGuid();
}

void FMovieSceneBindingReferences::RemoveInvalidBindings(const TSet<FGuid>& ValidBindingIDs)
{
	const int32 StartNum = SortedReferences.Num();
	for (int32 Index = StartNum-1; Index >= 0; --Index)
	{
		if (!ValidBindingIDs.Contains(SortedReferences[Index].ID))
		{
			SortedReferences.RemoveAtSwap(Index, 1, false);
		}
	}

	if (SortedReferences.Num() != StartNum)
	{
		Algo::SortBy(SortedReferences, &FMovieSceneBindingReference::ID);
	}
}

void FMovieSceneBindingReferences::UnloadBoundObject(const UE::UniversalObjectLocator::FResolveParams& ResolveParams, const FGuid& ObjectId, int32 BindingIndex)
{
	const int32 StartIndex = Algo::LowerBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);

	if (SortedReferences.IsValidIndex(StartIndex) && SortedReferences[StartIndex].ID == ObjectId)
	{
		const int32 EndIndex = Algo::UpperBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);
		ensure(StartIndex + BindingIndex <= EndIndex);
		ensure(EnumHasAllFlags(ResolveParams.Flags, ELocatorResolveFlags::Unload));
		SortedReferences[StartIndex + BindingIndex].Locator.Resolve(ResolveParams);
	}
}

void FMovieSceneBindingReference::InitializeLocatorResolveFlags()
{
	using namespace UE::UniversalObjectLocator;
	auto InitializeFlags = [](const EFragmentTypeFlags& FragmentTypeFlags, ELocatorResolveFlags& OutResolveFlags)
	{
		if (EnumHasAllFlags(FragmentTypeFlags, EFragmentTypeFlags::CanBeLoaded | EFragmentTypeFlags::LoadedByDefault))
		{
			EnumAddFlags(OutResolveFlags, ELocatorResolveFlags::Load);
		}
	};

#if WITH_EDITOR
	InitializeFlags(Locator.GetDefaultEditorFlags(), EditorResolveFlags);
#endif
	InitializeFlags(Locator.GetDefaultRuntimeFlags(), RuntimeResolveFlags);
}
