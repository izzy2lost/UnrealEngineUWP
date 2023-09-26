// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_RecursiveLoads.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncLoadingTests_RecursiveLoads)

UAsyncLoadingTests_RecursiveLoads::FOnPostLoadEvent UAsyncLoadingTests_RecursiveLoads::OnPostLoadEvent;
UAsyncLoadingTests_RecursiveLoads::FOnSerializeEvent UAsyncLoadingTests_RecursiveLoads::OnSerializeEvent;

#if WITH_DEV_AUTOMATION_TESTS

class FLoadingTests_RecursiveLoads : public FAutomationTestBase
{
public:
	using FAutomationTestBase::FAutomationTestBase;
protected:
	static constexpr const TCHAR* ObjectName = TEXT("TestObject");
	static constexpr const TCHAR* PackagePath1 = TEXT("/Engine/TestRecursiveLoads_Package1");
	static constexpr const TCHAR* ObjectPath1 = TEXT("/Engine/TestRecursiveLoads_Package1.TestObject");
	static constexpr const TCHAR* PackagePath2 = TEXT("/Engine/TestRecursiveLoads_Package2");
	static constexpr const TCHAR* ObjectPath2 = TEXT("/Engine/TestRecursiveLoads_Package2.TestObject");

	virtual void CreateObjects()
	{
		UPackage* Package1 = CreatePackage(PackagePath1);
		UAsyncLoadingTests_RecursiveLoads* Object1 = NewObject<UAsyncLoadingTests_RecursiveLoads>(Package1, ObjectName, RF_Public | RF_Standalone);

		UPackage* Package2 = CreatePackage(PackagePath2);
		UAsyncLoadingTests_RecursiveLoads* Object2 = NewObject<UAsyncLoadingTests_RecursiveLoads>(Package2, ObjectName, RF_Public | RF_Standalone);

		// This is the soft reference that we want to test loading for
		Object1->SoftReference = Object2;

		// To avoid an error on save, we need to mark the package as fully loaded.
		Package1->MarkAsFullyLoaded();
		Package2->MarkAsFullyLoaded();

		// Save packages to disk.
		check(UPackage::SavePackage(Package1, nullptr, *FPackageName::LongPackageNameToFilename(PackagePath1, FPackageName::GetAssetPackageExtension()), FSavePackageArgs()));
		check(UPackage::SavePackage(Package2, nullptr, *FPackageName::LongPackageNameToFilename(PackagePath2, FPackageName::GetAssetPackageExtension()), FSavePackageArgs()));

		// Remove RF_Standalone from top level object.
		Object1->ClearFlags(RF_Standalone);
		Object2->ClearFlags(RF_Standalone);
		
		// Remove RF_Standalone from the UMetaData otherwise the package will not GC.
		Package1->GetMetaData()->ClearFlags(RF_Standalone);
		Package2->GetMetaData()->ClearFlags(RF_Standalone);

		// GC and make sure everything gets cleaned up before loading
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1) != nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath1) != nullptr);
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2) != nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath2) != nullptr);

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1) == nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath1) == nullptr);
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2) == nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath2) == nullptr);
	}

	virtual void LoadObjects()
	{
		LoadPackage(nullptr, PackagePath1, LOAD_None);

		UAsyncLoadingTests_RecursiveLoads* Object2 = FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2);
		TestFalse(TEXT("The object should have been properly loaded recursively"), Object2->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));

		UAsyncLoadingTests_RecursiveLoads* Object1 = FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1);
		TestFalse(TEXT("The object should have been properly loaded recursively"), Object1->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
	}

	virtual void CleanupObjects()
	{
		// GC and make sure everything gets cleaned up before exiting
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1) != nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath1) != nullptr);
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2) != nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath2) != nullptr);

		FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1)->ClearFlags(RF_Standalone);
		FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2)->ClearFlags(RF_Standalone);

		FindObject<UPackage>(nullptr, PackagePath1)->GetMetaData()->ClearFlags(RF_Standalone);
		FindObject<UPackage>(nullptr, PackagePath2)->GetMetaData()->ClearFlags(RF_Standalone);

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath1) == nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath1) == nullptr);
		check(FindObject<UAsyncLoadingTests_RecursiveLoads>(nullptr, ObjectPath2) == nullptr);
		check(FindObject<UPackage>(nullptr, PackagePath2) == nullptr);
	}

	bool CanRunInEnvironment(const FString& TestParams, FString* OutReason, bool* OutWarn) const override
	{
		if (GetLoaderName() != TEXT("ZenLoader"))
		{
			if (OutReason)
			{
				*OutReason = FString::Printf(TEXT("Test %s is for ZenLoader only. Cannot run on non-compliant loader currently active: %s"), *GetTestName(), *GetLoaderName().ToString());
			}
			return false;
		}

		return true;
	}

	bool DoTest()
	{
		// Just make sure the async loading queue is empty before beginning.
		FlushAsyncLoading();

		// Creation phase
		CreateObjects();

		// Loading phase
		LoadObjects();

		// Cleanup phase
		CleanupObjects();

		return true;
	}
};

/**
 * This test validates loading an object synchronously during serialize.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromSerialize, 
	FLoadingTests_RecursiveLoads, 
	TEXT("System.Engine.Loading.RecursiveLoads.FromSerialize"),
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FromSerialize::RunTest(const FString& Parameters)
{
	FDelegateHandle DelegateHandle = UAsyncLoadingTests_RecursiveLoads::OnSerializeEvent.AddLambda(
		[this](FArchive& Ar, UAsyncLoadingTests_RecursiveLoads* Object)
		{
			if (Ar.IsLoading())
			{
				if (UObject* Obj = Object->SoftReference.LoadSynchronous())
				{
					TestTrue(TEXT("Recursive loads in serialize should be deserialized"), !Obj->HasAnyFlags(RF_NeedLoad));
					TestTrue(TEXT("Recursive loads in serialize should have their postload delayed until the caller is also ready to postload"), Obj->HasAnyFlags(RF_NeedPostLoad));
				}
			}
		}
	);

	ON_SCOPE_EXIT
	{
		UAsyncLoadingTests_RecursiveLoads::OnSerializeEvent.Remove(DelegateHandle);
	};

	return DoTest();
}

/**
 * This test validates loading an object synchronously during postload.
 */
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FromPostLoad,
	FLoadingTests_RecursiveLoads,
	TEXT("System.Engine.Loading.RecursiveLoads.FromPostLoad"),
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter
)

bool FLoadingTests_RecursiveLoads_FromPostLoad::RunTest(const FString& Parameters)
{
	FDelegateHandle DelegateHandle = UAsyncLoadingTests_RecursiveLoads::OnPostLoadEvent.AddLambda(
		[this](UAsyncLoadingTests_RecursiveLoads* Object)
		{
			if (UObject* Obj = Object->SoftReference.LoadSynchronous())
			{
				TestFalse(TEXT("Recursive loads in postload should be fully loaded"), Obj->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
			}
		}
	);

	ON_SCOPE_EXIT
	{
		UAsyncLoadingTests_RecursiveLoads::OnPostLoadEvent.Remove(DelegateHandle);
	};

	return DoTest();
}

/**
 * This test validates an error is emitted when flushing a requestid that is not a partial load from inside a recursive function.
 */
class FLoadingTests_RecursiveLoads_FullFlushFromSerialize_Base : public FLoadingTests_RecursiveLoads
{
public:
	using FLoadingTests_RecursiveLoads::FLoadingTests_RecursiveLoads;
protected:
	int32 RequestId;

	virtual void LoadObjects() override
	{
		// Create a request before starting the loading test so we get a request that is not tagged as partial.
		RequestId = LoadPackageAsync(PackagePath2);

		FLoadingTests_RecursiveLoads::LoadObjects();
	}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FLoadingTests_RecursiveLoads_FullFlushFromSerialize,
	FLoadingTests_RecursiveLoads_FullFlushFromSerialize_Base,
	TEXT("System.Engine.Loading.RecursiveLoads.FullFlushFromSerialize"),
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter
)
bool FLoadingTests_RecursiveLoads_FullFlushFromSerialize::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("can lead to undefined behavior and is strongly discouraged"), EAutomationExpectedErrorFlags::Contains);

	FDelegateHandle DelegateHandle = UAsyncLoadingTests_RecursiveLoads::OnSerializeEvent.AddLambda(
		[this](FArchive& Ar, UAsyncLoadingTests_RecursiveLoads* Object)
		{
			// Do not try to flush ourself as this would lead to a fatal error :)
			// Just flush Package2 when we're in Package1
			if (Ar.IsLoading() && Object->GetPathName() == ObjectPath1)
			{
				// Flush the requestId that has been created outside of the recursive load. This request
				// should be a full request and flushing it should result in an error being reported.
				FlushAsyncLoading(RequestId);
			}
		}
	);

	ON_SCOPE_EXIT
	{
		UAsyncLoadingTests_RecursiveLoads::OnSerializeEvent.Remove(DelegateHandle);
	};

	return DoTest();
}

#undef TEST_NAME_ROOT
#endif // WITH_DEV_AUTOMATION_TESTS
