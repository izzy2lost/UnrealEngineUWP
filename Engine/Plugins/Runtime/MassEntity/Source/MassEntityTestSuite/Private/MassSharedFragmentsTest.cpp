// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "MassEntityTestTypes.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace FMassEntityTest
{

struct FSharedFragmentValues_Contains : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		FMassArchetypeSharedFragmentValues Values;

		AITEST_FALSE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType(FTestSharedFragment_Int::StaticStruct()));
		AITEST_FALSE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType<FTestSharedFragment_Int>());

		{
			FTestSharedFragment_Int FragmentInstance(1);
			FConstSharedStruct SharedFragmentInstance;
			SharedFragmentInstance.InitializeAs(FragmentInstance);
			Values.AddConstSharedFragment(SharedFragmentInstance);
		}

		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType(FTestSharedFragment_Int::StaticStruct()));
		AITEST_TRUE("Empty FMassArchetypeSharedFragmentValues should fail ContainsType tests"
			, Values.ContainsType<FTestSharedFragment_Int>());

		{
			FTestSharedFragment_Float FragmentInstance(1.f);
			FConstSharedStruct SharedFragmentInstance;
			SharedFragmentInstance.InitializeAs(FragmentInstance);
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

} // FMassEntityTest

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
