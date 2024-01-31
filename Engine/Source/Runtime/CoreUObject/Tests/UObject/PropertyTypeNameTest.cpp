// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTypeName.h"

#if WITH_TESTS

#include "Async/ParallelFor.h"
#include "Misc/StringBuilder.h"
#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/Formatters/JsonArchiveInputFormatter.h"
#include "Serialization/Formatters/JsonArchiveOutputFormatter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
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
			Builder.AddTypeName(TEXT("Key"));
			Builder.AddTypeName(NAME_ByteProperty);
			Builder.EndTypeParameters();
		}
		Builder.AddTypeName(NAME_EnumProperty);
		{
			Builder.BeginTypeParameters();
			Builder.AddTypeName(TEXT("Value"));
			Builder.AddTypeName(NAME_ByteProperty);
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
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameterName(0) == TEXT("Key"));
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameterName(1) == NAME_ByteProperty);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(0).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(1).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(0).GetTypeParameter(2).IsEmpty());
		CHECK(TypeName.GetTypeParameter(1).GetTypeName() == NAME_EnumProperty);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameterName(0) == TEXT("Value"));
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameterName(1) == NAME_ByteProperty);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(0).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(1).GetTypeParameterCount() == 0);
		CHECK(TypeName.GetTypeParameter(1).GetTypeParameter(2).IsEmpty());
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

		Builder.Reset();
		Builder.AddTypeName(NAME_MapProperty);
		Builder.BeginTypeParameters();
		Builder.AddTypeName(NAME_IntProperty);
		Builder.AddTypeName(MapIntToInt);
		Builder.EndTypeParameters();
		const FPropertyTypeName MapIntToIntToIntAlternative = Builder.Build();

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

		CHECK(MapIntToIntToInt == MapIntToIntToIntAlternative);
	}

	SECTION("Archive")
	{
		FPropertyTypeName Empty;
		FPropertyTypeName Int = PropertyTypeNameTest::CreateInt();
		FPropertyTypeName EnumMap = PropertyTypeNameTest::CreateEnumMap();
		FPropertyTypeName VectorArray = PropertyTypeNameTest::CreateVectorArray();

		TArray<uint8> PersistentData;
		{
			FMemoryWriter Ar(PersistentData, /*bIsPersistent*/ true);
			Ar << Empty << Int << EnumMap << VectorArray;
		}

		TArray<uint8> MemoryData;
		{
			FMemoryWriter Ar(MemoryData, /*bIsPersistent*/ false);
			Ar << Empty << Int << EnumMap << VectorArray;
		}

		TArray<uint8> BinaryData;
		{
			FMemoryWriter Ar(BinaryData, /*bIsPersistent*/ true);
			FBinaryArchiveFormatter Formatter(Ar);
			FStructuredArchive StructuredAr(Formatter);
			FStructuredArchiveRecord Record = StructuredAr.Open().EnterRecord();
			Record.EnterField(TEXT("Empty")) << Empty;
			Record.EnterField(TEXT("Int")) << Int;
			Record.EnterField(TEXT("EnumMap")) << EnumMap;
			Record.EnterField(TEXT("VectorArray")) << VectorArray;
		}

	#if WITH_TEXT_ARCHIVE_SUPPORT
		TArray<uint8> JsonData;
		{
			FMemoryWriter Ar(JsonData, /*bIsPersistent*/ true);
			FJsonArchiveOutputFormatter Formatter(Ar);
			FStructuredArchive StructuredAr(Formatter);
			FStructuredArchiveRecord Record = StructuredAr.Open().EnterRecord();
			Record.EnterField(TEXT("Empty")) << Empty;
			Record.EnterField(TEXT("Int")) << Int;
			Record.EnterField(TEXT("EnumMap")) << EnumMap;
			Record.EnterField(TEXT("VectorArray")) << VectorArray;
		}
	#endif

		FPropertyTypeName EmptyCopy = Int; // needs to be non-empty
		FPropertyTypeName IntCopy;
		FPropertyTypeName EnumMapCopy;
		FPropertyTypeName VectorArrayCopy;

		{
			FMemoryReader Ar(PersistentData, /*bIsPersistent*/ true);
			Ar << EmptyCopy << IntCopy << EnumMapCopy << VectorArrayCopy;
		}
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);

		EmptyCopy = Int; // needs to be non-empty
		IntCopy.Reset();
		EnumMapCopy.Reset();
		VectorArrayCopy.Reset();

		{
			FMemoryReader Ar(MemoryData, /*bIsPersistent*/ false);
			Ar << EmptyCopy << IntCopy << EnumMapCopy << VectorArrayCopy;
		}
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);

		EmptyCopy = Int; // needs to be non-empty
		IntCopy.Reset();
		EnumMapCopy.Reset();
		VectorArrayCopy.Reset();

		{
			FMemoryReader Ar(BinaryData, /*bIsPersistent*/ true);
			FBinaryArchiveFormatter Formatter(Ar);
			FStructuredArchive StructuredAr(Formatter);
			FStructuredArchiveRecord Record = StructuredAr.Open().EnterRecord();
			Record.EnterField(TEXT("Empty")) << EmptyCopy;
			Record.EnterField(TEXT("Int")) << IntCopy;
			Record.EnterField(TEXT("EnumMap")) << EnumMapCopy;
			Record.EnterField(TEXT("VectorArray")) << VectorArrayCopy;
		}
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);

		EmptyCopy = Int; // needs to be non-empty
		IntCopy.Reset();
		EnumMapCopy.Reset();
		VectorArrayCopy.Reset();

	#if WITH_TEXT_ARCHIVE_SUPPORT
		{
			FMemoryReader Ar(JsonData, /*bIsPersistent*/ true);
			FJsonArchiveInputFormatter Formatter(Ar);
			FStructuredArchive StructuredAr(Formatter);
			FStructuredArchiveRecord Record = StructuredAr.Open().EnterRecord();
			Record.EnterField(TEXT("Empty")) << EmptyCopy;
			Record.EnterField(TEXT("Int")) << IntCopy;
			Record.EnterField(TEXT("EnumMap")) << EnumMapCopy;
			Record.EnterField(TEXT("VectorArray")) << VectorArrayCopy;
		}
		CHECK(Empty == EmptyCopy);
		CHECK(Int == IntCopy);
		CHECK(EnumMap == EnumMapCopy);
		CHECK(VectorArray == VectorArrayCopy);
	#endif
	}

	const auto Parse = [](FStringView String) -> FPropertyTypeName
	{
		FPropertyTypeNameBuilder Builder;
		CHECK(Builder.TryParse(String));
		return Builder.Build();
	};
	const auto TryParse = [](FStringView String) -> bool
	{
		FPropertyTypeNameBuilder Builder;
		return Builder.TryParse(String);
	};

	SECTION("String")
	{
		FPropertyTypeName Empty;
		FPropertyTypeName Int = PropertyTypeNameTest::CreateInt();
		FPropertyTypeName EnumMap = PropertyTypeNameTest::CreateEnumMap();
		FPropertyTypeName VectorArray = PropertyTypeNameTest::CreateVectorArray();

		TStringBuilder<128> EmptyString(InPlace, Empty);
		TStringBuilder<128> IntString(InPlace, Int);
		TStringBuilder<128> EnumMapString(InPlace, EnumMap);
		TStringBuilder<128> VectorArrayString(InPlace, VectorArray);

		CHECK(TEXTVIEW("None").Equals(EmptyString));
		CHECK(TEXTVIEW("IntProperty").Equals(IntString));
		CHECK(TEXTVIEW("MapProperty<EnumProperty<Key,ByteProperty>,EnumProperty<Value,ByteProperty>>").Equals(EnumMapString));
		CHECK(TEXTVIEW("ArrayProperty<StructProperty<Vector>>").Equals(VectorArrayString));

		CHECK(Empty == Parse(EmptyString));
		CHECK(Int == Parse(IntString));
		CHECK(EnumMap == Parse(EnumMapString));
		CHECK(VectorArray == Parse(VectorArrayString));
	}

	SECTION("Parse")
	{
		// There are positive parsing tests in the String section above.
		CHECK(Parse(TEXTVIEW(" \t IntProperty \t ")) == PropertyTypeNameTest::CreateInt());
		CHECK(Parse(TEXTVIEW(" \t ArrayProperty < StructProperty\t<\tVector\t>\t > \t ")) == PropertyTypeNameTest::CreateVectorArray());

		CHECK_FALSE(TryParse(TEXTVIEW("")));
		CHECK_FALSE(TryParse(TEXTVIEW(",")));
		CHECK_FALSE(TryParse(TEXTVIEW("<>")));
		CHECK_FALSE(TryParse(TEXTVIEW(",IntProperty")));
		CHECK_FALSE(TryParse(TEXTVIEW("IntProperty,")));
		CHECK_FALSE(TryParse(TEXTVIEW("IntProperty,IntProperty")));
		CHECK_FALSE(TryParse(TEXTVIEW("IntProperty>")));
		CHECK_FALSE(TryParse(TEXTVIEW("<IntProperty>")));
		CHECK_FALSE(TryParse(TEXTVIEW("<IntProperty")));
		CHECK_FALSE(TryParse(TEXTVIEW("ArrayProperty<IntProperty")));
		CHECK_FALSE(TryParse(TEXTVIEW("ArrayProperty<IntProperty>>")));
		CHECK_FALSE(TryParse(TEXTVIEW("ArrayProperty<<IntProperty>")));
		CHECK_FALSE(TryParse(TEXTVIEW("ArrayProperty<IntProperty>IntProperty")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<>")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<,>")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<IntProperty,>")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<,IntProperty>")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<IntProperty,,IntProperty>")));
		CHECK_FALSE(TryParse(TEXTVIEW("MapProperty<IntProperty><IntProperty>")));
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
