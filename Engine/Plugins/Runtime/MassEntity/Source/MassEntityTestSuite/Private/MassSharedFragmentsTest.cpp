// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "MassEntityTestTypes.h"
#include "MassArchetypeData.h"
#include "MassArchetypeTypes.h"
#include "Algo/RandomShuffle.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace FMassEntityTest
{
//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
template<typename TMassSharedFragmentType>
const TMassSharedFragmentType* GetConstSharedFragmentPtr(const FMassArchetypeSharedFragmentValues& Values)
{
	FConstSharedStruct SharedStruct = Values.GetConstSharedFragmentStruct(TMassSharedFragmentType::StaticStruct());
	return SharedStruct.IsValid() ? SharedStruct.GetPtr<const TMassSharedFragmentType>() : nullptr;
}

template<typename TMassSharedFragmentType>
TMassSharedFragmentType* GetMutableSharedFragmentPtr(FMassArchetypeSharedFragmentValues& Values)
{
	FSharedStruct SharedStruct = Values.GetSharedFragmentStruct(TMassSharedFragmentType::StaticStruct());
	return SharedStruct.IsValid() ? SharedStruct.GetPtr<TMassSharedFragmentType>() : nullptr;
}

//-----------------------------------------------------------------------------
// FMassArchetypeSharedFragmentValues Tests
//-----------------------------------------------------------------------------
struct FSharedFragmentValues_Create : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 59;
		FMassArchetypeSharedFragmentValues Values;

		{
			FTestSharedFragment_Int FragmentInstance(TestIntValue);
			FSharedStruct SharedFragmentInstance = FSharedStruct::Make(FragmentInstance);
			Values.AddSharedFragment(SharedFragmentInstance);
		}

		const FTestSharedFragment_Int* ConstInstance = GetConstSharedFragmentPtr<FTestSharedFragment_Int>(Values);
		FTestSharedFragment_Int* NonConstInstance = GetMutableSharedFragmentPtr<FTestSharedFragment_Int>(Values);

		AITEST_NULL("Fetching fragment as a const shared fragment should fail", ConstInstance);
		AITEST_NOT_NULL("Fetching fragment as a shared fragment should not fail", NonConstInstance);
		AITEST_EQUAL("The fetched value should match the expectations", NonConstInstance->Value, TestIntValue);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_Create, "System.Mass.SharedFragments.CreateValue");

struct FSharedFragmentValues_CreateConst : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 59;
		FMassArchetypeSharedFragmentValues Values;

		{
			FConstSharedStruct SharedFragmentInstance = FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue);
			Values.AddConstSharedFragment(SharedFragmentInstance);
		}

		const FTestSharedFragment_Int* ConstInstance = GetConstSharedFragmentPtr<FTestSharedFragment_Int>(Values);
		FTestSharedFragment_Int* NonConstInstance = GetMutableSharedFragmentPtr<FTestSharedFragment_Int>(Values);

		AITEST_NULL("Fetching fragment as a shared fragment should fail", NonConstInstance);
		AITEST_NOT_NULL("Fetching fragment as a const shared fragment should not fail", ConstInstance);
		AITEST_EQUAL("The fetched value should match the expectations", ConstInstance->Value, TestIntValue);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_CreateConst, "System.Mass.SharedFragments.CreateConstValue");

struct FSharedFragmentValues_Contains : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 31;
		constexpr float TestFloatValue = 63.f;
		FMassArchetypeSharedFragmentValues Values;

		AITEST_FALSE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType(FTestSharedFragment_Int::StaticStruct()));
		AITEST_FALSE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType<FTestSharedFragment_Int>());

		{
			FConstSharedStruct SharedFragmentInstance = FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue);
			Values.AddConstSharedFragment(SharedFragmentInstance);
		}

		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType(FTestSharedFragment_Int::StaticStruct()));
		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType<FTestSharedFragment_Int>());

		{
			FConstSharedStruct SharedFragmentInstance = FConstSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue);
			Values.AddConstSharedFragment(SharedFragmentInstance);
		}
		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType(FTestSharedFragment_Float::StaticStruct()));
		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType<FTestSharedFragment_Float>());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_Contains, "System.Mass.SharedFragments.Contains");

struct FSharedFragmentValues_Append : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 31;
		constexpr float TestFloatValue = 63.f;
		
		FMassArchetypeSharedFragmentValues ValuesNonConstInt;
		ValuesNonConstInt.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
		FMassArchetypeSharedFragmentValues ValuesNonConstFloat;
		ValuesNonConstFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));
		FMassArchetypeSharedFragmentValues ValuestNonConstIntFloat;
		ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
		ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));

		// `Appending` int/float values to an new Values instance should result in the same result as `Adding` them
		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesNonConstInt);
			AITEST_TRUE("#1 Append results should match expectations", Values.HasSameValues(ValuesNonConstInt));
			Values.Append(ValuesNonConstFloat);
			AITEST_TRUE("#2 Append results should match expectations", Values.HasSameValues(ValuestNonConstIntFloat));
		}
		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesNonConstFloat);
			AITEST_TRUE("#3 Append results should match expectations", Values.HasSameValues(ValuesNonConstFloat));
			Values.Append(ValuesNonConstInt);
			AITEST_TRUE("#4 Append results should match expectations", Values.HasSameValues(ValuestNonConstIntFloat));
		}

		FMassArchetypeSharedFragmentValues ValuesConstInt;
		ValuesConstInt.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
		FMassArchetypeSharedFragmentValues ValuesConstFloat;
		ValuesConstFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));
		FMassArchetypeSharedFragmentValues ValuestConstIntFloat;
		ValuestConstIntFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
		ValuestConstIntFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));

		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesConstInt);
			AITEST_TRUE("#5 Append results should match expectations", Values.HasSameValues(ValuesConstInt));
			Values.Append(ValuesConstFloat);
			AITEST_TRUE("#6 Append results should match expectations", Values.HasSameValues(ValuestConstIntFloat));
		}
		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesConstFloat);
			AITEST_TRUE("#7 Append results should match expectations", Values.HasSameValues(ValuesConstFloat));
			Values.Append(ValuesConstInt);
			AITEST_TRUE("#8 Append results should match expectations", Values.HasSameValues(ValuestConstIntFloat));
		}

		// test mismatching "mode" (const vs non-const) failure
		AITEST_SCOPED_CHECK("trying to switch", 2);
		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesNonConstInt);
			AITEST_EQUAL("#1 Adding mismatching mode should fail", Values.Append(ValuesConstInt), 0);
		}
		{
			FMassArchetypeSharedFragmentValues Values;
			Values.Append(ValuesConstInt);
			AITEST_EQUAL("#2 Adding mismatching mode should fail", Values.Append(ValuesNonConstInt), 0);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_Append, "System.Mass.SharedFragments.Append");

struct FSharedFragmentValues_Remove : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 31;
		constexpr float TestFloatValue = 63.f;

		{
			FMassArchetypeSharedFragmentValues ValuesNonConstInt;
			ValuesNonConstInt.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
			FMassArchetypeSharedFragmentValues ValuesNonConstFloat;
			ValuesNonConstFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));
			FMassArchetypeSharedFragmentValues ValuestNonConstIntFloat;
			ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
			ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));

			{
				FMassArchetypeSharedFragmentValues Values = ValuestNonConstIntFloat;
				AITEST_TRUE("Assignment should result in same values", Values.HasSameValues(ValuestNonConstIntFloat));

				// removing just the Int shared fragment
				Values.Remove(ValuesNonConstInt.GetSharedFragmentBitSet());
				AITEST_TRUE("#1 Removal results should match expectations", Values.HasSameValues(ValuesNonConstFloat));
			}
			{
				FMassArchetypeSharedFragmentValues Values = ValuestNonConstIntFloat;
				// removing just the Float shared fragment
				Values.Remove(ValuesNonConstFloat.GetSharedFragmentBitSet());
				AITEST_TRUE("#2 Removal results should match expectations", Values.HasSameValues(ValuesNonConstInt));
			}
		}
		{
			FMassArchetypeSharedFragmentValues ValuesConstInt;
			ValuesConstInt.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
			FMassArchetypeSharedFragmentValues ValuesConstFloat;
			ValuesConstFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));
			FMassArchetypeSharedFragmentValues ValuestConstIntFloat;
			ValuestConstIntFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
			ValuestConstIntFloat.AddConstSharedFragment(FConstSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));

			{
				FMassArchetypeSharedFragmentValues Values = ValuestConstIntFloat;
				AITEST_TRUE("Assignment should result in same values", Values.HasSameValues(ValuestConstIntFloat));

				// removing just the Int shared fragment
				Values.Remove(ValuesConstInt.GetSharedFragmentBitSet());
				AITEST_TRUE("#3 Removal results should match expectations", Values.HasSameValues(ValuesConstFloat));
			}
			{
				FMassArchetypeSharedFragmentValues Values = ValuestConstIntFloat;
				// removing just the Float shared fragment
				Values.Remove(ValuesConstFloat.GetSharedFragmentBitSet());
				AITEST_TRUE("#4 Removal results should match expectations", Values.HasSameValues(ValuesConstInt));
			}
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_Remove, "System.Mass.SharedFragments.Remove");

struct FSharedFragmentValues_Hash : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 31;
		constexpr float TestFloatValue = 63.f;

		FMassArchetypeSharedFragmentValues ValuestNonConstIntFloat;
		ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue));
		ValuestNonConstIntFloat.AddSharedFragment(FSharedStruct::Make<FTestSharedFragment_Float>(TestFloatValue));

		AITEST_SCOPED_CHECK("Expecting the containers to be sorted", 1);
		const uint32 EmptyHash = ValuestNonConstIntFloat.CalculateHash();
		AITEST_EQUAL("Expecting unsorted collection hashing to result in 0", EmptyHash, 0u);

		ValuestNonConstIntFloat.Sort();
		const uint32 ValidHash = ValuestNonConstIntFloat.CalculateHash();
		AITEST_NOT_EQUAL("Expecting sorted collection hashing to result in non 0", ValidHash, 0u);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragmentValues_Hash, "System.Mass.SharedFragments.Hash");

//-----------------------------------------------------------------------------
// Entity-related Tests
//-----------------------------------------------------------------------------
struct FSharedFragmentBase : public FEntityTestBase
{
	template<typename TSharedStruct, typename TSharedFragment>
	FConstStructView GetSharedFragmentView(const FMassEntityHandle EntityHandle)
	{
		if constexpr (TIsDerivedFrom<TSharedStruct, FSharedStruct>::IsDerived)
		{
			return EntityManager->GetSharedFragmentDataStruct(EntityHandle, TSharedFragment::StaticStruct());
		}
		else
		{
			return EntityManager->GetConstSharedFragmentDataStruct(EntityHandle, TSharedFragment::StaticStruct());
		}
	};

	template<typename TSharedStruct, typename TSharedFragment>
	void CreateEntities(TArray<FMassEntityHandle>& OutEntityHandles, const int32 NumToCreate, const typename TSharedFragment::FValueType TestValue)
	{
		TSharedFragment FragmentInstance(TestValue);
		FMassArchetypeSharedFragmentValues SharedIntValues;

		TSharedStruct SharedFragmentInstance = TSharedStruct::Make(FragmentInstance);

		if constexpr (TIsDerivedFrom<TSharedStruct, FSharedStruct>::IsDerived)
		{
			SharedIntValues.AddSharedFragment(SharedFragmentInstance);
		}
		else
		{
			SharedIntValues.AddConstSharedFragment(SharedFragmentInstance);
		}

		EntityManager->BatchCreateEntities(FloatsArchetype, SharedIntValues, NumToCreate, OutEntityHandles);
	}

	template<typename TSharedStruct, typename TSharedFragment>
	void CreateEntity(FMassEntityHandle& OutEntityHandle, const typename TSharedFragment::FValueType TestValue)
	{
		TSharedFragment FragmentInstance(TestValue);
		FMassArchetypeSharedFragmentValues SharedIntValues;

		TSharedStruct SharedFragmentInstance = TSharedStruct::Make(FragmentInstance);

		if constexpr (TIsDerivedFrom<TSharedStruct, FSharedStruct>::IsDerived)
		{
			SharedIntValues.AddSharedFragment(SharedFragmentInstance);
		}
		else
		{
			SharedIntValues.AddConstSharedFragment(SharedFragmentInstance);
		}

		OutEntityHandle = EntityManager->CreateEntity(FloatsArchetype, SharedIntValues);
	}
};

template<typename TSharedStruct>
struct FSharedFragment_CreateEntitesWithSharedFragment : public FSharedFragmentBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValueA = 1023;
		constexpr int32 TestIntValueB = 63;
		FMassEntityHandle EntityA, EntityB;

		CreateEntity<TSharedStruct, FTestSharedFragment_Int>(EntityA, TestIntValueA);
		CreateEntity<TSharedStruct, FTestSharedFragment_Int>(EntityB, TestIntValueB);

		AITEST_EQUAL("Both entities should end up in the same archetype", EntityManager->GetArchetypeForEntityUnsafe(EntityA)
			, EntityManager->GetArchetypeForEntityUnsafe(EntityB));

		/*FConstStructView SharedFragmentA, SharedFragmentB;
		if constexpr (TIsDerivedFrom<TSharedStruct, FSharedStruct>::IsDerived)
		{
			SharedFragmentA = EntityManager->GetSharedFragmentDataStruct(EntityA, FTestSharedFragment_Int::StaticStruct());
			SharedFragmentB = EntityManager->GetSharedFragmentDataStruct(EntityB, FTestSharedFragment_Int::StaticStruct());
		}
		else
		{
			SharedFragmentA = EntityManager->GetConstSharedFragmentDataStruct(EntityA, FTestSharedFragment_Int::StaticStruct());
			SharedFragmentB = EntityManager->GetConstSharedFragmentDataStruct(EntityB, FTestSharedFragment_Int::StaticStruct());
		}*/
		FConstStructView SharedFragmentA = GetSharedFragmentView<TSharedStruct, FTestSharedFragment_Int>(EntityA);
		FConstStructView SharedFragmentB = GetSharedFragmentView<TSharedStruct, FTestSharedFragment_Int>(EntityB);

		AITEST_TRUE("SharedFragmentA should be valid", SharedFragmentA.IsValid());
		AITEST_TRUE("SharedFragmentB should be valid", SharedFragmentB.IsValid());
		AITEST_EQUAL("SharedFragmentA should be of expected type", SharedFragmentA.GetScriptStruct(), FTestSharedFragment_Int::StaticStruct());
		AITEST_EQUAL("SharedFragmentB should be of expected type", SharedFragmentB.GetScriptStruct(), FTestSharedFragment_Int::StaticStruct());
		AITEST_NOT_EQUAL("SharedFragmentA and SharedFragmentB should be different instanceS", SharedFragmentA, SharedFragmentB);
		AITEST_NOT_EQUAL("SharedFragmentA and SharedFragmentB should be distinct", SharedFragmentA, SharedFragmentB);
		AITEST_EQUAL("SharedFragmentA's value should match the expected value", SharedFragmentA.Get<const FTestSharedFragment_Int>().Value, TestIntValueA);
		AITEST_EQUAL("SharedFragmentB's value should match the expected value", SharedFragmentB.Get<const FTestSharedFragment_Int>().Value, TestIntValueB);

		return true;
	}
};
using FSharedFragment_CreateEntitesWithNonConstSharedFragment = FSharedFragment_CreateEntitesWithSharedFragment<FSharedStruct>;
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_CreateEntitesWithNonConstSharedFragment, "System.Mass.SharedFragments.CreateEntities");
using FSharedFragment_CreateEntitesWithConstSharedFragment = FSharedFragment_CreateEntitesWithSharedFragment<FConstSharedStruct>;
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_CreateEntitesWithConstSharedFragment, "System.Mass.SharedFragments.CreateEntitiesConst");

template<typename TSharedStruct>
struct FSharedFragment_BatchCreateEntitesWithSharedFragment : public FSharedFragmentBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValueA = 1023;
		constexpr int32 TestIntValueB = 63;
		const int32 EntitiesPerChunk = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(FloatsArchetype).GetNumEntitiesPerChunk();
		// we create more than one chunk can handle properly test moving entities between chunks
		const int32 EntitiesToCreateNumA = FMath::FloorToInt(float(EntitiesPerChunk) * 1.2f);
		const int32 EntitiesToCreateNumB = 1;
		constexpr int32 ExpectedNumberOfInitialChunks = 3;
		TArray<FMassEntityHandle> EntitiesA;
		TArray<FMassEntityHandle> EntitiesB;

		CreateEntities<TSharedStruct, FTestSharedFragment_Int>(EntitiesA, EntitiesToCreateNumA, TestIntValueA);
		CreateEntities<TSharedStruct, FTestSharedFragment_Int>(EntitiesB, EntitiesToCreateNumB, TestIntValueB);

		FMassArchetypeHandle CommonArchetype = EntityManager->GetArchetypeForEntityUnsafe(EntitiesA[0]);
		AITEST_EQUAL("All the entities should end up in the same archetype"
			, FMassArchetypeHelper::ArchetypeDataFromHandleChecked(CommonArchetype).GetNumEntities(), EntitiesToCreateNumA + EntitiesToCreateNumB);
		AITEST_EQUAL("The total number of chunks in the resulting archetype should match expectations"
			, FMassArchetypeHelper::ArchetypeDataFromHandleChecked(CommonArchetype).GetChunkCount(), ExpectedNumberOfInitialChunks);

		for (FMassEntityHandle EntityHandle : EntitiesA)
		{
			FConstStructView SharedFragment = GetSharedFragmentView<TSharedStruct, FTestSharedFragment_Int>(EntityHandle);
			AITEST_TRUE("SharedFragment for entity type A should be valid", SharedFragment.IsValid());
			AITEST_EQUAL("SharedFragment for entity type A  should be of expected type", SharedFragment.GetScriptStruct(), FTestSharedFragment_Int::StaticStruct());
			AITEST_EQUAL("SharedFragment's value for entity type A should match the expected value", SharedFragment.Get<const FTestSharedFragment_Int>().Value, TestIntValueA);
		}

		FConstStructView SharedFragment = GetSharedFragmentView<TSharedStruct, FTestSharedFragment_Int>(EntitiesB[0]);
		AITEST_TRUE("SharedFragment for entity type B should be valid", SharedFragment.IsValid());
		AITEST_EQUAL("SharedFragment for entity type B  should be of expected type", SharedFragment.GetScriptStruct(), FTestSharedFragment_Int::StaticStruct());
		AITEST_EQUAL("SharedFragment's value for entity type B should match the expected value", SharedFragment.Get<const FTestSharedFragment_Int>().Value, TestIntValueB);

		return true;
	}
};
using FSharedFragment_BatchCreateEntitesWithNonConstSharedFragment = FSharedFragment_BatchCreateEntitesWithSharedFragment<FSharedStruct>;
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_BatchCreateEntitesWithNonConstSharedFragment, "System.Mass.SharedFragments.BatchCreateEntities");
using FSharedFragment_BatchCreateEntitesWithConstSharedFragment = FSharedFragment_BatchCreateEntitesWithSharedFragment<FConstSharedStruct>;
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_BatchCreateEntitesWithConstSharedFragment, "System.Mass.SharedFragments.BatchCreateEntitiesConst");


struct FSharedFragment_AddToEntity : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 1023;
		FTestSharedFragment_Int FragmentInstance(TestIntValue);
		FSharedStruct SharedFragmentInstance = FSharedStruct::Make(FragmentInstance);

		const FMassEntityHandle EntityHandle = EntityManager->CreateEntity(FloatsArchetype);

		FTestSharedFragment_Int* EntitySharedFragment = EntityManager->GetSharedFragmentDataPtr<FTestSharedFragment_Int>(EntityHandle);
		AITEST_NULL("Initially the entity is not expected to have the shared fragment", EntitySharedFragment);

		EntityManager->AddConstSharedFragmentToEntity(EntityHandle, SharedFragmentInstance);

		EntitySharedFragment = EntityManager->GetConstSharedFragmentDataPtr<FTestSharedFragment_Int>(EntityHandle);
		AITEST_NOT_NULL("The entity is expected to have the shared fragment after the operation", EntitySharedFragment);
		AITEST_EQUAL("The the shared fragment is expected to store the configured value", EntitySharedFragment->Value, TestIntValue);

		// at this point the Entity already has a shared fragment of a given type
		// now we're going to add it again and test the systems behavior, we'll be adding the same FMasSharedFragment type
		// in both const and non-const way.
		constexpr int32 DifferentTestIntValue = TestIntValue + 1;
		FTestSharedFragment_Int DifferentFragmentInstance(DifferentTestIntValue);
		FSharedStruct DifferentSharedFragmentInstance = FSharedStruct::Make(DifferentFragmentInstance);
		FConstSharedStruct DifferentConstSharedFragmentInstance = FConstSharedStruct::Make(DifferentFragmentInstance);

		GetTestRunner().AddExpectedError(TEXT("Changing shared fragment value of entities is not supported"), EAutomationExpectedErrorFlags::Contains, 2);

		const bool bSuccessfullyAddedSharedFragment = EntityManager->AddConstSharedFragmentToEntity(EntityHandle, DifferentSharedFragmentInstance);
		AITEST_FALSE("Adding existing shared fragment type should fail", bSuccessfullyAddedSharedFragment);
		const bool bSuccessfullyAddedConstSharedFragment = EntityManager->AddConstSharedFragmentToEntity(EntityHandle, DifferentConstSharedFragmentInstance);
		AITEST_FALSE("Adding existing const shared fragment type should fail", bSuccessfullyAddedConstSharedFragment);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_AddToEntity, "System.Mass.SharedFragments.AddToEntity");

struct FSharedFragment_BatchAddToEntity : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 1023;

		const FMassArchetypeHandle InitialArchetype = FloatsArchetype;
		const int32 EntitiesPerChunk = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(InitialArchetype).GetNumEntitiesPerChunk();
		const int32 EntitiesToCreateNum = FMath::FloorToInt(float(EntitiesPerChunk) * 2.2f);
		const int32 EntitiesToMoveNum = FMath::FloorToInt(float(EntitiesPerChunk) * 1.2f);

		TArray<FMassEntityHandle> CreatedEntityHandles;

		EntityManager->BatchCreateEntities(InitialArchetype, EntitiesToCreateNum, CreatedEntityHandles);

		TArray<FMassEntityHandle> EntitiesToMove = CreatedEntityHandles;
		Algo::RandomShuffle(EntitiesToMove);
		TConstArrayView<FMassEntityHandle> EntitiesMoved = MakeArrayView(EntitiesToMove.GetData(), EntitiesToMoveNum);
		FMassArchetypeEntityCollection EntityCollection(InitialArchetype, EntitiesMoved, FMassArchetypeEntityCollection::EDuplicatesHandling::NoDuplicates);

		FMassArchetypeSharedFragmentValues SharedValues;
		FConstSharedStruct ConstSharedFragment = FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue);
		SharedValues.AddConstSharedFragment(ConstSharedFragment);
		EntityManager->BatchAddSharedFragmentsForEntities(MakeArrayView(&EntityCollection, 1), SharedValues);

		const FMassArchetypeHandle TargetArchetype = EntityManager->GetArchetypeForEntityUnsafe(EntitiesToMove[0]);
		const int32 EntitiesMovedNum = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(TargetArchetype).GetNumEntities();
		AITEST_EQUAL("Number of entities moves needs to match expectations", EntitiesMovedNum, EntitiesToMoveNum);
		for (const FMassEntityHandle& EntityHandle : EntitiesMoved)
		{
			FTestSharedFragment_Int* SharedFragmentInstance = EntityManager->GetConstSharedFragmentDataPtr<FTestSharedFragment_Int>(EntityHandle);
			AITEST_NOT_NULL("Every entity moved needs to have a valid shared fragment", SharedFragmentInstance);
			AITEST_EQUAL("The shared fragment's value needs to match expectations", SharedFragmentInstance->Value, TestIntValue);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_BatchAddToEntity, "System.Mass.SharedFragments.BatchAddToEntity");

struct FSharedFragment_BatchSetAttempt : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 TestIntValue = 1023;
		constexpr int32 OtherTestIntValue = TestIntValue + 1;

		const int32 EntitiesPerChunk = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(FloatsArchetype).GetNumEntitiesPerChunk();
		const int32 EntitiesToCreateNum = FMath::FloorToInt(float(EntitiesPerChunk) * 2.2f);
		const int32 EntitiesToMoveNum = FMath::FloorToInt(float(EntitiesPerChunk) * 1.2f);

		TArray<FMassEntityHandle> CreatedEntityHandles;
		FMassArchetypeSharedFragmentValues SharedIntValues;
		FConstSharedStruct ConstSharedFragment = FConstSharedStruct::Make<FTestSharedFragment_Int>(TestIntValue);
		SharedIntValues.AddConstSharedFragment(ConstSharedFragment);

		TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(FloatsArchetype, SharedIntValues, EntitiesToCreateNum, CreatedEntityHandles);
		const FMassArchetypeHandle ResultingArchetype = CreationContext->GetEntityCollection().GetArchetype();

		FMassArchetypeEntityCollection EntityCollection(ResultingArchetype, CreatedEntityHandles, FMassArchetypeEntityCollection::EDuplicatesHandling::NoDuplicates);
		
		// attempting to add the same values again should fail with checks and ensures
		{	
			AITEST_SCOPED_CHECK("Setting shared fragment values without archetype change is not supported", 1);
			AITEST_SCOPED_CHECK("Trying to set shared fragment values, without adding new shared fragments", 1);
			EntityManager->BatchAddSharedFragmentsForEntities(MakeArrayView(&EntityCollection, 1), SharedIntValues);
		}
		{
			FMassArchetypeSharedFragmentValues DifferentSharedIntValues;
			FConstSharedStruct OtherConstSharedFragment = FConstSharedStruct::Make<FTestSharedFragment_Int>(OtherTestIntValue);
			DifferentSharedIntValues.AddConstSharedFragment(OtherConstSharedFragment);

			AITEST_SCOPED_CHECK("Setting shared fragment values without archetype change is not supported", 1);
			AITEST_SCOPED_CHECK("Trying to set shared fragment values, without adding new shared fragments", 1);
			EntityManager->BatchAddSharedFragmentsForEntities(MakeArrayView(&EntityCollection, 1), DifferentSharedIntValues);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_BatchSetAttempt, "System.Mass.SharedFragments.BatchSetAttempt");

struct FSharedFragment_BatchAddToEmpty : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 NumToReserve = 32;

		FMassArchetypeSharedFragmentValues SharedIntValues;
		FConstSharedStruct ConstSharedFragment = FConstSharedStruct::Make<FTestSharedFragment_Int>();
		SharedIntValues.AddConstSharedFragment(ConstSharedFragment);

		TArray<FMassEntityHandle> ReservedEntityHandles;
		EntityManager->BatchReserveEntities(NumToReserve, ReservedEntityHandles);
		
		FMassArchetypeEntityCollection EntityCollection(FMassArchetypeHandle(), ReservedEntityHandles, FMassArchetypeEntityCollection::EDuplicatesHandling::NoDuplicates);
		// attempting to add the values before the entities are created is not a valid operation
		{
			AITEST_SCOPED_CHECK("Adding shared fragments to archetype-less entities is not supported", 1);
			EntityManager->BatchAddSharedFragmentsForEntities(MakeArrayView(&EntityCollection, 1), SharedIntValues);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSharedFragment_BatchAddToEmpty, "System.Mass.SharedFragments.BatchAddToEmpty");

} // FMassEntityTest

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
