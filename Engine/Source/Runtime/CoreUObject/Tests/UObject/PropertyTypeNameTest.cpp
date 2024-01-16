// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTypeName.h"

#if WITH_TESTS

#include "Async/ParallelFor.h"
#include "Misc/StringBuilder.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/TestHarnessAdapter.h"

namespace UE
{

namespace PropertyTypeNameTest
{

static FPropertyTypeName CreateInt()
{
	FPropertyTypeNameBuilder Builder;
	Builder.AddTypeName(NAME_IntProperty);
	return Builder.Build();
}

static FPropertyTypeName CreateVectorArray()
{
	FPropertyTypeNameBuilder Builder;
	Builder.AddTypeName(NAME_ArrayProperty);
	{
		Builder.BeginTypeParameters();
		Builder.AddTypeName(NAME_StructProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_Vector);
			Builder.EndTypeParameters();
		}
		Builder.EndTypeParameters();
	}
	return Builder.Build();
}

static FPropertyTypeName CreateEnumMap()
{
	FPropertyTypeNameBuilder Builder;
	Builder.AddTypeName(NAME_MapProperty);
	{
		Builder.BeginTypeParameters();
		Builder.AddTypeName(NAME_EnumProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_ByteProperty);
			{
				Builder.BeginTypeParameters();
				Builder.AddTypeName(TEXT("Key"));
				Builder.EndTypeParameters();
			}
			Builder.EndTypeParameters();
		}
		Builder.AddTypeName(NAME_EnumProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_ByteProperty);
			{
				Builder.BeginTypeParameters();
				Builder.AddTypeName(TEXT("Value"));
				Builder.EndTypeParameters();
			}
			Builder.EndTypeParameters();
		}
		Builder.EndTypeParameters();
	}
	return Builder.Build();
}

} // PropertyTypeNameTest

TEST_CASE_NAMED(FPropertyTypeNameSmokeTest, "CoreUObject::PropertyTypeName::Smoke", "[Core][UObject][SmokeFilter]")
{
	SECTION("Empty")
	{
		FPropertyTypeName TypeName;
		CHECK(TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName().IsNone());
		CHECK(TypeName.GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).IsEmpty());
	}

	SECTION("Numeric")
	{
		FPropertyTypeName TypeName = PropertyTypeNameTest::CreateInt();
		CHECK(!TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName() == NAME_IntProperty);
		CHECK(TypeName.GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).IsEmpty());
	}

	SECTION("NumericArray")
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddTypeName(NAME_ArrayProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_IntProperty);
			Builder.EndTypeParameters();
		}

		FPropertyTypeName TypeName = Builder.Build();
		CHECK(!TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName() == NAME_ArrayProperty);
		CHECK(TypeName.GetTypeParameterCount() == 1);
		CHECK(TypeName.GetTypeParameter(0).GetTypeName() == NAME_IntProperty);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).IsEmpty());
	}

	SECTION("StructArray")
	{
		FPropertyTypeName TypeName = PropertyTypeNameTest::CreateVectorArray();
		CHECK(!TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName() == NAME_ArrayProperty);
		CHECK(TypeName.GetTypeParameterCount() == 1);
		CHECK(TypeName.GetTypeParameter(0).GetTypeName() == NAME_StructProperty);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameterCount() == 1);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeName() == NAME_Vector);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(1).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).IsEmpty());
	}

	SECTION("NumericMap")
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddTypeName(NAME_MapProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_Int32Property);
			Builder.AddTypeName(NAME_UInt32Property);
			Builder.EndTypeParameters();
		}

		FPropertyTypeName TypeName = Builder.Build();
		CHECK(!TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName() == NAME_MapProperty);
		CHECK(TypeName.GetTypeParameterCount() == 2);
		CHECK(TypeName.GetTypeParameter(0).GetTypeName() == NAME_Int32Property);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).GetTypeName() == NAME_UInt32Property);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(2).IsEmpty());
	}

	SECTION("EnumMap")
	{
		FPropertyTypeName TypeName = PropertyTypeNameTest::CreateEnumMap();
		CHECK(!TypeName.IsEmpty());
		CHECK(TypeName.GetTypeName() == NAME_MapProperty);
		CHECK(TypeName.GetTypeParameterCount() == 2);
		CHECK(TypeName.GetTypeParameter(0).GetTypeName() == NAME_EnumProperty);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeName() == NAME_ByteProperty);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(0).GetTypeName() == TEXT("Key"));
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(1).IsEmpty());
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(1).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).GetTypeName() == NAME_EnumProperty);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).GetTypeName() == NAME_ByteProperty);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).GetTypeParameter(0).GetTypeName() == TEXT("Value"));
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).GetTypeParameter(0).GetTypeParameter(0).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).GetTypeParameter(1).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(1).IsEmpty());
		CHECK(TypeName.GetTypeParameter(2).IsEmpty());
	}

	SECTION("Equals+Less+GetTypeHash")
	{
		const FPropertyTypeName Int = PropertyTypeNameTest::CreateInt();
		const FPropertyTypeName VectorArray = PropertyTypeNameTest::CreateVectorArray();

		FPropertyTypeNameBuilder Builder;
		Builder.AddTypeName(NAME_MapProperty);
		Builder.BeginTypeParameters();
		Builder.AddTypeName(NAME_IntProperty);
		Builder.AddTypeName(NAME_IntProperty);
		Builder.EndTypeParameters();
		const FPropertyTypeName MapIntToInt = Builder.Build();

		Builder.Reset();
		Builder.AddTypeName(NAME_MapProperty);
		Builder.BeginTypeParameters();
		Builder.AddTypeName(NAME_IntProperty);
		Builder.AddTypeName(NAME_MapProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(NAME_IntProperty);
			Builder.AddTypeName(NAME_IntProperty);
			Builder.EndTypeParameters();
		}
		Builder.EndTypeParameters();
		const FPropertyTypeName MapIntToIntToInt = Builder.Build();

		CHECK(FPropertyTypeName() == FPropertyTypeName());
		CHECK(GetTypeHash(FPropertyTypeName()) == GetTypeHash(FPropertyTypeName()));

		CHECK(Int == CopyTemp(Int));
		CHECK_FALSE(Int < CopyTemp(Int));
		CHECK(GetTypeHash(Int) == GetTypeHash(CopyTemp(Int)));

		CHECK_FALSE(Int == VectorArray);
		CHECK_FALSE(Int == FPropertyTypeName());
		CHECK_FALSE(VectorArray < VectorArray);
		CHECK_FALSE(Int < VectorArray);
		CHECK(VectorArray < Int);
		CHECK_FALSE(GetTypeHash(Int) == GetTypeHash(VectorArray));
		CHECK_FALSE(GetTypeHash(Int) == GetTypeHash(FPropertyTypeName()));

		CHECK(Int == MapIntToInt.GetTypeParameter(0));
		CHECK_FALSE(Int == MapIntToInt);
		CHECK_FALSE(MapIntToInt == Int);
		CHECK_FALSE(MapIntToInt < MapIntToInt);
		CHECK(Int < MapIntToInt);
		CHECK_FALSE(MapIntToInt < Int);
		CHECK_FALSE(MapIntToInt < VectorArray);
		CHECK(VectorArray < MapIntToInt);
		CHECK(GetTypeHash(Int) == GetTypeHash(MapIntToInt.GetTypeParameter(0)));
		CHECK_FALSE(GetTypeHash(Int) == GetTypeHash(MapIntToInt));
		CHECK_FALSE(GetTypeHash(MapIntToInt) == GetTypeHash(Int));

		CHECK(MapIntToInt == MapIntToIntToInt.GetTypeParameter(1));
		CHECK_FALSE(MapIntToInt == MapIntToIntToInt);
		CHECK_FALSE(MapIntToIntToInt < MapIntToIntToInt);
		CHECK(MapIntToInt < MapIntToIntToInt);
		CHECK_FALSE(MapIntToIntToInt < MapIntToInt);
		CHECK_FALSE(MapIntToIntToInt < VectorArray);
		CHECK(VectorArray < MapIntToIntToInt);
		CHECK(GetTypeHash(MapIntToInt) == GetTypeHash(MapIntToIntToInt.GetTypeParameter(1)));
		CHECK_FALSE(GetTypeHash(MapIntToInt) == GetTypeHash(MapIntToIntToInt));
	}

	SECTION("Archive")
	{
		FPropertyTypeName Empty;
		FPropertyTypeName Int = PropertyTypeNameTest::CreateInt();
		FPropertyTypeName EnumMap = PropertyTypeNameTest::CreateEnumMap();
		FPropertyTypeName VectorArray = PropertyTypeNameTest::CreateVectorArray();

		TArray<uint8> PersistentData;
		FMemoryWriter PersistentSaveAr(PersistentData, /*bIsPersistent*/ true);
		PersistentSaveAr << Empty << Int << EnumMap << VectorArray;

		TArray<uint8> MemoryData;
		FMemoryWriter MemorySaveAr(MemoryData, /*bIsPersistent*/ false);
		MemorySaveAr << Empty << Int << EnumMap << VectorArray;

		FPropertyTypeName EmptyCopy = Int; // needs to be non-empty
		FPropertyTypeName IntCopy;
		FPropertyTypeName EnumMapCopy;
		FPropertyTypeName VectorArrayCopy;

		FMemoryReader PersistentReader(PersistentData, /*bIsPersistent*/ true);
		PersistentReader << EmptyCopy << IntCopy << EnumMapCopy << VectorArrayCopy;
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);

		EmptyCopy = Int; // needs to be non-empty
		IntCopy.Reset();
		EnumMapCopy.Reset();
		VectorArrayCopy.Reset();

		FMemoryReader MemoryReader(MemoryData, /*bIsPersistent*/ false);
		MemoryReader << EmptyCopy << IntCopy << EnumMapCopy << VectorArrayCopy;
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);
	}

	SECTION("String")
	{
		CHECK(TEXTVIEW("None").Equals(WriteToString<128>(FPropertyTypeName())));
		CHECK(TEXTVIEW("IntProperty").Equals(WriteToString<128>(PropertyTypeNameTest::CreateInt())));
		CHECK(TEXTVIEW("MapProperty<EnumProperty<ByteProperty<Key>>,EnumProperty<ByteProperty<Value>>>").Equals(WriteToString<128>(PropertyTypeNameTest::CreateEnumMap())));
		CHECK(TEXTVIEW("ArrayProperty<StructProperty<Vector>>").Equals(WriteToString<128>(PropertyTypeNameTest::CreateVectorArray())));
	}
}

TEST_CASE_NAMED(FPropertyTypeNameConcurrencyTest, "CoreUObject::PropertyTypeName::Concurrency", "[Core][UObject][EngineFilter]")
{
	ParallelFor(TEXT("PropertyTypeNameConcurrencyTest"), 64 * 1024, 1, [](int32 Index)
	{
		FPropertyTypeNameBuilder Builder;

		if (Index % 8 == 0)
		{
			Builder.AddTypeName(NAME_OptionalProperty);
			Builder.BeginTypeParameters();
		}
		if (Index % 4 == 0)
		{
			Builder.AddTypeName(NAME_ArrayProperty);
			Builder.BeginTypeParameters();
		}

		Builder.AddTypeName(NAME_StructProperty);
		Builder.BeginTypeParameters();
		Builder.AddTypeName(FName(WriteToString<32>(TEXTVIEW("TestStruct"), Index)));
		Builder.EndTypeParameters();

		if (Index % 4 == 0)
		{
			Builder.EndTypeParameters();
		}
		if (Index % 8 == 0)
		{
			Builder.EndTypeParameters();
		}

		Builder.Build();
	});
}

} // UE

#endif // WITH_TESTS
