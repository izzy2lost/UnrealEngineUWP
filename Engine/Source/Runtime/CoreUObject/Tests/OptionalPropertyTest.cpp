// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "OptionalPropertyTestObject.h"

#include "Tests/TestHarnessAdapter.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/UObjectGlobals.h"

TEST_CASE_NAMED(FOptionalPropertyTestSize, "UE::CoreUObject::OptionalProperty::Size", "[Core][UObject][SmokeFilter]")
{
	const FName TestPackageName(TEXT("/Engine/TestPackage"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
    UOptionalPropertyTestObject* Obj = NewObject<UOptionalPropertyTestObject>(TestPackage);

    UClass* Class = Obj->GetClass();
    FProperty* StringProperty = Class->FindPropertyByName("OptionalString");
    REQUIRE(StringProperty != nullptr);
    FProperty* TextProperty = Class->FindPropertyByName("OptionalText");
    REQUIRE(TextProperty != nullptr);
    FProperty* NameProperty = Class->FindPropertyByName("OptionalName");
    REQUIRE(NameProperty != nullptr);
    FProperty* IntProperty = Class->FindPropertyByName("OptionalInt");
    REQUIRE(IntProperty != nullptr);

    CHECK(StringProperty->GetSize() == sizeof(Obj->OptionalString));
    CHECK(TextProperty->GetSize() == sizeof(Obj->OptionalText));
    CHECK(NameProperty->GetSize() == sizeof(Obj->OptionalName));
    CHECK(IntProperty->GetSize() == sizeof(Obj->OptionalInt));
}

TEST_CASE_NAMED(FOptionalPropertyTestClearValue, "UE::CoreUObject::OptionalProperty::ClearValue", "[Core][UObject][SmokeFilter]")
{
	const FName TestPackageName(TEXT("/Engine/TestPackage"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
    UOptionalPropertyTestObject* Obj = NewObject<UOptionalPropertyTestObject>(TestPackage);

    UClass* Class = Obj->GetClass();
    FProperty* StringProperty = Class->FindPropertyByName("OptionalString");
    REQUIRE(StringProperty != nullptr);
    FProperty* TextProperty = Class->FindPropertyByName("OptionalText");
    REQUIRE(TextProperty != nullptr);
    FProperty* NameProperty = Class->FindPropertyByName("OptionalName");
    REQUIRE(NameProperty != nullptr);
    FProperty* IntProperty = Class->FindPropertyByName("OptionalInt");
    REQUIRE(IntProperty != nullptr);

    Obj->OptionalString.Emplace(TEXT("Optional"));
    Obj->OptionalText.Emplace(FText::FromStringView(TEXTVIEW("Optional")));
    Obj->OptionalName.Emplace(TEXT("Optional"));
    Obj->OptionalInt.Emplace(42);

    StringProperty->ClearValue(&Obj->OptionalString);
    CHECK_FALSE(Obj->OptionalString.IsSet());
    TextProperty->ClearValue(&Obj->OptionalText);
    CHECK_FALSE(Obj->OptionalText.IsSet());
    NameProperty->ClearValue(&Obj->OptionalName);
    CHECK_FALSE(Obj->OptionalName.IsSet());
    IntProperty->ClearValue(&Obj->OptionalInt);
    CHECK_FALSE(Obj->OptionalInt.IsSet());
}

TEST_CASE_NAMED(FOptionalPropertyTestCopyValueIn, "UE::CoreUObject::OptionalProperty::CopyValueIn", "[Core][UObject][SmokeFilter]")
{
	const FName TestPackageName(TEXT("/Engine/TestPackage"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
    UOptionalPropertyTestObject* Obj = NewObject<UOptionalPropertyTestObject>(TestPackage);

    UClass* Class = Obj->GetClass();
    FProperty* StringProperty = Class->FindPropertyByName("OptionalString");
    REQUIRE(StringProperty != nullptr);
    FProperty* TextProperty = Class->FindPropertyByName("OptionalText");
    REQUIRE(TextProperty != nullptr);
    FProperty* NameProperty = Class->FindPropertyByName("OptionalName");
    REQUIRE(NameProperty != nullptr);
    FProperty* IntProperty = Class->FindPropertyByName("OptionalInt");
    REQUIRE(IntProperty != nullptr);

    TOptional<FString> OptString(FString(TEXT("Optional")));
    TOptional<FText> OptText(FText::FromStringView(TEXTVIEW("Optional")));
    TOptional<FName> OptName("Optional");
    TOptional<int32> OptInt(58);

    StringProperty->CopySingleValue(&Obj->OptionalString, &OptString);
    TextProperty->CopySingleValue(&Obj->OptionalText, &OptText);
    NameProperty->CopySingleValue(&Obj->OptionalName, &OptName);
    IntProperty->CopySingleValue(&Obj->OptionalInt, &OptInt);

    CHECK(Obj->OptionalString.IsSet());
    CHECK(Obj->OptionalString.Get(FString()) == OptString.GetValue());
    CHECK(Obj->OptionalText.IsSet());
    CHECK(Obj->OptionalText.Get(FText::GetEmpty()).EqualTo(OptText.GetValue()));
    CHECK(Obj->OptionalName.IsSet());
    CHECK(Obj->OptionalName.Get(FName()) == OptName.GetValue());
    CHECK(Obj->OptionalInt.IsSet());
    CHECK(Obj->OptionalInt.Get(0) == OptInt.GetValue());
}

TEST_CASE_NAMED(FOptionalPropertyTestLayout, "UE::CoreUObject::OptionalProperty::OptionalPropertyLayout", "[Core][UObject][SmokeFilter]")
{
	const FName TestPackageName(TEXT("/Engine/TestPackage"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
    UOptionalPropertyTestObject* Obj = NewObject<UOptionalPropertyTestObject>(TestPackage);

    UClass* Class = Obj->GetClass();
    FOptionalProperty* StringProperty = CastField<FOptionalProperty>(Class->FindPropertyByName("OptionalString"));
    REQUIRE(StringProperty != nullptr);
    FOptionalProperty* TextProperty = CastField<FOptionalProperty>(Class->FindPropertyByName("OptionalText"));
    REQUIRE(TextProperty != nullptr);
    FOptionalProperty* NameProperty = CastField<FOptionalProperty>(Class->FindPropertyByName("OptionalName"));
    REQUIRE(NameProperty != nullptr);
    FOptionalProperty* IntProperty = CastField<FOptionalProperty>(Class->FindPropertyByName("OptionalInt"));
    REQUIRE(IntProperty != nullptr);

    FOptionalPropertyLayout StringPropertyLayout(StringProperty->GetValueProperty());
    FOptionalPropertyLayout TextPropertyLayout(TextProperty->GetValueProperty());
    FOptionalPropertyLayout NamePropertyLayout(NameProperty->GetValueProperty());
    FOptionalPropertyLayout IntPropertyLayout(IntProperty->GetValueProperty());

    CHECK_FALSE(StringPropertyLayout.IsSet(&Obj->OptionalString));
    CHECK_FALSE(TextPropertyLayout.IsSet(&Obj->OptionalText));
    CHECK_FALSE(NamePropertyLayout.IsSet(&Obj->OptionalName));
    CHECK_FALSE(IntPropertyLayout.IsSet(&Obj->OptionalInt));

    FString* InnerString = (FString*)StringPropertyLayout.MarkSetAndGetInitializedValuePointerToReplace(&Obj->OptionalString);
    CHECK(Obj->OptionalString.IsSet());
    *InnerString = TEXT("Optional");
    CHECK(Obj->OptionalString.GetValue() == TEXT("Optional"));
    
    FText* InnerText = (FText*)TextPropertyLayout.MarkSetAndGetInitializedValuePointerToReplace(&Obj->OptionalText);
    CHECK(Obj->OptionalText.IsSet());
    *InnerText = FText::FromStringView(TEXTVIEW("Optional"));
    CHECK(Obj->OptionalText.GetValue().ToString() == TEXT("Optional"));

    FName* InnerName = (FName*)NamePropertyLayout.MarkSetAndGetInitializedValuePointerToReplace(&Obj->OptionalName);
    CHECK(Obj->OptionalName.IsSet());
    *InnerName = FName("Optional");
    CHECK(Obj->OptionalName.GetValue() == FName("Optional"));

    int32* InnerInt = (int32*)IntPropertyLayout.MarkSetAndGetInitializedValuePointerToReplace(&Obj->OptionalInt);
    CHECK(Obj->OptionalInt.IsSet());
    *InnerInt = 79;
    CHECK(Obj->OptionalInt.GetValue() == 79);

    StringPropertyLayout.MarkUnset(&Obj->OptionalString);
    TextPropertyLayout.MarkUnset(&Obj->OptionalText);
    NamePropertyLayout.MarkUnset(&Obj->OptionalName);
    IntPropertyLayout.MarkUnset(&Obj->OptionalInt);

    CHECK_FALSE(StringPropertyLayout.IsSet(&Obj->OptionalString));
    CHECK_FALSE(TextPropertyLayout.IsSet(&Obj->OptionalText));
    CHECK_FALSE(NamePropertyLayout.IsSet(&Obj->OptionalName));
    CHECK_FALSE(IntPropertyLayout.IsSet(&Obj->OptionalInt));
}

#endif