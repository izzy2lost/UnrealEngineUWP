// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Templates/SharedPointer.h"
#include "GeometryCollection/ManagedArray.h"

struct FManagedArrayCollection;

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth Asset collection Selection facade class. Each Selection consists of a set of integer indices, a name, and a string representing the type of element being indexed.
	 * Const access (read only) version.
	 */
	class FCollectionClothSelectionConstFacade
	{
	public:
		CHAOSCLOTHASSET_API explicit FCollectionClothSelectionConstFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothSelectionConstFacade() = delete;
		FCollectionClothSelectionConstFacade(const FCollectionClothSelectionConstFacade&) = delete;
		FCollectionClothSelectionConstFacade& operator=(const FCollectionClothSelectionConstFacade&) = delete;
		FCollectionClothSelectionConstFacade(FCollectionClothSelectionConstFacade&&) = default;
		FCollectionClothSelectionConstFacade& operator=(FCollectionClothSelectionConstFacade&&) = default;
		virtual ~FCollectionClothSelectionConstFacade() = default;

		/** Return whether the facade is defined on the collection. */
		CHAOSCLOTHASSET_API bool IsValid() const;

		/** Return the number of selections in the collection. */
		CHAOSCLOTHASSET_API int32 GetNumSelections() const;

		/** Return an array of all the selections' name in the collection. */
		CHAOSCLOTHASSET_API TArray<FName> GetNames() const;

		/** Return whether a selection with the given name cyrrently exists in the collection. */
		CHAOSCLOTHASSET_API bool HasSelection(const FName& Name) const;

		/** Return the selection group dependency. The selection must exist to call this function. */
		CHAOSCLOTHASSET_API FName GetSelectionGroup(const FName& Name) const;

		/** Get the selection set for the given selection name. The selection must exist to call this function. */
		CHAOSCLOTHASSET_API const TSet<int32>& GetSelectionSet(const FName& Name) const;

		/** Find a selection with the given name, or nullptr if no such selection exists. */
		CHAOSCLOTHASSET_API const TSet<int32>* FindSelectionSet(const FName& Name) const;

	protected:
		TSharedRef<FManagedArrayCollection> ManagedArrayCollection;
	};

	/**
	 * Cloth Asset collection Selection facade class. Each Selection consists of a set of integer indices, a name, and a string representing the type of element being indexed.
	 * Non-const access (read/write) version.
	 */
	class FCollectionClothSelectionFacade final : public FCollectionClothSelectionConstFacade
	{
	public:
		CHAOSCLOTHASSET_API explicit FCollectionClothSelectionFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothSelectionFacade() = delete;
		FCollectionClothSelectionFacade(const FCollectionClothSelectionFacade&) = delete;
		FCollectionClothSelectionFacade& operator=(const FCollectionClothSelectionFacade&) = delete;
		FCollectionClothSelectionFacade(FCollectionClothSelectionFacade&&) = default;
		FCollectionClothSelectionFacade& operator=(FCollectionClothSelectionFacade&&) = default;
		virtual ~FCollectionClothSelectionFacade() override = default;

		/** Add the Selection attributes to the underlying collection. */
		CHAOSCLOTHASSET_API void DefineSchema();

		/** Get the selection set for the given selection name. The selection must exist to call this function. */
		CHAOSCLOTHASSET_API TSet<int32>& GetSelectionSet(const FName& Name);

		/** Find a selection with the given name, or nullptr if no such selection exists. */
		CHAOSCLOTHASSET_API TSet<int32>* FindSelectionSet(const FName& Name);

		/** Remove the selection with the given name if it exists. */
		CHAOSCLOTHASSET_API void RemoveSelectionSet(const FName& Name);

		/** 
		 * Append all sets from an existing collection to this collection.
		 * Matching sets (i.e., same name and type) will be unioned.
		 * Mismatching sets (same name, different type) will be handled according to bOverwriteExistingIfMismatched
		 * 
		 * @param bOverwriteExistingIfMismatched: If true, overwrite existing mismatched sets.
		 *   If false, keep the existing set.
		 */
		CHAOSCLOTHASSET_API void Append(const FCollectionClothSelectionConstFacade& Other, bool bOverwriteExistingIfMismatched);

		/**
		 * Find, or add if it doesn't already exist, a selection for the specified group with the given name.
		 * If the group doesn't already exists, this function will create it.
		 * If the selection already exists, but depends on a different group, the old selection will be deleted and a new one recreated with the new group dependency.
		 */
		CHAOSCLOTHASSET_API TSet<int32>& FindOrAddSelectionSet(const FName& Name, const FName& GroupName);
	};
}
