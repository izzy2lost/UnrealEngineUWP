// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingTests_Shared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UAsyncLoadingTests_Shared::FOnPostLoadDelegate UAsyncLoadingTests_Shared::OnPostLoad;
UAsyncLoadingTests_Shared::FOnSerializeDelegate UAsyncLoadingTests_Shared::OnSerialize;
UAsyncLoadingTests_Shared::FOnIsReadyForAsyncPostLoadDelegate UAsyncLoadingTests_Shared::OnIsReadyForAsyncPostLoad;
UAsyncLoadingTests_Shared::FOnIsPostLoadThreadSafeDelegate UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe;

#if WITH_DEV_AUTOMATION_TESTS

void FLoadingTestsScope::CreateObjects()
{
	Package1 = CreatePackage(PackagePath1);
	Object1 = NewObject<UAsyncLoadingTests_Shared>(Package1, ObjectName, RF_Public | RF_Standalone);

	Package2 = CreatePackage(PackagePath2);
	Object2 = NewObject<UAsyncLoadingTests_Shared>(Package2, ObjectName, RF_Public | RF_Standalone);
}

void FLoadingTestsScope::MutateObjects()
{
	// This is the soft reference that we want to test loading for
	Object1->SoftReference = Object2;
}

void FLoadingTestsScope::SaveObjects()
{
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
	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1) != nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath1) != nullptr);
	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath2) != nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath2) != nullptr);

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1) == nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath1) == nullptr);
	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath2) == nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath2) == nullptr);
}

void FLoadingTestsScope::LoadObjects()
{
	LoadPackage(nullptr, PackagePath1, LOAD_None);

	UAsyncLoadingTests_Shared* LocalObject1 = FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1);
	AutomationTest.TestTrue(TEXT("The object should have been properly loaded recursively"), LocalObject1 != nullptr);
	if (LocalObject1)
	{
		AutomationTest.TestFalse(TEXT("The object should have been properly loaded recursively"), LocalObject1->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
	}
}

void FLoadingTestsScope::CleanupObjects()
{
	// GC and make sure everything gets cleaned up before exiting
	Object1 = FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1);
	Object2 = FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath2);
	
	if (Object1)
	{
		Object1->ClearFlags(RF_Standalone);
	}
	if (Object2)
	{
		Object2->ClearFlags(RF_Standalone);
	}

	Package1 = FindObject<UPackage>(nullptr, PackagePath1);
	Package2 = FindObject<UPackage>(nullptr, PackagePath2);

	if (Package1)
	{
		Package1->GetMetaData()->ClearFlags(RF_Standalone);
	}
	
	if (Package2)
	{
		Package2->GetMetaData()->ClearFlags(RF_Standalone);
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath1) == nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath1) == nullptr);
	check(FindObject<UAsyncLoadingTests_Shared>(nullptr, ObjectPath2) == nullptr);
	check(FindObject<UPackage>(nullptr, PackagePath2) == nullptr);

	UAsyncLoadingTests_Shared::OnPostLoad.Unbind();
	UAsyncLoadingTests_Shared::OnSerialize.Unbind();
	UAsyncLoadingTests_Shared::OnIsPostLoadThreadSafe.Unbind();
	UAsyncLoadingTests_Shared::OnIsReadyForAsyncPostLoad.Unbind();
}

#endif // WITH_DEV_AUTOMATION_TESTS
