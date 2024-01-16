// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathName.h"

#if WITH_TESTS

#include "Misc/StringBuilder.h"
#include "Tests/TestHarnessAdapter.h"

namespace UE
{

TEST_CASE_NAMED(FPropertyPathNameTest, "CoreUObject::PropertyPathName", "[Core][UObject][SmokeFilter]")
{
	const FName CountName(TEXTVIEW("Count"));

	FPropertyTypeNameBuilder IntTypeBuilder;
	IntTypeBuilder.AddTypeName(NAME_IntProperty);
	const FPropertyTypeName IntType = IntTypeBuilder.Build();

	SECTION("Empty")
	{
		FPropertyPathName PathName;
		CHECK(PathName.IsEmpty());
		CHECK(PathName.GetSegmentCount() == 0);
		CHECK(WriteToString<16>(PathName).Len() == 0);
	}

	SECTION("Name")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName});
		CHECK(!PathName.IsEmpty());
		CHECK(PathName.GetSegmentCount() == 1);
		CHECK(PathName.GetSegment(0).Name == CountName);
		CHECK(PathName.GetSegment(0).Type.IsEmpty());
		CHECK(PathName.GetSegment(0).Index == INDEX_NONE);
		CHECK(TEXTVIEW("Count").Equals(WriteToString<16>(PathName)));
	}

	SECTION("NameType")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName, IntType});
		CHECK(!PathName.IsEmpty());
		CHECK(PathName.GetSegmentCount() == 1);
		CHECK(PathName.GetSegment(0).Name == CountName);
		CHECK(PathName.GetSegment(0).Type == IntType);
		CHECK(PathName.GetSegment(0).Index == INDEX_NONE);
		CHECK(TEXTVIEW("Count (IntProperty)").Equals(WriteToString<16>(PathName)));
	}

	SECTION("NameTypeIndex")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName, IntType, 7});
		CHECK(!PathName.IsEmpty());
		CHECK(PathName.GetSegmentCount() == 1);
		CHECK(PathName.GetSegment(0).Name == CountName);
		CHECK(PathName.GetSegment(0).Type == IntType);
		CHECK(PathName.GetSegment(0).Index == 7);
		CHECK(TEXTVIEW("Count[7] (IntProperty)").Equals(WriteToString<16>(PathName)));
	}

	SECTION("MultipleSegments")
	{
		FPropertyTypeNameBuilder ArrayTypeBuilder;
		ArrayTypeBuilder.AddTypeName(NAME_ArrayProperty);
		ArrayTypeBuilder.BeginTypeParameters();
		ArrayTypeBuilder.AddTypeName(NAME_StructProperty);
		ArrayTypeBuilder.BeginTypeParameters();
		ArrayTypeBuilder.AddTypeName(TEXT("TestType"));
		ArrayTypeBuilder.EndTypeParameters();
		ArrayTypeBuilder.EndTypeParameters();

		FPropertyTypeNameBuilder VectorTypeBuilder;
		VectorTypeBuilder.AddTypeName(NAME_StructProperty);
		VectorTypeBuilder.BeginTypeParameters();
		VectorTypeBuilder.AddTypeName(TEXT("IntVector"));
		VectorTypeBuilder.EndTypeParameters();

		FPropertyPathName PathName;
		PathName.Push({TEXT("TestArray"), ArrayTypeBuilder.Build(), 7});
		PathName.Push({TEXT("Position"), VectorTypeBuilder.Build()});
		PathName.Push({TEXT("X"), IntType});

		CHECK(PathName.GetSegmentCount() == 3);
		CHECK(TEXTVIEW("TestArray[7] (ArrayProperty<StructProperty<TestType>>) -> Position (StructProperty<IntVector>) -> X (IntProperty)").Equals(WriteToString<128>(PathName)));
	}

	SECTION("SetType/SetIndex")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName});
		PathName.Push({CountName});

		PathName.SetType(IntType);
		CHECK(PathName.GetSegment(0).Type.IsEmpty());
		CHECK(PathName.GetSegment(1).Type == IntType);

		PathName.SetIndex(7);
		CHECK(PathName.GetSegment(0).Index == INDEX_NONE);
		CHECK(PathName.GetSegment(1).Index == 7);
	}

	SECTION("Pop/Empty/Reset")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName, IntType, 0});
		PathName.Push({CountName, IntType, 1});
		PathName.Push({CountName, IntType, 2});
		CHECK(PathName.GetSegmentCount() == 3);
		CHECK(PathName.GetSegment(2).Index == 2);

		SECTION("Pop")
		{
			PathName.Pop();
			CHECK(PathName.GetSegmentCount() == 2);
			CHECK(PathName.GetSegment(1).Index == 1);
			PathName.Pop();
			CHECK(PathName.GetSegmentCount() == 1);
			CHECK(PathName.GetSegment(0).Index == 0);
			PathName.Pop();
			CHECK(PathName.GetSegmentCount() == 0);
			CHECK(PathName.IsEmpty());
		}

		SECTION("Empty")
		{
			PathName.Empty();
			CHECK(PathName.GetSegmentCount() == 0);
			CHECK(PathName.IsEmpty());
		}

		SECTION("Reset")
		{
			PathName.Reset();
			CHECK(PathName.GetSegmentCount() == 0);
			CHECK(PathName.IsEmpty());
		}
	}

	SECTION("GetTypeHash")
	{
		FPropertyPathName Empty, Name, NameType, NameTypeIndex;
		Name.Push({CountName});
		NameType.Push({CountName, IntType});
		NameTypeIndex.Push({CountName, IntType, 7});

		const uint32 EmptyHash = GetTypeHash(Empty);
		const uint32 NameHash = GetTypeHash(Name);
		const uint32 NameTypeHash = GetTypeHash(NameType);
		const uint32 NameTypeIndexHash = GetTypeHash(NameTypeIndex);

		CHECK(EmptyHash == 0);
		CHECK(EmptyHash != NameHash);
		CHECK(EmptyHash != NameTypeHash);
		CHECK(EmptyHash != NameTypeIndexHash);
		CHECK(NameHash != NameTypeHash);
		CHECK(NameTypeHash != NameTypeIndexHash);

		NameTypeIndex.SetIndex(8);
		CHECK(NameTypeIndexHash != GetTypeHash(NameTypeIndex));
	}
}

} // UE

#endif // WITH_TESTS
