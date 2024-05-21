// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectUtilsTest.h"

#if WITH_TESTS && WITH_EDITORONLY_DATA

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
#include "UObject/UObjectThreadContext.h"

namespace UE
{

TEST_CASE_NAMED(FInstanceDataObjectUtilsTest, "CoreUObject::Serialization::InstanceDataObjectUtils", "[Core][UObject][EngineFilter]")
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
	UObject* TestObject = NewObject<UObject>(GetTransientPackage(), TestClass, TestObjectName);
	void* StructData = StructProperty->ContainerPtrToValuePtr<void>(TestObject);

	CHECK_FALSE(WasPropertySetBySerialization(TestClass, TestObject, StructProperty));
	MarkPropertySetBySerialization(TestClass, TestObject, StructProperty);
	CHECK(WasPropertySetBySerialization(TestClass, TestObject, StructProperty));
	CHECK_FALSE(WasPropertySetBySerialization(TestClass, TestObject, Int32Property));

	CHECK_FALSE(IsPropertyValueInitialized(TestClass, TestObject, StructProperty));
	SetPropertyValueInitialized(TestClass, TestObject, StructProperty);
	CHECK(IsPropertyValueInitialized(TestClass, TestObject, StructProperty));
	ClearPropertyValueInitialized(TestClass, TestObject, StructProperty);
	CHECK_FALSE(IsPropertyValueInitialized(TestClass, TestObject, StructProperty));

	CHECK_FALSE(WasPropertySetBySerialization(StructProperty->Struct, StructData, AProperty));
	MarkPropertySetBySerialization(StructProperty->Struct, StructData, AProperty);
	CHECK(WasPropertySetBySerialization(StructProperty->Struct, StructData, AProperty));
	CHECK_FALSE(WasPropertySetBySerialization(StructProperty->Struct, StructData, BProperty));
	MarkPropertySetBySerialization(StructProperty->Struct, StructData, BProperty);
	CHECK(WasPropertySetBySerialization(StructProperty->Struct, StructData, BProperty));
	CHECK_FALSE(WasPropertySetBySerialization(StructProperty->Struct, StructData, CProperty));
	CHECK_FALSE(WasPropertySetBySerialization(StructProperty->Struct, StructData, DProperty));

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

TEST_CASE_NAMED(FITrackInitializedPropertiesTest, "CoreUObject::Serialization::TrackInitializedProperties", "[Core][UObject][EngineFilter]")
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

} // UE

#endif // WITH_TESTS && WITH_EDITORONLY_DATA
