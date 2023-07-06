// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ConcertPropertySelection.h"

#include "ConcertLogGlobal.h"
#include "Replication/PropertyChainUtils.h"

#include "Serialization/ArchiveSerializedPropertyChain.h"

const FName FConcertPropertyChain::InternalContainerPropertyValueName(TEXT("Value"));

TOptional<FConcertPropertyChain> FConcertPropertyChain::CreateFromPath(UStruct& Class, const TArray<FName>& NamePath)
{
	TOptional<FConcertPropertyChain> Result;
	// The goal here is to find a valid FProperty path based on the NamePath...
	UE::ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(Class, [&NamePath, &Result](FConcertPropertyChain&& Chain) mutable
	{
		// ... and performance could be improved by not expanding down the search tree by introducing ETreeBreakBehavior::SkipSubtree
		if (Chain == NamePath)
		{
			Result.Emplace(MoveTemp(Chain));
			return EBreakBehavior::Break;
		}
		return EBreakBehavior::Continue;
	});
	return Result;
}

FConcertPropertyChain::FConcertPropertyChain(const FArchiveSerializedPropertyChain* OptionalChain, const FProperty& LeafProperty)
{
	using namespace UE::ConcertSyncCore::PropertyChain;
	
	if (OptionalChain)
	{
		PathToProperty.Reserve(OptionalChain->GetNumProperties() + 1);
		for (int32 i = 0; i < OptionalChain->GetNumProperties(); ++i)
		{
			const FProperty* CurrentProperty = OptionalChain->GetPropertyFromRoot(i);
			
			const FProperty* ParentProperty = CurrentProperty->GetOwner<FProperty>();
			const bool bIsKeyOfMap = ParentProperty && ParentProperty->IsA(FMapProperty::StaticClass()) && CastField<FMapProperty>(ParentProperty)->KeyProp == CurrentProperty;
			if (!ensureMsgf(!bIsKeyOfMap, TEXT("Key child properties never appear in a path because all of them are assumed to be replicated. See documentation of PathToProperty!")))
			{
				// The input was invalid but we've constructed FConcertPropertyChain with valid state: it ends at the TMap property.
				return;
			}
			
			if (!IsInnerContainerProperty(*CurrentProperty))
			{
				PathToProperty.Add(OptionalChain->GetPropertyFromRoot(i)->GetFName());
			}

			if (!ensureMsgf(!IsNativeStructProperty(*CurrentProperty), TEXT("Child properties of structs implementing a custom Serialize function never appear in a path. See documentation of PathToProperty!")))
			{
				// The input was invalid but we've constructed FConcertPropertyChain with valid state: it ends at the native struct property.
				return;
			}
		}
	}
	else if (!ensureMsgf(LeafProperty.GetOwner<FProperty>() == nullptr, TEXT("Not a leaf property!")))
	{
		// The input was invalid but we've constructed FConcertPropertyChain with valid state: no property.
		return;
	}

	// LeafProperty is allowed to be an inner property of FArrayProperty, FSetProperty, FMapProperty::ValueProp but only if it is primitive or a native structs
	// In that case the property display label is InternalContainerPropertyValueName, which is "Value"
	if (IsInnerContainerProperty(LeafProperty))
	{
		if (ensureMsgf(IsPrimitiveProperty(LeafProperty) || IsNativeStructProperty(LeafProperty), TEXT("The chain only contains inner properties of primitives or native structs!")))
		{
			PathToProperty.Add(InternalContainerPropertyValueName);
		}
	}
	else
	{
		PathToProperty.Add(LeafProperty.GetFName());
	}

	UE_CLOG(!IsReplicatableProperty(LeafProperty), LogConcert, Warning, TEXT("Instantiated property chain with non replicable property: %s"), *ToString());
}

bool FConcertPropertyChain::IsChildOf(const FConcertPropertyChain& ParentToCheck) const
{
	if (PathToProperty.Num() < ParentToCheck.PathToProperty.Num())
	{
		return false;
	}

	for (int32 i = 0; i < ParentToCheck.PathToProperty.Num(); ++i)
	{
		if (PathToProperty[i] != ParentToCheck.PathToProperty[i])
		{
			return false;
		}
	}

	return true;
}

bool FConcertPropertyChain::IsDirectChildOf(const FConcertPropertyChain& ParentToCheck) const
{
	if (ParentToCheck.PathToProperty.Num() != PathToProperty.Num() - 1)
	{
		return false;
	}

	bool bArePathsEqual = true;
	for (int32 i = 0; bArePathsEqual && i < ParentToCheck.PathToProperty.Num(); ++i)
	{
		bArePathsEqual &= ParentToCheck.PathToProperty[i] == PathToProperty[i];
	}
	return bArePathsEqual;
}

FString FConcertPropertyChain::ToString(EToStringMethod Method) const
{
	switch (Method)
	{
	case EToStringMethod::Path:
		return FString::JoinBy(PathToProperty, TEXT("."), [](const FName& Name){ return Name.ToString(); });
	case EToStringMethod::LeafProperty:
		return PathToProperty.IsEmpty()
			? FName(NAME_None).ToString()
			: PathToProperty[PathToProperty.Num() - 1].ToString();
	default:
		checkNoEntry();
		return FName(NAME_None).ToString();
	}
	
}

uint32 GetTypeHash(const FConcertPropertyChain& Chain)
{
	return GetTypeHash(Chain.GetPathToProperty());
}

