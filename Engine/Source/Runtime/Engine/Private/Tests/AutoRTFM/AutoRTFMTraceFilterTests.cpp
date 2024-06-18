// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "TraceFilter.h"
#include "AutoRTFM/AutoRTFM.h"
#include "AutoRTFMTestObject.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMTraceFilterTests, "AutoRTFM + FTraceFilter", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMTraceFilterTests::RunTest(const FString & Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMTraceFilterTests' test. AutoRTFM disabled.")));
		return true;
	}

#if TRACE_FILTERING_ENABLED
	{
		UAutoRTFMTestObject* Object = NewObject<UAutoRTFMTestObject>();

		FTraceFilter::SetObjectIsTraceable(Object, false);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FTraceFilter::SetObjectIsTraceable(Object, true);
				AutoRTFM::AbortTransaction();
			});

		TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				FTraceFilter::SetObjectIsTraceable(Object, true);
			});

		TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([Object]
					{
						FTraceFilter::SetObjectIsTraceable(Object, false);
					});

				AutoRTFM::AbortTransaction();
			});

		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnCommit([Object]
					{
						FTraceFilter::SetObjectIsTraceable(Object, true);
					});
			});

		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));
	}

	{
		UAutoRTFMTestObject* Object = NewObject<UAutoRTFMTestObject>();

		FTraceFilter::SetObjectIsTraceable(Object, false);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		FTraceFilter::MarkObjectTraceable(Object);
		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));

		FTraceFilter::SetObjectIsTraceable(Object, false);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				FTraceFilter::MarkObjectTraceable(Object);
				AutoRTFM::AbortTransaction();
			});

		TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				FTraceFilter::MarkObjectTraceable(Object);
			});

		TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));

		FTraceFilter::SetObjectIsTraceable(Object, false);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([Object]
					{
						FTraceFilter::MarkObjectTraceable(Object);
					});

				AutoRTFM::AbortTransaction();
			});

		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));

		FTraceFilter::SetObjectIsTraceable(Object, false);
		TEST_CHECK_TRUE(!FTraceFilter::IsObjectTraceable(Object));

		Result = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnCommit([Object]
					{
						FTraceFilter::MarkObjectTraceable(Object);
					});
			});

		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object));
	}

	{
		UAutoRTFMTestObject* Object = NewObject<UAutoRTFMTestObject>();

		bool bTraceable = true;

		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				bTraceable = FTraceFilter::IsObjectTraceable(Object);
			});

		TEST_CHECK_TRUE(AutoRTFM::ETransactionResult::Committed == Result);
		TEST_CHECK_TRUE(FTraceFilter::IsObjectTraceable(Object) == bTraceable);
	}

	return true;
#else
	ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMTraceFilterTests' test. Trace filtering disabled.")));
	return true;
#endif
}

#endif //WITH_DEV_AUTOMATION_TESTS
