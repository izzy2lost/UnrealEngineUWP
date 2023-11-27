// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBindingReferences.h"
#include "IMovieSceneBoundObjectProxy.h"

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

void FMovieSceneBindingReferences::AddBinding(const FGuid& ObjectId, FUniversalObjectLocator&& NewLocator)
{
	const int32 Index = Algo::UpperBoundBy(SortedReferences, ObjectId, &FMovieSceneBindingReference::ID);
	SortedReferences.Insert(FMovieSceneBindingReference{ ObjectId, MoveTemp(NewLocator) }, Index);
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
		UObject* ResolvedObject = SortedReferences[Index].Locator.Resolve(ResolveParams).SyncGet().Object;

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

