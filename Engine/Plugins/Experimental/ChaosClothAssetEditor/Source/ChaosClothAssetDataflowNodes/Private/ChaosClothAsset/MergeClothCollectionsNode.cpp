// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/MergeClothCollectionsNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MergeClothCollectionsNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetMergeClothCollectionsNode"

FChaosClothAssetMergeClothCollectionsNode::FChaosClothAssetMergeClothCollectionsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}

void FChaosClothAssetMergeClothCollectionsNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;
		using namespace Chaos::Softs;

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		// Keep track of whether any of these collections are valid cloth collections
		FCollectionClothFacade ClothFacade(ClothCollection);
		bool bAreAnyValid = ClothFacade.IsValid();

		// Make it a valid cloth collection if needed
		if (!bAreAnyValid)
		{
			ClothFacade.DefineSchema();
		}

		FCollectionPropertyMutableFacade PropertyFacade(ClothCollection);
		bAreAnyValid |= PropertyFacade.IsValid();

		FCollectionClothSelectionFacade SelectionFacade(ClothCollection);
		bAreAnyValid |= SelectionFacade.IsValid();

		// Iterate through the inputs and append them to LOD 0
		const TArray<const FManagedArrayCollection*> Collections = GetCollections();
		for (int32 InputIndex = 1; InputIndex < Collections.Num(); ++InputIndex)
		{
			FManagedArrayCollection OtherCollection = GetValue<FManagedArrayCollection>(Context, Collections[InputIndex]);  // Can't use a const reference here sadly since the facade needs a SharedRef to be created
			const TSharedRef<const FManagedArrayCollection> OtherClothCollection = MakeShared<const FManagedArrayCollection>(MoveTemp(OtherCollection));
			const FCollectionClothConstFacade OtherClothFacade(OtherClothCollection);
			if (OtherClothFacade.IsValid())
			{
				ClothFacade.Append(OtherClothFacade);
				bAreAnyValid = true;
			}
			// Copy properties
			const FCollectionPropertyConstFacade OtherPropertyFacade(OtherClothCollection);
			if (OtherPropertyFacade.IsValid())
			{
				constexpr bool bUpdateExistingProperties = true; // Want last one wins.
				PropertyFacade.Append(OtherClothCollection.ToSharedPtr(), bUpdateExistingProperties);
				bAreAnyValid = true;
			}
			// Copy selections
			const FCollectionClothSelectionConstFacade OtherSelectionFacade(OtherClothCollection);
			if (OtherSelectionFacade.IsValid())
			{
				constexpr bool bUpdateExistingSelections = true; // Want last one wins.
				SelectionFacade.Append(OtherSelectionFacade, bUpdateExistingSelections);
				bAreAnyValid = true;
			}
		}

		// Set the output
		if (bAreAnyValid)
		{
			// Use the merged cloth collection, but only if there were at least one valid input cloth collections
			SetValue(Context, MoveTemp(*ClothCollection), &Collection);
		}
		else
		{
			// Otherwise pass through the first input unchanged
			const FManagedArrayCollection& Passthrough = GetValue<FManagedArrayCollection>(Context, &Collection);
			SetValue(Context, Passthrough, &Collection);
		}
	}
}

Dataflow::FPin FChaosClothAssetMergeClothCollectionsNode::AddPin()
{
	auto AddInput = [this](const FManagedArrayCollection* InCollection) -> Dataflow::FPin
	{
		RegisterInputConnection(InCollection);
		const FDataflowInput* const Input = FindInput(InCollection);
		return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
	};

	switch (NumInputs)
	{
	case 1: ++NumInputs; return AddInput(&Collection1);
	case 2: ++NumInputs; return AddInput(&Collection2);
	case 3: ++NumInputs; return AddInput(&Collection3);
	case 4: ++NumInputs; return AddInput(&Collection4);
	case 5: ++NumInputs; return AddInput(&Collection5);
	default: break;
	}

	return Super::AddPin();
}

Dataflow::FPin FChaosClothAssetMergeClothCollectionsNode::GetPinToRemove() const
{
	auto PinToRemove = [this](const FManagedArrayCollection* InCollection) -> Dataflow::FPin
	{
		const FDataflowInput* const Input = FindInput(InCollection);
		check(Input);
		return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
	};

	switch (NumInputs - 1)
	{
	case 1: return PinToRemove(&Collection1);
	case 2: return PinToRemove(&Collection2);
	case 3: return PinToRemove(&Collection3);
	case 4: return PinToRemove(&Collection4);
	case 5: return PinToRemove(&Collection5);
	default: break;
	}
	return Super::GetPinToRemove();
}

void FChaosClothAssetMergeClothCollectionsNode::OnPinRemoved(const Dataflow::FPin& Pin)
{
	auto CheckPinRemoved = [this, &Pin](const FManagedArrayCollection* InCollection)
	{
		check(Pin.Direction == Dataflow::FPin::EDirection::INPUT);
#if DO_CHECK
		const FDataflowInput* const Input = FindInput(InCollection);
		check(Input);
		check(Input->GetName() == Pin.Name);
		check(Input->GetType() == Pin.Type);
#endif
	};

	switch (NumInputs - 1)
	{
	case 1:
		CheckPinRemoved(&Collection1);
		--NumInputs;
		break;
	case 2:
		CheckPinRemoved(&Collection2);
		--NumInputs;
		break;
	case 3:
		CheckPinRemoved(&Collection3);
		--NumInputs;
		break;
	case 4:
		CheckPinRemoved(&Collection4);
		--NumInputs;
		break;
	case 5:
		CheckPinRemoved(&Collection5);
		--NumInputs;
		break;
	default:
		checkNoEntry();
		break;
	}

	return Super::OnPinRemoved(Pin);
}

TArray<const FManagedArrayCollection*> FChaosClothAssetMergeClothCollectionsNode::GetCollections() const
{
	TArray<const FManagedArrayCollection*> Collections;
	Collections.SetNumUninitialized(NumInputs);

	for (int32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		switch (InputIndex)
		{
		case 0: Collections[InputIndex] = &Collection; break;
		case 1: Collections[InputIndex] = &Collection1; break;
		case 2: Collections[InputIndex] = &Collection2; break;
		case 3: Collections[InputIndex] = &Collection3; break;
		case 4: Collections[InputIndex] = &Collection4; break;
		case 5: Collections[InputIndex] = &Collection5; break;
		default: Collections[InputIndex] = nullptr; check(false); break;
		}
	}
	return Collections;
}

void FChaosClothAssetMergeClothCollectionsNode::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		const int32 NumInputsToAdd = NumInputs - 1;
		NumInputs = 1;  // AddPin will increment it again
		for (int32 InputIndex = 0; InputIndex < NumInputsToAdd; ++InputIndex)
		{
			AddPin();
		}
		check(NumInputsToAdd == NumInputs - 1);
	}
}

#undef LOCTEXT_NAMESPACE
