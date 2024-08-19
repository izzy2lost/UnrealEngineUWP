// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "AutoRTFM/AutoRTFM.h"
#include "Chaos/RefCountedObject.h"
#include "Templates/UniquePtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMChaosRefCountedObject, "AutoRTFM + ChaosRefCountedObject", \
	                             EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | \
								 EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMChaosRefCountedObject::RunTest(const FString& Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, 
								                TEXT("SKIPPED 'FAutoRTFMChaosRefCountedObject' test. AutoRTFM disabled.")));
		return true;
	}

	// It should be safe to declare a ref-counted object which is not used.
	AutoRTFM::Transact([&]
		{
			Chaos::FChaosRefCountedObject DoNothingObject;
		});

	// Adding and releasing a reference to a transient object will cause it to delete itself.
	{
		auto* TransientObject = new Chaos::FChaosRefCountedObject();
		AutoRTFM::Transact([&]
			{
				TransientObject->AddRef();
				TransientObject->Release();
			});
	}

	// Adding and releasing a reference to a persistent object will not delete it.
	// This tests uses TUniquePtr to perform the deletion when the object falls out of scope.
	{
		auto PersistentObject = MakeUnique<Chaos::FChaosRefCountedObject>();
		PersistentObject->MakePersistent();
		AutoRTFM::Transact([&]
			{
				PersistentObject->AddRef();
				PersistentObject->Release();
			});
	}

	// It is safe to make an object persistent inside of a transaction.
	{
		auto PersistentObject = MakeUnique<Chaos::FChaosRefCountedObject>();
		AutoRTFM::Transact([&]
			{
				PersistentObject->MakePersistent();
				PersistentObject->AddRef();
				PersistentObject->Release();
			});
	}

	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
