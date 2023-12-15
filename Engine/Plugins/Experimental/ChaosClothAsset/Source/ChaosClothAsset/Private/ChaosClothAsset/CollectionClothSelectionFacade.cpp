// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#define LOCTEXT_NAMESPACE "FCollectionClothSelectionFacade"

namespace UE::Chaos::ClothAsset
{

	namespace Private
	{
		static const FName SelectionGroup(TEXT("Selection"));
	}

	// --------------- FCollectionClothSelectionConstFacade -------------------------------

	FCollectionClothSelectionConstFacade::FCollectionClothSelectionConstFacade(const TSharedRef<const FManagedArrayCollection>& InManagedArrayCollection) :
		ManagedArrayCollection(ConstCastSharedRef<FManagedArrayCollection>(InManagedArrayCollection))
	{
	}

	bool FCollectionClothSelectionConstFacade::IsValid() const
	{
		return ManagedArrayCollection->HasGroup(Private::SelectionGroup) &&
			ManagedArrayCollection->NumElements(Private::SelectionGroup) == 1;
	}

	int32 FCollectionClothSelectionConstFacade::GetNumSelections() const
	{
		return ManagedArrayCollection->NumAttributes(Private::SelectionGroup);
	}

	TArray<FName> FCollectionClothSelectionConstFacade::GetNames() const
	{
		return ManagedArrayCollection->AttributeNames(Private::SelectionGroup);
	}

	bool FCollectionClothSelectionConstFacade::HasSelection(const FName& Name) const
	{
		return IsValid() && ManagedArrayCollection->HasAttribute(Name, Private::SelectionGroup);
	}

	FName FCollectionClothSelectionConstFacade::GetSelectionGroup(const FName& Name) const
	{
		check(IsValid());
		check(HasSelection(Name));
		return ManagedArrayCollection->GetDependency(Name, Private::SelectionGroup);
	}

	const TSet<int32>& FCollectionClothSelectionConstFacade::GetSelectionSet(const FName& Name) const
	{
		check(IsValid());
		const TManagedArray<TSet<int32>>* const Selection = ManagedArrayCollection->FindAttributeTyped<TSet<int32>>(Name, Private::SelectionGroup);
		check(Selection);
		return *Selection->GetData();
	}

	const TSet<int32>* FCollectionClothSelectionConstFacade::FindSelectionSet(const FName& Name) const
	{
		check(IsValid());
		const TManagedArray<TSet<int32>>* const Selection = ManagedArrayCollection->FindAttributeTyped<TSet<int32>>(Name, Private::SelectionGroup);
		return Selection ? Selection->GetData() : nullptr;
	}

	// --------------- FCollectionClothSelectionFacade -------------------------------

	FCollectionClothSelectionFacade::FCollectionClothSelectionFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection) :
		FCollectionClothSelectionConstFacade(ManagedArrayCollection)
	{}

	void FCollectionClothSelectionFacade::DefineSchema()
	{
		if (!ManagedArrayCollection->HasGroup(Private::SelectionGroup))
		{
			ManagedArrayCollection->AddGroup(Private::SelectionGroup);
		}
		const int32 NumElements = ManagedArrayCollection->NumElements(Private::SelectionGroup);
		if (NumElements > 1)
		{
			ManagedArrayCollection->RemoveElements(Private::SelectionGroup, NumElements - 1, 1);
		}
		else if (NumElements == 0)
		{
			ManagedArrayCollection->AddElements(1, Private::SelectionGroup);
		}
	}

	void FCollectionClothSelectionFacade::Append(const FCollectionClothSelectionConstFacade& Other, bool bOverwriteExistingIfMismatched)
	{
		if (Other.IsValid())
		{
			const int32 NumInSelections = Other.GetNumSelections();
			const TArray<FName> InSelectionNames = Other.GetNames();
			for (int32 InSelectionIndex = 0; InSelectionIndex < NumInSelections; ++InSelectionIndex)
			{
				const FName& SelectionName = InSelectionNames[InSelectionIndex];
				if (HasSelection(SelectionName))
				{
					if (GetSelectionGroup(SelectionName) == Other.GetSelectionGroup(SelectionName))
					{
						TSet<int32>& UnionedSet = GetSelectionSet(SelectionName);
						UnionedSet.Append(Other.GetSelectionSet(SelectionName));
						continue;
					}
					if (!bOverwriteExistingIfMismatched)
					{
						continue;
					}
				}

				FindOrAddSelectionSet(SelectionName, Other.GetSelectionGroup(SelectionName)) = Other.GetSelectionSet(SelectionName);
			}
		}
	}

	TSet<int32>& FCollectionClothSelectionFacade::GetSelectionSet(const FName& Name)
	{
		check(IsValid());
		TManagedArray<TSet<int32>>* const Selection = ManagedArrayCollection->FindAttributeTyped<TSet<int32>>(Name, Private::SelectionGroup);
		check(Selection);
		return *Selection->GetData();
	}

	TSet<int32>* FCollectionClothSelectionFacade::FindSelectionSet(const FName& Name)
	{
		check(IsValid());
		TManagedArray<TSet<int32>>* const Selection = ManagedArrayCollection->FindAttributeTyped<TSet<int32>>(Name, Private::SelectionGroup);
		return Selection ? Selection->GetData() : nullptr;
	}

	void FCollectionClothSelectionFacade::RemoveSelectionSet(const FName& Name)
	{
		check(IsValid());
		ManagedArrayCollection->RemoveAttribute(Name, Private::SelectionGroup);
	}

	TSet<int32>& FCollectionClothSelectionFacade::FindOrAddSelectionSet(const FName& Name, const FName& GroupName)
	{
		check(IsValid());
		ensure(GroupName != NAME_None && Name != NAME_None);
		constexpr bool bAllowCircularDependency = false;

		TManagedArray<TSet<int32>>* Selection = ManagedArrayCollection->FindAttributeTyped<TSet<int32>>(Name, Private::SelectionGroup);
		if (Selection)
		{
			check(Selection->Num() == 1);  // This should always be the case if the facade is valid

			// Recycle the existing selection, with the new group if needed
			if (ManagedArrayCollection->GetDependency(Name, Private::SelectionGroup) != GroupName)
			{
				ManagedArrayCollection->SetDependency(Name, Private::SelectionGroup, GroupName, bAllowCircularDependency);
				(*Selection)[0].Reset();  // No point in keeping unrelated selection indices since the group has changed better clear everything
			}
		}
		else
		{
			// Create a new selection
			constexpr bool bSaved = true;
			const FManagedArrayCollection::FConstructionParameters GroupDependency(GroupName, bSaved, bAllowCircularDependency);
			Selection = &ManagedArrayCollection->AddAttribute<TSet<int32>>(Name, Private::SelectionGroup, GroupDependency);

			check(Selection->Num() == 1);  // This should always be the case if the facade is valid
		}

		return (*Selection)[0];
	}
}	// namespace  UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
