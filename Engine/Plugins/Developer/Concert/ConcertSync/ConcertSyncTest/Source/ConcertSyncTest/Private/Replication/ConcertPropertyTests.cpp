// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/PropertyChainUtils.h"
#include "Misc/AutomationTest.h"

#include "TestReflectionObject.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"

/**
 * Tests that the FConcertPropertyChain constructor works as the documentation dictates:
 *  - Root properties
 *  - Array, set, map of
 *		- "normal" structs, like FVector > lists sub-properties
 *		- primitives and native structs > Path contains FConcertPropertyChain::InternalContainerPropertyValueName ("Value") at the end
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertPropertyChainTests, "Concert.Replication.PropertyChainTests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConcertPropertyChainTests::RunTest(const FString& Parameters)
{
	// The test is intentionally set up to make it it easy to set breakpoints albeit at the expense of increasing the amount of code

	// 1. Input
	UStruct* TestReplicationStruct = FTestReplicationStruct::StaticStruct();
	FProperty* ValueProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, Value));
	FProperty* VectorProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, Vector));
	FProperty* VectorXProp = CastField<FStructProperty>(VectorProp)->Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FVector, X));
	
	FProperty* FloatArrayProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, FloatArray));
	FProperty* FloatArrayInnerProp = CastField<FArrayProperty>(FloatArrayProp)->Inner;
	FProperty* StringArrayProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, StringArray));
	FProperty* NativeStructArrayProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, NativeStructArray));
	FProperty* NativeStructArrayInnerProp = CastField<FArrayProperty>(NativeStructArrayProp)->Inner;
	
	FProperty* FloatSetProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, FloatSet));
	FProperty* FloatSetInnerProp = CastField<FSetProperty>(FloatSetProp)->ElementProp;
	FProperty* StringSetProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, StringSet));
	FProperty* NativeStructSetProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, NativeStructSet));
	FProperty* NativeStructSetInnerProp = CastField<FSetProperty>(NativeStructSetProp)->ElementProp;
	
	FProperty* StringToFloatProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, StringToFloat));
	FProperty* StringToFloatInnerProp = CastField<FMapProperty>(StringToFloatProp)->ValueProp;
	FProperty* StringToVectorProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, StringToVector));
	FProperty* StringToNativeStructProp = TestReplicationStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FTestReplicationStruct, StringToNativeStruct));
	FProperty* StringToNativeStructInnerProp = CastField<FMapProperty>(StringToNativeStructProp)->ValueProp;

	FArchiveSerializedPropertyChain VectorXChain;
	FArchiveSerializedPropertyChain FloatArrayChain;
	FArchiveSerializedPropertyChain FloatSetArrayChain;
	FArchiveSerializedPropertyChain StringToFloatChain;
	FArchiveSerializedPropertyChain NativeStructArrayChain;
	FArchiveSerializedPropertyChain NativeStructSetChain;
	FArchiveSerializedPropertyChain StringToNativeStructChain;
	VectorXChain.PushProperty(VectorProp, false);
	FloatArrayChain.PushProperty(FloatArrayProp, false);
	FloatSetArrayChain.PushProperty(FloatSetProp, false);
	StringToFloatChain.PushProperty(StringToFloatProp, false);
	NativeStructArrayChain.PushProperty(NativeStructArrayProp, false);
	NativeStructSetChain.PushProperty(NativeStructSetProp, false);
	StringToNativeStructChain.PushProperty(StringToNativeStructProp, false);
	

	
	// 2. Run
	const FConcertPropertyChain Value(nullptr, *ValueProp);
	const FConcertPropertyChain Vector(nullptr, *VectorProp);
	const FConcertPropertyChain VectorX(&VectorXChain, *VectorXProp);
	
	const FConcertPropertyChain FloatArray(nullptr, *FloatArrayProp);
	const FConcertPropertyChain FloatArrayInner(&FloatArrayChain, *FloatArrayInnerProp);
	const FConcertPropertyChain StringArray(nullptr, *StringArrayProp);
	const FConcertPropertyChain NativeStructArray(nullptr, *NativeStructArrayProp);
	const FConcertPropertyChain NativeStructArrayInner(&NativeStructArrayChain, *NativeStructArrayInnerProp);
	
	const FConcertPropertyChain FloatSet(nullptr, *FloatSetProp);
	const FConcertPropertyChain FloatSetInner(&FloatSetArrayChain, *FloatSetInnerProp);
	const FConcertPropertyChain StringSet(nullptr, *StringSetProp);
	const FConcertPropertyChain NativeStructSet(nullptr, *NativeStructSetProp);
	const FConcertPropertyChain NativeStructSetInner(&NativeStructSetChain, *NativeStructSetInnerProp);
	
	const FConcertPropertyChain StringToFloat(nullptr, *StringToFloatProp);
	const FConcertPropertyChain StringToFloatInner(&StringToFloatChain, *StringToFloatInnerProp);
	const FConcertPropertyChain StringToVector(nullptr, *StringToVectorProp);
	const FConcertPropertyChain StringToNativeStruct(nullptr, *StringToNativeStructProp);
	const FConcertPropertyChain StringToNativeStructInner(&StringToNativeStructChain, *StringToNativeStructInnerProp);


	
	// 3. Test
	TestTrue(TEXT("Value"), Value == TArray<FName>{ TEXT("Value") });
	TestTrue(TEXT("Vector"), Vector == TArray<FName>{ TEXT("Vector") });
	TestTrue(TEXT("Vector.X"), VectorX == TArray<FName>{ FName(TEXT("Vector")), FName(TEXT("X")) });
	
	TestTrue(TEXT("FloatArray"), FloatArray == TArray<FName>{ TEXT("FloatArray") });
	TestTrue(TEXT("FloatArrayInner"), FloatArrayInner == TArray<FName>{ FName(TEXT("FloatArray")), FConcertPropertyChain::InternalContainerPropertyValueName });
	TestTrue(TEXT("StringArray"), StringArray == TArray<FName>{ TEXT("StringArray") });
	TestTrue(TEXT("NativeStructArray"), NativeStructArray == TArray<FName>{ TEXT("NativeStructArray") });
	TestTrue(TEXT("NativeStructArrayInner"), NativeStructArrayInner == TArray<FName>{ FName(TEXT("NativeStructArray")), FConcertPropertyChain::InternalContainerPropertyValueName });
	
	TestTrue(TEXT("FloatSet"), FloatSet == TArray<FName>{ TEXT("FloatSet") });
	TestTrue(TEXT("FloatSetInner"), FloatSetInner == TArray<FName>{ FName(TEXT("FloatSet")), FConcertPropertyChain::InternalContainerPropertyValueName });
	TestTrue(TEXT("StringSet"), StringSet == TArray<FName>{ TEXT("StringSet") });
	TestTrue(TEXT("NativeStructSet"), NativeStructSet == TArray<FName>{ TEXT("NativeStructSet") });
	TestTrue(TEXT("NativeStructSetInner"), NativeStructSetInner == TArray<FName>{ FName(TEXT("NativeStructSet")), FConcertPropertyChain::InternalContainerPropertyValueName });
	
	TestTrue(TEXT("StringToFloat"), StringToFloat == TArray<FName>{ TEXT("StringToFloat") });
	TestTrue(TEXT("StringToFloatInner"), StringToFloatInner == TArray<FName>{ FName(TEXT("StringToFloat")), FConcertPropertyChain::InternalContainerPropertyValueName });
	TestTrue(TEXT("StringToVector"), StringToVector == TArray<FName>{ TEXT("StringToVector") });
	TestTrue(TEXT("StringToNativeStruct"), StringToNativeStruct == TArray<FName>{ TEXT("StringToNativeStruct") });
	TestTrue(TEXT("StringToNativeStructInner"), StringToNativeStructInner == TArray<FName>{ FName(TEXT("StringToNativeStruct")), FConcertPropertyChain::InternalContainerPropertyValueName });
	
	return true;
}

/**
 * Tests that all property cases on the specially constructed example class UTestReflectionObject.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertForEachReplictableConcertPropertyTests, "Concert.Replication.ForEachReplicatableConcertProperty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConcertForEachReplictableConcertPropertyTests::RunTest(const FString& Parameters)
{
	UClass* Class = UTestReflectionObject::StaticClass();
	TArray<FConcertPropertyChain> Properties;
	UE::ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*Class, [&Properties](FConcertPropertyChain&& Chain)
	{
		Properties.Add(MoveTemp(Chain));
		return EBreakBehavior::Continue;
	});

	using FExpectedPropertyPath = TArray<FName>;
	auto ToString = [](const FExpectedPropertyPath& Path)
	{
		FString Result;
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			if (i == 0)
			{
				Result += Path[i].ToString();
			}
			else
			{
				Result += TEXT(".") + Path[i].ToString();
			}
		}
		return Result;
	};
	const TArray<FExpectedPropertyPath> ExpectedProperties {
		{ TEXT("Float") },
		{ TEXT("Vector") },
		{ TEXT("Vector"), TEXT("X") },
		{ TEXT("Vector"), TEXT("Y") },
		{ TEXT("Vector"), TEXT("Z") },
		{ TEXT("TestStruct") },
		{ TEXT("TestStruct"), TEXT("Nested") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("Value") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("Vector") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("Vector"), TEXT("X")  },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("Vector"), TEXT("Y")  },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("Vector"), TEXT("Z")  },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("NativeStruct") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("FloatArray") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("FloatArray"), FConcertPropertyChain::InternalContainerPropertyValueName },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringArray") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("NativeStructArray") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("NativeStructArray"), FConcertPropertyChain::InternalContainerPropertyValueName },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("FloatSet") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("FloatSet"), FConcertPropertyChain::InternalContainerPropertyValueName },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringSet") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("NativeStructSet") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("NativeStructSet"), FConcertPropertyChain::InternalContainerPropertyValueName },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToFloat") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToFloat") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToVector") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToVector"), TEXT("X") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToVector"), TEXT("Y") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToVector"), TEXT("Z") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToNativeStruct") },
		{ TEXT("TestStruct"), TEXT("Nested"), TEXT("StringToNativeStruct"), FConcertPropertyChain::InternalContainerPropertyValueName },
	};

	for (const FExpectedPropertyPath& Expected : ExpectedProperties)
	{
		// This structure is easier for setting breakpoints than using TestTrue
		if (!Properties.Contains(Expected))
		{
			AddError(ToString(Expected));
		}
	}
	TestEqual(TEXT("Does not contain disallowed properties"), Properties.Num(), ExpectedProperties.Num());
	
	return true;
}