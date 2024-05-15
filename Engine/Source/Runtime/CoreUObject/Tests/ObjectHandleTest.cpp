// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "ObjectPtrTestClass.h"
#include "UObject/ObjectHandle.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/MetaData.h"
#include "HAL/PlatformProperties.h"
#include "ObjectRefTrackingTestBase.h"
#include "IO/IoDispatcher.h"
#include "TestHarness.h"
#include "UObject/ObjectRef.h"
#include "UObject/ObjectPathId.h"
#include "UObject/PropertyBagRepository.h"

static_assert(sizeof(FObjectHandle) == sizeof(void*), "FObjectHandle type must always compile to something equivalent to a pointer size.");

class FObjectHandleTestBase : public FObjectRefTrackingTestBase
{
public:
	
protected:
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	void TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef PackedRef)
	{
		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectHandle TargetHandle = { PackedRef.EncodedRef };
		UObject* ResolvedObject = FObjectPtr(TargetHandle).Get();
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1);

		CHECK(ResolvedObject == nullptr);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should be incremented by one after a failed resolve attempt"), 1);
	}
#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
	void TestResolvableNonNull(const ANSICHAR* PackageName, const ANSICHAR* ObjectName, bool bExpectSubRefReads)
	{

		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectRef TargetRef(FName(PackageName), NAME_None, NAME_None, UE::CoreUObject::Private::FObjectPathId(ObjectName));
		UObject* ResolvedObject = TargetRef.Resolve();
		FObjectPtr Ptr(ResolvedObject);
		Ptr.Get();
		TEST_TRUE(TEXT("expected not null"), ResolvedObject != nullptr);
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1, bExpectSubRefReads /*bAllowAdditionalReads*/);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a successful resolve attempt"), 0);
	}

	void TestResolveFailure(const ANSICHAR* PackageName, const ANSICHAR* ObjectName)
	{
		FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
		FObjectRef TargetRef(FName(PackageName), NAME_None, NAME_None, UE::CoreUObject::Private::FObjectPathId(ObjectName));
		const UObject* ResolvedObject = TargetRef.Resolve();
		ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should be incremented by one after a resolve attempt"), 1);
		ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt"), 1);
		CHECK(ResolvedObject == nullptr);
		ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should be incremented by one after a failed resolve attempt"), 1);
	}
#endif
};

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Null Behavior", "[CoreUObject][ObjectHandle]")
{
	FObjectHandle TargetHandle = UE::CoreUObject::Private::MakeObjectHandle(nullptr);

	TEST_TRUE(TEXT("Handle to target is null"), IsObjectHandleNull(TargetHandle));
	TEST_TRUE(TEXT("Handle to target is resolved"), IsObjectHandleResolved(TargetHandle));

	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	UObject* ResolvedObject = UE::CoreUObject::Private::ResolveObjectHandle(TargetHandle);

	TEST_EQUAL(TEXT("Resolved object is equal to original object"), (UObject*)nullptr, ResolvedObject);

	ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a resolve attempt on a null handle"), 0);
	ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should not change after a resolve attempt on a null handle"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt on a null handle"), 1);
}

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Pointer Behavior", "[CoreUObject][ObjectHandle]")
{
	FObjectHandle TargetHandle = UE::CoreUObject::Private::MakeObjectHandle((UObject*)0x0042);

	TEST_FALSE(TEXT("Handle to target is null"), IsObjectHandleNull(TargetHandle));
	TEST_TRUE(TEXT("Handle to target is resolved"), IsObjectHandleResolved(TargetHandle));

	FSnapshotObjectRefMetrics ObjectRefMetrics(*this);
	UObject* ResolvedObject = UE::CoreUObject::Private::ResolveObjectHandle(TargetHandle);

	TEST_EQUAL(TEXT("Resolved object is equal to original object"), (UObject*)0x0042, ResolvedObject);

	ObjectRefMetrics.TestNumResolves(TEXT("NumResolves should not change after a resolve attempt on a pointer handle"), 0);
	ObjectRefMetrics.TestNumFailedResolves(TEXT("NumFailedResolves should not change after a resolve attempt on a pointer handle"), 0);
	ObjectRefMetrics.TestNumReads(TEXT("NumReads should be incremented by one after a resolve attempt on a pointer handle"),1);
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Engine Content Target", "[CoreUObject][ObjectHandle]")
{
	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	UObject* TestSubObject = NewObject<UObjectPtrTestClass>(TestSoftObject, TEXT("SubObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	TestResolvableNonNull("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject.SubObject", true);
	TestResolvableNonNull("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject", false);
}


// TODO: Disabled until warnings and errors related to loading a non-existent package have been fixed.
DISABLED_TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Non Existent Target", "[CoreUObject][ObjectHandle]")
{
	// Confirm we don't successfully resolve an incorrect reference to engine content
	TestResolveFailure("/Engine/EngineResources/NonExistentPackageName_0", "DefaultTexture");

	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* TestSoftObject = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	TestResolveFailure("/Engine/Test/ObjectPtrDefaultSerialize/Transient", "DefaultSerializeObject_DoesNotExist");
}

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Script Target", "[CoreUObject][ObjectHandle]")
{
	// Confirm we successfully resolve a correct reference to engine content
	TestResolvableNonNull("/Script/CoreUObject", "MetaData", true);
}

#endif

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::TObjectPtr::HandleNullGetClass", "[CoreUObject][ObjectHandle]")
{
	TObjectPtr<UObject> Ptr = nullptr;
	TEST_TRUE(TEXT("TObjectPtr.GetClass should return null on a null object"), Ptr.GetClass() == nullptr);
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
TEST_CASE("CoreUObject::FObjectHandle::Names")
{
	const FName TestPackageName(TEXT("/Engine/Test/PackageResolve/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* Obj1 = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	FObjectPtr Test;
	FObjectPtr PackagePtr(MakeUnresolvedHandle(TestPackage));
	FObjectPtr Obj1Ptr(MakeUnresolvedHandle(Obj1));

	CHECK(!PackagePtr.IsResolved());
	CHECK(TestPackage->GetPathName() == PackagePtr.GetPathName());
	CHECK(TestPackage->GetFName() == PackagePtr.GetFName());
	CHECK(TestPackage->GetName() == PackagePtr.GetName());
	CHECK(TestPackage->GetFullName() == PackagePtr.GetFullName());
	CHECK(!PackagePtr.IsResolved());

	CHECK(!Obj1Ptr.IsResolved());
	CHECK(Obj1->GetPathName() == Obj1Ptr.GetPathName());
	CHECK(Obj1->GetFName() == Obj1Ptr.GetFName());
	CHECK(Obj1->GetName() == Obj1Ptr.GetName());
	CHECK(Obj1->GetFullName() == Obj1Ptr.GetFullName());
	CHECK(!Obj1Ptr.IsResolved());
}
#endif

#if UE_WITH_OBJECT_HANDLE_TRACKING || UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE("CoreUObject::ObjectRef")
{
	const FName TestPackageName(TEXT("/Engine/Test/ObjectRef/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	UObject* Obj1 = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("DefaultSerializeObject"));
	UObject* Inner1 = NewObject<UObjectPtrTestClass>(Obj1, TEXT("Inner"));
	ON_SCOPE_EXIT{
		TestPackage->RemoveFromRoot();
	};

	{
		FObjectImport ObjectImport(Obj1);
		FObjectRef ObjectRef(Obj1);

		CHECK(ObjectImport.ClassPackage == ObjectRef.ClassPackageName);
		CHECK(ObjectImport.ClassName == ObjectRef.ClassName);
		CHECK(TestPackage->GetFName() == ObjectRef.PackageName);
	}

	{
		FObjectImport ObjectImport(Inner1);
		FObjectRef ObjectRef(Inner1);

		CHECK(ObjectImport.ClassPackage == ObjectRef.ClassPackageName);
		CHECK(ObjectImport.ClassName == ObjectRef.ClassName);
		CHECK(TestPackage->GetFName() == ObjectRef.PackageName);
	}
}

#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::TObjectPtr::Null Behavior", "[CoreUObject][ObjectHandle]")
{
	TObjectPtr<UObject> Ptr = nullptr;
	UObjectPtrTestClass* TestObject = nullptr;

	uint32 ResolveCount = 0;
	auto ResolveDelegate = [&ResolveCount](const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object)
		{
			++ResolveCount;
		};
	auto Handle = UE::CoreUObject::AddObjectHandleReferenceResolvedCallback(ResolveDelegate);
	ON_SCOPE_EXIT
	{
		UE::CoreUObject::RemoveObjectHandleReferenceResolvedCallback(Handle);
	};
	//compare against all flavours of nullptr, should not try and resolve this pointer
	CHECK(Ptr == nullptr); CHECK(ResolveCount == 0u);
	CHECK(nullptr == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != nullptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(nullptr != Ptr); CHECK(ResolveCount == 0u);
	CHECK(!Ptr); CHECK(ResolveCount == 0u);

	//using an if otherwise the macros try to convert to a pointer and not use the bool operator
	if (Ptr)
	{
		CHECK(false);
	}
	else
	{
		CHECK(true);
	}
	CHECK(ResolveCount == 0u);

	CHECK(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject != Ptr); CHECK(ResolveCount == 0u);

	FObjectRef TargetRef(FName("SomePackage"), FName("ClassPackageName"), FName("ClassName"), UE::CoreUObject::Private::FObjectPathId("ObjectName"));
	UE::CoreUObject::Private::FPackedObjectRef PackedObjectRef = UE::CoreUObject::Private::MakePackedObjectRef(TargetRef);
	FObjectPtr ObjectPtr({ PackedObjectRef.EncodedRef });
	REQUIRE(!ObjectPtr.IsResolved()); //make sure not resolved

	//an unresolved pointers compared against nullptr should still not resolve
	Ptr = *reinterpret_cast<TObjectPtr<UObject>*>(&ObjectPtr);
	CHECK_FALSE(Ptr == nullptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(nullptr == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != nullptr); CHECK(ResolveCount == 0u);
	CHECK(nullptr != Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(!Ptr); CHECK(ResolveCount == 0u);

	//using an if otherwise the macros try to convert to a pointer and not use the bool operator
	if (Ptr)
	{
		CHECK(true);
	}
	else
	{
		CHECK(false);
	}
	CHECK(ResolveCount == 0u);

	//test an unresolve pointer against a null raw pointer
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr);	CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);

	//creating a real object for something that can resolve
	const FName TestPackageName(TEXT("/Engine/Test/ObjectPtrDefaultSerialize/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();

	const FName TestObjectName(TEXT("MyObject"));
	TestObject = NewObject<UObjectPtrTestClass>(TestPackage, TestObjectName, RF_Transient);
	TObjectPtr<UObject> TestNotLazyObject = NewObject<UObjectPtrNotLazyTestClass>(TestPackage, TEXT("NotLazy"), RF_Transient);

	//compare resolved ptr against nullptr
	TObjectPtr<UObject> ResolvedPtr = TestObject;
	CHECK(ResolvedPtr.IsResolved());
	CHECK(Ptr != ResolvedPtr);  CHECK(ResolveCount == 0u);
	CHECK(ResolvedPtr != Ptr);  CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr == ResolvedPtr);  CHECK(ResolveCount == 0u);
	CHECK_FALSE(ResolvedPtr == Ptr);  CHECK(ResolveCount == 0u);

	//compare unresolved against nullptr
	FObjectPtr FPtr(MakeUnresolvedHandle(TestObject));
	TObjectPtr<UObject> UnResolvedPtr = *reinterpret_cast<TObjectPtr<UObject>*>(&FPtr);
	CHECK(!UnResolvedPtr.IsResolved());
	CHECK_FALSE(Ptr == UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(UnResolvedPtr == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr != Ptr); CHECK(ResolveCount == 0u);

	//compare unresolved against resolved not equal
	CHECK_FALSE(TestNotLazyObject == UnResolvedPtr); CHECK(ResolveCount == 0u); 
	CHECK_FALSE(UnResolvedPtr == TestNotLazyObject); CHECK(ResolveCount == 0u);
	CHECK(TestNotLazyObject != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr != TestNotLazyObject); CHECK(ResolveCount == 0u);

	//compare resolved against naked pointer
	Ptr = TestObject;
	REQUIRE(Ptr.IsResolved());
	CHECK(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject != Ptr); CHECK(ResolveCount == 0u);

	//compare resolved pointer and unresolved of the same object
	CHECK(Ptr == UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK(UnResolvedPtr == Ptr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(Ptr != UnResolvedPtr); CHECK(ResolveCount == 0u);
	CHECK_FALSE(UnResolvedPtr != Ptr); CHECK(ResolveCount == 0u);

	TestObject = nullptr;
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);

	TestObject = static_cast<UObjectPtrTestClass*>(Ptr.Get());
	Ptr = nullptr;
	CHECK_FALSE(Ptr == TestObject); CHECK(ResolveCount == 0u);
	CHECK_FALSE(TestObject == Ptr); CHECK(ResolveCount == 0u);
	CHECK(Ptr != TestObject); CHECK(ResolveCount == 0u);
	CHECK(TestObject != Ptr); CHECK(ResolveCount == 0u);

}

#endif

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Resolve Malformed Handle", "[CoreUObject][ObjectHandle]")
{
	// make one packed ref guarantee something is in the object handle index
	FObjectRef TargetRef(FName("/Test/DummyPackage"), FName("ClassPackageName"), FName("ClassName"), UE::CoreUObject::Private::FObjectPathId("DummyObjectName"));
	UE::CoreUObject::Private::MakePackedObjectRef(TargetRef);

	uint32 ObjectId = ~0u;
	UPTRINT PackedId = ObjectId << 1 | 1;
	UE::CoreUObject::Private::FPackedObjectRef PackedObjectRef = { PackedId };
	TestResolveFailure(PackedObjectRef); // packed ref has a valid package id but invalid object id

	TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef { 0xFFFF'FFFF'FFFF'FFFFull });
	TestResolveFailure(UE::CoreUObject::Private::FPackedObjectRef { 0xEFEF'EFEF'EFEF'EFEFull });
}
#endif // UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Hash Object Without Index", "[CoreUObject][ObjectHandle]")
{
	UObject DummyObjectWithInvalidIndex(EC_StaticConstructor, RF_NoFlags);
	CHECK(DummyObjectWithInvalidIndex.GetUniqueID() == -1);

	FObjectHandle DummyObjectHandle = UE::CoreUObject::Private::MakeObjectHandle(&DummyObjectWithInvalidIndex);
	CHECK(GetTypeHash(DummyObjectHandle) == GetTypeHash(&DummyObjectWithInvalidIndex));
}

#if UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
TEST_CASE_METHOD(FObjectHandleTestBase, "CoreUObject::FObjectHandle::Type Safety", "[CoreUObject][ObjectHandle]")
{
	const FName TestPackageName(TEXT("/Engine/Test/ObjectHandle/TypeSafety/Transient"));
	UPackage* TestPackage = NewObject<UPackage>(nullptr, TestPackageName, RF_Transient);
	TestPackage->AddToRoot();
	ON_SCOPE_EXIT
	{
		TestPackage->RemoveFromRoot();
	};

	// construct an unsafe class type
	UClass* TestUnsafeClass = UE::FPropertyBagRepository::CreatePropertyBagPlaceholderClass(TestPackage, UClass::StaticClass(), TEXT("TestUnsafeClass"));

	// construct objects for testing
	UObjectPtrTestClass* TestSafeObject = NewObject<UObjectPtrTestClass>(TestPackage, TEXT("TestSafeObject"), RF_Transient);
	UObject* TestUnsafeObject = NewObject<UObject>(TestPackage, TestUnsafeClass, TEXT("TestUnsafeObject"), RF_Transient);

	// construct object handles for testing
	FObjectHandle NullObjectHandle = UE::CoreUObject::Private::MakeObjectHandle(nullptr);
	FObjectHandle TestSafeObjectHandle = UE::CoreUObject::Private::MakeObjectHandle(TestSafeObject);
	FObjectHandle TestUnsafeObjectHandle = UE::CoreUObject::Private::MakeObjectHandle(TestUnsafeObject);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	FObjectHandle TestLateResolveSafeObjectHandle = { UE::CoreUObject::Private::MakePackedObjectRef(TestSafeObject).EncodedRef };
	FObjectHandle TestLateResolveUnsafeObjectHandle = { UE::CoreUObject::Private::MakePackedObjectRef(TestUnsafeObject).EncodedRef };
#endif

	// NULL/type-safe object handles should report as being safe
	CHECK(IsObjectHandleTypeSafe(NullObjectHandle));
	CHECK(IsObjectHandleTypeSafe(TestSafeObjectHandle));
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(IsObjectHandleTypeSafe(TestLateResolveSafeObjectHandle));
#endif

	// unsafe type object handles should report as being unsafe, but only when late resolve is enabled (required for support)
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK_FALSE(IsObjectHandleTypeSafe(TestUnsafeObjectHandle));
	CHECK_FALSE(IsObjectHandleTypeSafe(TestLateResolveUnsafeObjectHandle));
#else
	CHECK(IsObjectHandleTypeSafe(TestUnsafeObjectHandle));
#endif

	// unsafe type object handles should resolve the class to the unsafe type
	CHECK(UE::CoreUObject::Private::ResolveObjectHandleClass(TestUnsafeObjectHandle) == TestUnsafeClass);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(UE::CoreUObject::Private::ResolveObjectHandleClass(TestLateResolveUnsafeObjectHandle) == TestUnsafeClass);
#endif

	// an unsafe type object handle should not equate to other unsafe type object handles (including NULL), except for itself
	CHECK(NullObjectHandle != TestUnsafeObjectHandle);			// note: this behavior intentionally differs from object *pointers* (see below)
	CHECK(TestUnsafeObjectHandle != NullObjectHandle);			// see note directly above
	CHECK(TestSafeObjectHandle != TestUnsafeObjectHandle);
	CHECK(TestUnsafeObjectHandle != TestSafeObjectHandle);
	CHECK(TestUnsafeObjectHandle == TestUnsafeObjectHandle);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(NullObjectHandle != TestLateResolveUnsafeObjectHandle);
	CHECK(TestLateResolveUnsafeObjectHandle != NullObjectHandle);
	CHECK(TestSafeObjectHandle != TestLateResolveUnsafeObjectHandle);
	CHECK(TestLateResolveUnsafeObjectHandle != TestSafeObjectHandle);
	CHECK(TestUnsafeObjectHandle == TestLateResolveUnsafeObjectHandle);
	CHECK(TestLateResolveUnsafeObjectHandle == TestUnsafeObjectHandle);
	CHECK(TestLateResolveSafeObjectHandle != TestLateResolveUnsafeObjectHandle);
	CHECK(TestLateResolveUnsafeObjectHandle != TestLateResolveSafeObjectHandle);
	CHECK(TestLateResolveUnsafeObjectHandle == TestLateResolveUnsafeObjectHandle);
#endif

	// the type safety and class queries above should not have resolved an object handle that's using late resolve
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK_FALSE(IsObjectHandleResolved(TestLateResolveSafeObjectHandle));
	CHECK_FALSE(IsObjectHandleResolved(TestLateResolveUnsafeObjectHandle));
#endif

	// unsafe type object handles should resolve/evaluate to the original type object
	CHECK(UE::CoreUObject::Private::ResolveObjectHandle(TestSafeObjectHandle) == TestSafeObject);
	CHECK(UE::CoreUObject::Private::ResolveObjectHandle(TestUnsafeObjectHandle) == TestUnsafeObject);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(UE::CoreUObject::Private::ResolveObjectHandle(TestLateResolveSafeObjectHandle) == TestSafeObject);
	CHECK(UE::CoreUObject::Private::ResolveObjectHandle(TestLateResolveUnsafeObjectHandle) == TestUnsafeObject);
#endif

	// all handles should now report as being resolved
	CHECK(IsObjectHandleResolved(NullObjectHandle));
	CHECK(IsObjectHandleResolved(TestSafeObjectHandle));
	CHECK(IsObjectHandleResolved(TestUnsafeObjectHandle));
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(IsObjectHandleResolved(TestLateResolveSafeObjectHandle));
	CHECK(IsObjectHandleResolved(TestLateResolveUnsafeObjectHandle));
#endif

	// construct object pointers for testing intentionally different behaviors of UObject-type vs. non-UObject-type bindings
	TObjectPtr<UObject> NullObjectPtr(nullptr);
	TObjectPtr<UObject> TestSafeObjectPtr(TestUnsafeObject);								// type safe pointer to placeholder (bound to UObject type)
	TObjectPtr<const UObject> TestSafeConstObjectPtr(TestUnsafeObject);						// type safe const pointer to placeholder (bound to UObject type)
	FObjectPtr TestUnsafeObjectPtr_Untyped(TestUnsafeObject);
	TObjectPtr<UObjectPtrTestClass> TestUnsafeObjectPtr(TestUnsafeObjectPtr_Untyped);		// unsafe pointer to placeholder object (bound to non-UObject type)
	TObjectPtr<const UObjectPtrTestClass> TestUnsafeConstObjectPtr(TestUnsafeObjectPtr_Untyped);	// unsafe const pointer to placeholder object (bound to non-UObject type)
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	// note: "safe" in this context means the pointer should be type safe because it's bound to the UObject base type, but both reference the same "unsafe" object
	TObjectPtr<UObject> TestLateResolveSafeObjectPtr(FObjectPtr({ UE::CoreUObject::Private::MakePackedObjectRef(TestUnsafeObject).EncodedRef }));
	TObjectPtr<const UObject> TestLateResolveSafeConstObjectPtr(FObjectPtr({ UE::CoreUObject::Private::MakePackedObjectRef(TestUnsafeObject).EncodedRef }));
	TObjectPtr<UObjectPtrTestClass> TestLateResolveUnsafeObjectPtr(FObjectPtr({ UE::CoreUObject::Private::MakePackedObjectRef(TestUnsafeObject).EncodedRef }));
	TObjectPtr<const UObjectPtrTestClass> TestLateResolveUnsafeConstObjectPtr(FObjectPtr({ UE::CoreUObject::Private::MakePackedObjectRef(TestUnsafeObject).EncodedRef }));
#endif

	// an unsafe packed object reference should now exist with the unsafe type bit set for an unsafe object reference
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK((UE::CoreUObject::Private::FindExistingPackedObjectRef(TestUnsafeObject).EncodedRef & (1 << UE::CoreUObject::Private::TypeIdShift)) != 0);
#endif

	// type safe object pointers should evaluate to true/non-NULL
	CHECK(TestSafeObjectPtr);
	CHECK(!!TestSafeObjectPtr);
	CHECK(NULL != TestSafeObjectPtr);
	CHECK(TestSafeObjectPtr != NULL);
	CHECK(nullptr != TestSafeObjectPtr);
	CHECK(TestSafeObjectPtr != nullptr);
	CHECK(TestSafeConstObjectPtr);
	CHECK(!!TestSafeConstObjectPtr);
	CHECK(NULL != TestSafeConstObjectPtr);
	CHECK(TestSafeConstObjectPtr != NULL);
	CHECK(nullptr != TestSafeConstObjectPtr);
	CHECK(TestSafeConstObjectPtr != nullptr);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(TestLateResolveSafeObjectPtr);
	CHECK(!!TestLateResolveSafeObjectPtr);
	CHECK(NULL != TestLateResolveSafeObjectPtr);
	CHECK(TestLateResolveSafeObjectPtr != NULL);
	CHECK(nullptr != TestLateResolveSafeObjectPtr);
	CHECK(TestLateResolveSafeObjectPtr != nullptr);
	CHECK(TestLateResolveSafeConstObjectPtr);
	CHECK(!!TestLateResolveSafeConstObjectPtr);
	CHECK(NULL != TestLateResolveSafeConstObjectPtr);
	CHECK(TestLateResolveSafeConstObjectPtr != NULL);
	CHECK(nullptr != TestLateResolveSafeConstObjectPtr);
	CHECK(TestLateResolveSafeConstObjectPtr != nullptr);
#endif

	// unsafe type object pointers should evaluate to NULL/false (for type safety), but only when late resolve is enabled (required for support)
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK_FALSE(TestUnsafeObjectPtr);
	CHECK_FALSE(!!TestUnsafeObjectPtr);
	CHECK(NULL == TestUnsafeObjectPtr);
	CHECK(TestUnsafeObjectPtr == NULL);
	CHECK(nullptr == TestUnsafeObjectPtr);
	CHECK(TestUnsafeObjectPtr == nullptr);
	CHECK_FALSE(TestUnsafeConstObjectPtr);
	CHECK_FALSE(!!TestUnsafeConstObjectPtr);
	CHECK(NULL == TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr == NULL);
	CHECK(nullptr == TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr == nullptr);
	CHECK_FALSE(TestLateResolveUnsafeObjectPtr);
	CHECK_FALSE(!!TestLateResolveUnsafeObjectPtr);
	CHECK(NULL == TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr == NULL);
	CHECK(nullptr == TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr == nullptr);
	CHECK_FALSE(TestLateResolveUnsafeConstObjectPtr);
	CHECK_FALSE(!!TestLateResolveUnsafeConstObjectPtr);
	CHECK(NULL == TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr == NULL);
	CHECK(nullptr == TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr == nullptr);
#else
	CHECK(TestUnsafeObjectPtr);
	CHECK(!!TestUnsafeObjectPtr);
	CHECK(NULL != TestUnsafeObjectPtr);
	CHECK(TestUnsafeObjectPtr != NULL);
	CHECK(nullptr != TestUnsafeObjectPtr);
	CHECK(TestUnsafeObjectPtr != nullptr);
	CHECK(TestUnsafeConstObjectPtr);
	CHECK(!!TestUnsafeConstObjectPtr);
	CHECK(NULL != TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr != NULL);
	CHECK(nullptr != TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr != nullptr);
#endif

	// an unsafe type object pointer should not equate to other unsafe type object pointers, excluding NULL and itself
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(NullObjectPtr == TestUnsafeObjectPtr);				// note: this intentionally differs from object *handles* (see above), and depends on late resolve
	CHECK(TestUnsafeObjectPtr == NullObjectPtr);				// see note directly above
	CHECK(NullObjectPtr == TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr == NullObjectPtr);
	CHECK(NullObjectPtr != TestLateResolveSafeObjectPtr);
	CHECK(TestLateResolveSafeObjectPtr != NullObjectPtr);
	CHECK(NullObjectPtr == TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr == NullObjectPtr);
	CHECK(TestSafeObjectPtr != TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr != TestSafeObjectPtr);
	CHECK(NullObjectPtr != TestLateResolveSafeConstObjectPtr);
	CHECK(TestLateResolveSafeConstObjectPtr != NullObjectPtr);
	CHECK(NullObjectPtr == TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr == NullObjectPtr);
	CHECK(TestSafeConstObjectPtr != TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr != TestSafeConstObjectPtr);
	CHECK(TestUnsafeObjectPtr == TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr == TestUnsafeObjectPtr);
	CHECK(TestUnsafeConstObjectPtr == TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr == TestUnsafeConstObjectPtr);
	CHECK(TestLateResolveSafeObjectPtr != TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr != TestLateResolveSafeObjectPtr);
	CHECK(TestLateResolveSafeConstObjectPtr != TestLateResolveUnsafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr != TestLateResolveSafeConstObjectPtr);
	CHECK(TestLateResolveUnsafeObjectPtr == TestLateResolveUnsafeObjectPtr);
	CHECK(TestLateResolveUnsafeConstObjectPtr == TestLateResolveUnsafeConstObjectPtr);
#else
	CHECK(NullObjectPtr != TestUnsafeObjectPtr);				// note: if late resolve is disabled, these will no longer be equivalent
	CHECK(TestUnsafeObjectPtr != NullObjectPtr);				// see note directly above
	CHECK(NullObjectPtr != TestUnsafeConstObjectPtr);
	CHECK(TestUnsafeConstObjectPtr != NullObjectPtr);
#endif

	// an unsafe type object should evaluate the object's attributes correctly
	CHECK(TestUnsafeObjectPtr.GetName() == TestUnsafeObject->GetName());
	CHECK(TestUnsafeObjectPtr.GetFName() == TestUnsafeObject->GetFName());
	CHECK(TestUnsafeObjectPtr.GetPathName() == TestUnsafeObject->GetPathName());
	CHECK(TestUnsafeObjectPtr.GetFullName() == TestUnsafeObject->GetFullName());
	CHECK(TestUnsafeObjectPtr.GetOuter() == TestUnsafeObject->GetOuter());
	CHECK(TestUnsafeObjectPtr.GetClass() == TestUnsafeObject->GetClass());
	CHECK(TestUnsafeObjectPtr.GetPackage() == TestUnsafeObject->GetPackage());
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(TestLateResolveUnsafeObjectPtr.GetName() == TestUnsafeObject->GetName());
	CHECK(TestLateResolveUnsafeObjectPtr.GetFName() == TestUnsafeObject->GetFName());
	CHECK(TestLateResolveUnsafeObjectPtr.GetPathName() == TestUnsafeObject->GetPathName());
	CHECK(TestLateResolveUnsafeObjectPtr.GetFullName() == TestUnsafeObject->GetFullName());
	CHECK(TestLateResolveUnsafeObjectPtr.GetOuter() == TestUnsafeObject->GetOuter());
	CHECK(TestLateResolveUnsafeObjectPtr.GetClass() == TestUnsafeObject->GetClass());
	CHECK(TestLateResolveUnsafeObjectPtr.GetPackage() == TestUnsafeObject->GetPackage());
#endif

	// the type safety and queries above should not have resolved an object pointer that's using late resolve
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK_FALSE(TestLateResolveSafeObjectPtr.IsResolved());
	CHECK_FALSE(TestLateResolveUnsafeObjectPtr.IsResolved());
	CHECK_FALSE(TestLateResolveSafeConstObjectPtr.IsResolved());
	CHECK_FALSE(TestLateResolveUnsafeConstObjectPtr.IsResolved());
#endif

	// a type safe object pointer should resolve to a non-NULL value when dereferenced
	CHECK(TestSafeObjectPtr.Get() == TestUnsafeObject);
	CHECK(TestSafeConstObjectPtr.Get() == TestUnsafeObject);
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(TestLateResolveSafeObjectPtr.Get() == TestUnsafeObject);
	CHECK(TestLateResolveSafeConstObjectPtr.Get() == TestUnsafeObject);
#endif

	// an unsafe type object pointer should resolve to NULL when dereferenced (for type safety), but only when late resolve is enabled (required for support)
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(TestUnsafeObjectPtr.Get() == nullptr);
	CHECK(TestUnsafeConstObjectPtr.Get() == nullptr);
	CHECK(TestLateResolveUnsafeObjectPtr.Get() == nullptr);
	CHECK(TestLateResolveUnsafeConstObjectPtr.Get() == nullptr);
#else
	CHECK(TestUnsafeObjectPtr.Get() == TestUnsafeObject);
	CHECK(TestUnsafeConstObjectPtr.Get() == TestUnsafeObject);
#endif

	// all pointers should now be resolved
	CHECK(TestSafeObjectPtr.IsResolved());
	CHECK(TestUnsafeObjectPtr.IsResolved());
	CHECK(TestSafeConstObjectPtr.IsResolved());
	CHECK(TestUnsafeConstObjectPtr.IsResolved());
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	CHECK(TestLateResolveSafeObjectPtr.IsResolved());
	CHECK(TestLateResolveUnsafeObjectPtr.IsResolved());
	CHECK(TestLateResolveSafeConstObjectPtr.IsResolved());
	CHECK(TestLateResolveUnsafeConstObjectPtr.IsResolved());
#endif
}
#endif

#endif
