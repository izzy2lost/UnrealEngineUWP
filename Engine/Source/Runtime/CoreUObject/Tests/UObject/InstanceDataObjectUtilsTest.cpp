// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectUtilsTest.h"

#if WITH_TESTS && WITH_EDITORONLY_DATA

#include "Logging/LogScopedVerbosityOverride.h"
#include "Misc/ScopeExit.h"
#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/Formatters/JsonArchiveInputFormatter.h"
#include "Serialization/Formatters/JsonArchiveOutputFormatter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "Tests/TestHarnessAdapter.h"
#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/Package.h"
#include "UObject/PropertyBagRepository.h"
#include "UObject/PropertyHelper.h"
#include "UObject/PropertyPathNameTree.h"
#include "UObject/UObjectThreadContext.h"

namespace UE
{

TEST_CASE_NAMED(FInstanceDataObjectUtilsTest, "CoreUObject::Serialization::InstanceDataObjectUtils", "[CoreUObject][EngineFilter]")
{
	UTestInstanceDataObjectClass* BaseObject = NewObject<UTestInstanceDataObjectClass>();
	UClass* TestClass = CreateInstanceDataObjectClass(nullptr, BaseObject->GetClass(), BaseObject->GetOuter());

	FIntProperty* Int32Property = FindFProperty<FIntProperty>(TestClass, TEXT("Int32"));
	FStructProperty* StructProperty = FindFProperty<FStructProperty>(TestClass, TEXT("Struct"));
	REQUIRE(Int32Property);
	REQUIRE(StructProperty);
	REQUIRE(StructProperty->Struct);

	FIntProperty* AProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("A"));
	FIntProperty* BProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("B"));
	FIntProperty* CProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("C"));
	FIntProperty* DProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("D"));
	REQUIRE(AProperty);
	REQUIRE(BProperty);
	REQUIRE(CProperty);
	REQUIRE(DProperty);

	FName TestObjectName = MakeUniqueObjectName(nullptr, TestClass, FName(WriteToString<128>(TestClass->GetFName(), TEXT("_Instance"))));
	UObject* Owner = NewObject<UObject>(GetTransientPackage(), TestClass, TestObjectName);
	void* StructData = StructProperty->ContainerPtrToValuePtr<void>(Owner);

	CHECK_FALSE(WasPropertyValueSerialized(TestClass, Owner, StructProperty));
	MarkPropertyValueSerialized(TestClass, Owner, StructProperty);
	CHECK(WasPropertyValueSerialized(TestClass, Owner, StructProperty));
	CHECK_FALSE(WasPropertyValueSerialized(TestClass, Owner, Int32Property));

	CHECK_FALSE(IsPropertyValueInitialized(TestClass, Owner, StructProperty));
	SetPropertyValueInitialized(TestClass, Owner, StructProperty);
	CHECK(IsPropertyValueInitialized(TestClass, Owner, StructProperty));
	ClearPropertyValueInitialized(TestClass, Owner, StructProperty);
	CHECK_FALSE(IsPropertyValueInitialized(TestClass, Owner, StructProperty));

	CHECK_FALSE(WasPropertyValueSerialized(StructProperty->Struct, StructData, AProperty));
	MarkPropertyValueSerialized(StructProperty->Struct, StructData, AProperty);
	CHECK(WasPropertyValueSerialized(StructProperty->Struct, StructData, AProperty));
	CHECK_FALSE(WasPropertyValueSerialized(StructProperty->Struct, StructData, BProperty));
	MarkPropertyValueSerialized(StructProperty->Struct, StructData, BProperty);
	CHECK(WasPropertyValueSerialized(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(WasPropertyValueSerialized(StructProperty->Struct, StructData, CProperty));
	CHECK_FALSE(WasPropertyValueSerialized(StructProperty->Struct, StructData, DProperty));

	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	SetPropertyValueInitialized(StructProperty->Struct, StructData, AProperty);
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));
	SetPropertyValueInitialized(StructProperty->Struct, StructData, BProperty);
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, CProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, DProperty));
	ClearPropertyValueInitialized(StructProperty->Struct, StructData, AProperty);
	ClearPropertyValueInitialized(StructProperty->Struct, StructData, BProperty);
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));

	SetPropertyValueInitialized(StructProperty->Struct, StructData, AProperty);
	SetPropertyValueInitialized(StructProperty->Struct, StructData, DProperty);
	ResetPropertyValueInitialized(StructProperty->Struct, StructData);
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, CProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, DProperty));
}

TEST_CASE_NAMED(FTrackInitializedPropertiesTest, "CoreUObject::Serialization::TrackInitializedProperties", "[CoreUObject][EngineFilter]")
{
	UTestInstanceDataObjectClass* BaseObject = NewObject<UTestInstanceDataObjectClass>();
	UClass* TestClass = CreateInstanceDataObjectClass(nullptr, BaseObject->GetClass(), BaseObject->GetOuter());

	FStructProperty* StructProperty = FindFProperty<FStructProperty>(TestClass, TEXT("Struct"));
	REQUIRE(StructProperty);
	REQUIRE(StructProperty->Struct);

	FIntProperty* AProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("A"));
	FIntProperty* BProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("B"));
	FIntProperty* CProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("C"));
	FIntProperty* DProperty = FindFProperty<FIntProperty>(StructProperty->Struct, TEXT("D"));
	REQUIRE(AProperty);
	REQUIRE(BProperty);
	REQUIRE(CProperty);
	REQUIRE(DProperty);

	void* DefaultStructData = StructProperty->AllocateAndInitializeValue();
	void* StructData = StructProperty->AllocateAndInitializeValue();
	ON_SCOPE_EXIT
	{
		StructProperty->DestroyAndFreeValue(StructData);
		StructProperty->DestroyAndFreeValue(DefaultStructData);
	};

	AProperty->SetPropertyValue_InContainer(DefaultStructData, -1);
	BProperty->SetPropertyValue_InContainer(DefaultStructData, -1);
	CProperty->SetPropertyValue_InContainer(DefaultStructData, -1);
	DProperty->SetPropertyValue_InContainer(DefaultStructData, -1);

	AProperty->SetPropertyValue_InContainer(StructData, 1);
	BProperty->SetPropertyValue_InContainer(StructData, 2);
	CProperty->SetPropertyValue_InContainer(StructData, 3);
	DProperty->SetPropertyValue_InContainer(StructData, -1);

	SetPropertyValueInitialized(StructProperty->Struct, StructData, AProperty);
	SetPropertyValueInitialized(StructProperty->Struct, StructData, BProperty);
	SetPropertyValueInitialized(StructProperty->Struct, StructData, DProperty);

	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<bool> TrackInitializedPropertiesScope(SerializeContext->bTrackInitializedProperties, true);

	TArray<uint8> BinaryData;
	{
		FMemoryWriter Ar(BinaryData, /*bIsPersistent*/ true);
		FBinaryArchiveFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		StructProperty->Struct->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)StructData, StructProperty->Struct, (uint8*)DefaultStructData);
	}

#if WITH_TEXT_ARCHIVE_SUPPORT
	TArray<uint8> JsonData;
	{
		FMemoryWriter Ar(JsonData, /*bIsPersistent*/ true);
		FJsonArchiveOutputFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		StructProperty->Struct->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)StructData, StructProperty->Struct, (uint8*)DefaultStructData);
	}
#endif // WITH_TEXT_ARCHIVE_SUPPORT

	AProperty->SetPropertyValue_InContainer(StructData, 4);
	BProperty->SetPropertyValue_InContainer(StructData, 4);
	CProperty->SetPropertyValue_InContainer(StructData, 4);
	DProperty->SetPropertyValue_InContainer(StructData, 4);

	ResetPropertyValueInitialized(StructProperty->Struct, StructData);

	{
		FMemoryReader Ar(BinaryData, /*bIsPersistent*/ true);
		FBinaryArchiveFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		StructProperty->Struct->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)StructData, StructProperty->Struct, (uint8*)DefaultStructData);
	}

	CHECK(AProperty->GetPropertyValue_InContainer(StructData) == 1);
	CHECK(BProperty->GetPropertyValue_InContainer(StructData) == 2);
	CHECK(CProperty->GetPropertyValue_InContainer(StructData) == 4); // C unchanged because it is not initialized
	CHECK(DProperty->GetPropertyValue_InContainer(StructData) == 4); // D unchanged because it was serialized without its value

	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, CProperty));
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, DProperty));

#if WITH_TEXT_ARCHIVE_SUPPORT
	AProperty->SetPropertyValue_InContainer(StructData, 4);
	BProperty->SetPropertyValue_InContainer(StructData, 4);
	CProperty->SetPropertyValue_InContainer(StructData, 4);
	DProperty->SetPropertyValue_InContainer(StructData, 4);

	ResetPropertyValueInitialized(StructProperty->Struct, StructData);

	{
		FMemoryReader Ar(JsonData, /*bIsPersistent*/ true);
		FJsonArchiveInputFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		StructProperty->Struct->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)StructData, StructProperty->Struct, (uint8*)DefaultStructData);
	}

	CHECK(AProperty->GetPropertyValue_InContainer(StructData) == 1);
	CHECK(BProperty->GetPropertyValue_InContainer(StructData) == 2);
	CHECK(CProperty->GetPropertyValue_InContainer(StructData) == 4); // C unchanged because it is not initialized
	CHECK(DProperty->GetPropertyValue_InContainer(StructData) == 4); // D unchanged because it was serialized without its value

	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, AProperty));
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(IsPropertyValueInitialized(StructProperty->Struct, StructData, CProperty));
	CHECK(IsPropertyValueInitialized(StructProperty->Struct, StructData, DProperty));
#endif // WITH_TEXT_ARCHIVE_SUPPORT
}

TEST_CASE_NAMED(FTrackUnknownPropertiesTest, "CoreUObject::Serialization::TrackUnknownProperties", "[CoreUObject][EngineFilter]")
{
	const auto MakePropertyTypeName = [](FName Name)
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddName(Name);
		return Builder.Build();
	};

	UObject* Owner = NewObject<UTestInstanceDataObjectClass>();

	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<UObject*> SerializedObjectScope(SerializeContext->SerializedObject, Owner);
	TGuardValue<bool> TrackSerializedPropertyPathScope(SerializeContext->bTrackSerializedPropertyPath, true);
	TGuardValue<bool> TrackUnknownPropertiesScope(SerializeContext->bTrackUnknownProperties, true);
	TGuardValue<bool> TrackImpersonatePropertiesScope(SerializeContext->bImpersonateProperties, true);
	FSerializedPropertyPathScope SerializedObjectPath(SerializeContext, {"Struct"});

	FTestInstanceDataObjectStructAlternate AltStructData;
	AltStructData.B = 2.5f;
	AltStructData.C = 3;
	AltStructData.D = 4;
	AltStructData.E = 5;

	TArray<uint8> BinaryData;
	{
		FMemoryWriter Ar(BinaryData, /*bIsPersistent*/ true);
		FBinaryArchiveFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		FTestInstanceDataObjectStructAlternate::StaticStruct()->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)&AltStructData, nullptr, nullptr);
	}

#if WITH_TEXT_ARCHIVE_SUPPORT
	TArray<uint8> JsonData;
	{
		FMemoryWriter Ar(JsonData, /*bIsPersistent*/ true);
		FJsonArchiveOutputFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		FTestInstanceDataObjectStructAlternate::StaticStruct()->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)&AltStructData, nullptr, nullptr);
	}
#endif // WITH_TEXT_ARCHIVE_SUPPORT

	LOG_SCOPE_VERBOSITY_OVERRIDE(LogClass, ELogVerbosity::Error);

	FTestInstanceDataObjectStruct StructData;

	{
		FMemoryReader Ar(BinaryData, /*bIsPersistent*/ true);
		FBinaryArchiveFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		FTestInstanceDataObjectStruct::StaticStruct()->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)&StructData, nullptr, nullptr);
	}

	CHECK(StructData.A == -1);
	CHECK(StructData.B == 2);
	CHECK(StructData.C == 3);
	CHECK(StructData.D == 4);

	FPropertyPathNameTree* Tree = FPropertyBagRepository::Get().FindOrCreateUnknownPropertyTree(Owner);
	{
		FSerializedPropertyPathScope Path(SerializeContext, {"B", MakePropertyTypeName(NAME_FloatProperty)});
		CHECK(Tree->Find(SerializeContext->SerializedPropertyPath));
	}
	{
		FSerializedPropertyPathScope Path(SerializeContext, {"C", MakePropertyTypeName(NAME_Int64Property)});
		CHECK(Tree->Find(SerializeContext->SerializedPropertyPath));
	}
	{
		FSerializedPropertyPathScope Path(SerializeContext, {"E", MakePropertyTypeName(NAME_IntProperty)});
		CHECK(Tree->Find(SerializeContext->SerializedPropertyPath));
	}
	FPropertyBagRepository::Get().DestroyOuterBag(Owner);

#if WITH_TEXT_ARCHIVE_SUPPORT
	StructData = {};

	{
		FMemoryReader Ar(JsonData, /*bIsPersistent*/ true);
		FJsonArchiveInputFormatter Formatter(Ar);
		FStructuredArchive StructuredAr(Formatter);
		FTestInstanceDataObjectStruct::StaticStruct()->SerializeTaggedProperties(StructuredAr.Open(), (uint8*)&StructData, nullptr, nullptr);
	}

	CHECK(StructData.A == -1);
	CHECK(StructData.B == 2);
	CHECK(StructData.C == 3);
	CHECK(StructData.D == 4);

	// Testing of the unknown property tree is skipped because it is not supported by the text format.
#endif // WITH_TEXT_ARCHIVE_SUPPORT
}

TEST_CASE_NAMED(FUnknownEnumNamesTest, "CoreUObject::Serialization::UnknownEnumNames", "[CoreUObject][EngineFilter]")
{
	UObject* Owner = NewObject<UTestInstanceDataObjectClass>();

	FPropertyBagRepository& Repo = FPropertyBagRepository::Get();

	TArray<FName> Names{NAME_None};
	bool bHasFlags = true;

	// Test a non-flags enum...

	FPropertyTypeName FruitTypeName = []
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddPath(StaticEnum<ETestInstanceDataObjectFruit>());
		return Builder.Build();
	}();

	Repo.FindUnknownEnumNames(Owner, FruitTypeName, Names, bHasFlags);
	CHECK(Names.IsEmpty());
	CHECK_FALSE(bHasFlags);

	const FName NAME_Cherry = "Cherry";
	const FName NAME_Pear = "Pear";

	Repo.AddUnknownEnumName(Owner, nullptr, FruitTypeName, NAME_Pear);
	Repo.AddUnknownEnumName(Owner, StaticEnum<ETestInstanceDataObjectFruit>(), {}, NAME_Cherry);
	Repo.AddUnknownEnumName(Owner, StaticEnum<ETestInstanceDataObjectFruit>(), {}, NAME_Pear);
	Repo.AddUnknownEnumName(Owner, nullptr, FruitTypeName, NAME_Cherry);

	Repo.FindUnknownEnumNames(Owner, FruitTypeName, Names, bHasFlags);
	CHECKED_IF(Names.Num() == 2)
	{
		CHECK(Names[0] == NAME_Pear);
		CHECK(Names[1] == NAME_Cherry);
	}
	CHECK_FALSE(bHasFlags);

	// Test a flags enum by name only...

	FPropertyTypeName FlagsTypeName = []
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddPath(StaticEnum<ETestInstanceDataObjectFlags>());
		return Builder.Build();
	}();

	const FName NAME_South = "South";
	const FName NAME_Down = "Down";
	const FName NAME_Up = "Up";

	TStringBuilder<128> FlagsString;
	FlagsString.Join(MakeArrayView({NAME_Up, NAME_Down, NAME_South}), TEXTVIEW(" | "));

	Repo.AddUnknownEnumName(Owner, nullptr, FlagsTypeName, NAME_Down);
	Repo.AddUnknownEnumName(Owner, nullptr, FlagsTypeName, *FlagsString);

	Repo.FindUnknownEnumNames(Owner, FlagsTypeName, Names, bHasFlags);
	CHECKED_IF(Names.Num() == 3)
	{
		CHECK(Names[0] == NAME_Down);
		CHECK(Names[1] == NAME_Up);
		CHECK(Names[2] == NAME_South);
	}
	CHECK(bHasFlags);

	// Test resetting unknown enum names for an owner...

	Repo.ResetUnknownEnumNames(Owner);

	Repo.FindUnknownEnumNames(Owner, FlagsTypeName, Names, bHasFlags);
	CHECK(Names.IsEmpty());
	CHECK_FALSE(bHasFlags);

	// Test a flags enum by enum...

	Repo.AddUnknownEnumName(Owner, StaticEnum<ETestInstanceDataObjectFlags>(), {}, NAME_Up);

	Repo.FindUnknownEnumNames(Owner, FlagsTypeName, Names, bHasFlags);
	CHECK(Names.Num() == 1);
	CHECK(bHasFlags);

	Repo.AddUnknownEnumName(Owner, StaticEnum<ETestInstanceDataObjectFlags>(), FlagsTypeName, *FlagsString);

	Repo.FindUnknownEnumNames(Owner, FlagsTypeName, Names, bHasFlags);
	CHECKED_IF(Names.Num() == 2)
	{
		CHECK(Names[0] == NAME_Up);
		CHECK(Names[1] == NAME_Down);
	}
	CHECK(bHasFlags);
}

} // UE

#endif // WITH_TESTS && WITH_EDITORONLY_DATA
