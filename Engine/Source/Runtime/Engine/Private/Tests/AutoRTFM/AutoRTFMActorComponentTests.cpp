// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "AutoRTFM/AutoRTFM.h"
#include "AutoRTFMTestActor.h"
#include "AutoRTFMTestBodySetup.h"
#include "AutoRTFMTestLevel.h"
#include "AutoRTFMTestObject.h"
#include "AutoRTFMTestPrimitiveComponent.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Chaos/Core.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_CHECK_TRUE(b) do \
{ \
	if (!(b)) \
	{ \
		FString String(FString::Printf(TEXT("FAILED: %s:%u"), TEXT(__FILE__), __LINE__)); \
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, String)); \
		return false; \
	} \
} while(false)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMActorComponentTests, "AutoRTFM + UActorComponent", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMActorComponentTests::RunTest(const FString & Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMActorComponentTests' test. AutoRTFM disabled.")));
		return true;
	}

	UWorld* World = NewObject<UWorld>();
	World->CreatePhysicsScene(nullptr);

	UAutoRTFMTestLevel* Level = NewObject<UAutoRTFMTestLevel>();
	Level->OwningWorld = World;
	AAutoRTFMTestActor* Actor = NewObject<AAutoRTFMTestActor>(Level);
	UAutoRTFMTestPrimitiveComponent* Component = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);

	// Helper to re-add actor handles after each unregister.
	auto ResetActorHandle = [&]
		{
			Component->BodyInstance.ActorHandle = Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle());
			Component->BodyInstance.ActorHandle->GetParticle_LowLevel()->SetGeometry(MakeImplicitObjectPtr<Chaos::FSphere>(Chaos::FVec3(1, 2, 3), 1));
			World->GetPhysicsScene()->GetSolver()->RegisterObject(Component->BodyInstance.ActorHandle);
		};

	ResetActorHandle();

	Component->RegisterComponent();
	TEST_CHECK_TRUE(Component->IsRegistered());
	Component->UnregisterComponent();
	TEST_CHECK_TRUE(!Component->IsRegistered());
	TEST_CHECK_TRUE(!Component->BodyInstance.ActorHandle);

	ResetActorHandle();

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponent();

			if (Component->IsRegistered())
			{
				AutoRTFM::AbortTransaction();
			}
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(!Component->IsRegistered());

	bool bWasRegistered = false;

	Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponent();
			bWasRegistered = Component->IsRegistered();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(bWasRegistered);
	TEST_CHECK_TRUE(Component->IsRegistered());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(Component->IsRegistered());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(!Component->IsRegistered());

	// This test requires us to have a fresh body instance so that it has to be created during the register.
	Component->BodyInstance = FBodyInstance();

	// And we need a valid body setup so that there is shapes created too.
	Component->BodySetup = NewObject<UAutoRTFMTestBodySetup>();
	Component->BodySetup->AggGeom.SphereElems.Add(FKSphereElem(1.0f));

	Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponentWithWorld(World);
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(!Component->IsRegistered());

	Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponentWithWorld(World);
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(Component->IsRegistered());

	Component->UnregisterComponent();
	TEST_CHECK_TRUE(!Component->IsRegistered());

	FBodyInstance SomeInstance;

	// This test requires us to have a fresh body instance so that it has to be created during the register.
	Component->BodyInstance = FBodyInstance();
	Component->BodyInstance.bSimulatePhysics = 1;
	Component->BodyInstance.WeldParent = &SomeInstance;
	TEST_CHECK_TRUE(Component->IsWelded());

	UAutoRTFMTestBodySetup* BodySetup = NewObject<UAutoRTFMTestBodySetup>();
	BodySetup->AggGeom.SphereElems.Add(FKSphereElem(1.0f));

	Component->BodyInstance.BodySetup = BodySetup;

	UAutoRTFMTestPrimitiveComponent* Parent0 = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);
	UAutoRTFMTestPrimitiveComponent* Parent1 = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);

	Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent0);
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(Component->IsWelded());
	TEST_CHECK_TRUE(&SomeInstance == Component->BodyInstance.WeldParent);

	Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent0);
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(!Component->IsWelded());
	TEST_CHECK_TRUE(nullptr == Component->BodyInstance.WeldParent);

	Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent1);
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(!Component->IsWelded());

	Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent1);
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(!Component->IsWelded());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnWeldFromParent();
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(!Component->IsWelded());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnWeldFromParent();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(!Component->IsWelded());

	TEST_CHECK_TRUE(!Component->IsRegistered());
	Component->RegisterComponent();

	UAutoRTFMTestObject* const Object = NewObject<UAutoRTFMTestObject>();

	Component->OnComponentPhysicsStateChanged.AddDynamic(Object, &UAutoRTFMTestObject::OnComponentPhysicsStateChanged);

	TEST_CHECK_TRUE(!Object->bHitOnComponentPhysicsStateChanged);

	Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
			AutoRTFM::AbortTransaction();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TEST_CHECK_TRUE(!Object->bHitOnComponentPhysicsStateChanged);

	Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
		});

	TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
	TEST_CHECK_TRUE(Object->bHitOnComponentPhysicsStateChanged);

	return true;
}

#undef TEST_CHECK_TRUE

#endif //WITH_DEV_AUTOMATION_TESTS
