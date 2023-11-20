// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSource.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCaptureSourceTest, "Plugin.CaptureSourceFramework.CaptureSource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::MediumPriority)

static const FString TestCaptureSourceFactoryId;
static const FString TestCaptureSourceName;

class FTestCapability final : public FCaptureSourceCapability
{
public:
	static const FString Name;

	static const FString TestEvent;

	FTestCapability()
		: FCaptureSourceCapability(Name)
	{
		RegisterEvent(TestEvent);
	}
};

class FTestCaptureSource final : public FCaptureSource
{
public:
	FTestCaptureSource(const FString& InName) :
		FCaptureSource(TestCaptureSourceFactoryId, InName)
	{
		TUniquePtr<FTestCapability> TestCapability = MakeUnique<FTestCapability>();

		AddCapability(TestCapability.Get());

		OwnedCapabilities.Add(MoveTemp(TestCapability));
	}

	virtual FCaptureVoidResult Start() final
	{
		return MakeValue();
	}

	virtual FCaptureVoidResult Stop() final
	{
		return MakeValue();
	}

private:

	TArray<TUniquePtr<FCaptureSourceCapability>> OwnedCapabilities;
};

TUniquePtr<FCaptureSource> CaptureSource;

END_DEFINE_SPEC(FCaptureSourceTest)

const FString FCaptureSourceTest::TestCaptureSourceFactoryId = TEXT("TestCaptureSourceFactoryId");
const FString FCaptureSourceTest::TestCaptureSourceName = TEXT("TestCaptureSource");

const FString FCaptureSourceTest::FTestCapability::Name = TEXT("TestCapability");
const FString FCaptureSourceTest::FTestCapability::TestEvent = TEXT("TestEvent");

void FCaptureSourceTest::Define()
{
	CaptureSource = MakeUnique<FTestCaptureSource>(TestCaptureSourceName);

	Describe("GetCapabilities()", [this]()
	{
		It("should return all registered capabilities", [this]()
		{
			const TArray<FString> Capabilities = CaptureSource->GetCapabilities();

			UTEST_EQUAL("One capability is registered", Capabilities.Num(), 1);
			UTEST_EQUAL("Registered capability must match", Capabilities[0], FTestCapability::Name);
			return true;
		});
	});

	Describe("GetCapability()", [this]()
	{
		It("should return registered capability", [this]()
		{
			FCaptureSourceCapability* TestCapability = CaptureSource->GetCapability(FTestCapability::Name);

			UTEST_NOT_NULL("Fetched capability is valid", TestCapability);
			UTEST_EQUAL("Fetched capability name must match", TestCapability->GetName(), FTestCapability::Name);
			return true;
		});

		It("should return invalid capability (nullptr)", [this]()
		{
			FCaptureSourceCapability* NotExistingCapability = CaptureSource->GetCapability(TEXT("NotExistingCapability"));

			UTEST_NULL("Fetched capability is nullptr", NotExistingCapability);
			return true;
		});
	});

	Describe("GetCaptureSourceFactoryId()", [this]()
	{
		It("should return correct capture source factory id", [this]()
		{
			const FString CaptureSourceFactoryId = CaptureSource->GetCaptureSourceFactoryId();

			UTEST_EQUAL("Capture source factory id must match", CaptureSourceFactoryId, TestCaptureSourceFactoryId);
			return true;
		});
	});

	Describe("GetName()", [this]()
	{
		It("should return correct capture source name", [this]()
		{
			const FString Name = CaptureSource->GetName();

			UTEST_EQUAL("Capture source name must match", Name, TestCaptureSourceName);
			return true;
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS