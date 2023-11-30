// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceManager.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCaptureSourceManagerTest, "Plugins.CaptureSourceFramework.CaptureSourceManager", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::MediumPriority)

class FTestCaptureSource final : public FCaptureSource
{
public:
	FTestCaptureSource(const FString& InName)
		: FCaptureSource(FTestCaptureSourceFactory::Id, InName)
	{
	}

	virtual FCaptureVoidResult Start() final
	{
		return MakeValue();
	}

	virtual FCaptureVoidResult Stop() final
	{
		return MakeValue();
	}
};

class FTestCaptureSourceFactory final : public FCaptureSourceFactory
{
public:

	static const FString Id;

	static const FString CreationParam;

	virtual FString GetCaptureSourceFactoryName() const override
	{
		return TEXT("Test Capture Source Factory");
	}

	virtual FString GetCaptureSourceFactoryId() const override
	{
		return Id;
	}

	virtual bool IsDiscoverable() const override
	{
		return bIsDiscoverable;
	}

	void SetDiscoverable(bool bInIsDiscoverable)
	{
		bIsDiscoverable = bInIsDiscoverable;
	}

private:

	virtual void DiscoverImpl(FDiscoverCallback InCallback, FCaptureVoidResult& OutResult) override
	{
		TArray<FCaptureSourceResult> DiscoveredCaptureSources;

		DiscoveredCaptureSources.Add(MakeValue(MakeUnique<FTestCaptureSource>(TEXT("DefaultName"))));

		InCallback.ExecuteIfBound(MoveTemp(DiscoveredCaptureSources));

		OutResult = MakeValue();
	}

	virtual FCaptureSourceResult CreateCaptureSourceImpl(const FString& InName, const TMap<FString, FPropertyValue>& InCreationParamsValue) override
	{
		if (!InCreationParamsValue.IsEmpty())
		{
			return MakeError(FCaptureSourceError(ECaptureSourceError::InvalidArgument));
		}

		return MakeValue(MakeUnique<FTestCaptureSource>(InName));
	}

	virtual void CreateCaptureSourceDescriptorImpl(FCaptureSourceDescriptor& OutDescriptor) const override
	{
		OutDescriptor.SetDiscoverable(bIsDiscoverable);
	}

	bool bIsDiscoverable;
};

FCaptureSourceManager Manager;
FTestCaptureSourceFactory* FactoryPtr = nullptr;

using FCreationParamValues = TMap<FString, FPropertyValue>;

END_DEFINE_SPEC(FCaptureSourceManagerTest)

const FString FCaptureSourceManagerTest::FTestCaptureSourceFactory::Id = TEXT("TestCaptureSourceFactoryId");

void FCaptureSourceManagerTest::Define()
{
	TUniquePtr<FTestCaptureSourceFactory> Factory = MakeUnique<FTestCaptureSourceFactory>();
	FactoryPtr = Factory.Get();

	Manager.RegisterCaptureSourceFactory(MoveTemp(Factory));
	bSuppressLogs = true;

	Describe("CreateCaptureSource()", [this]()
	{
		It("returns a valid capture source", [this]()
		{
			FString Name = TEXT("Name");
			FCaptureSourceManager::FCaptureSourceResult Result = Manager.CreateCaptureSource(FTestCaptureSourceFactory::Id, MoveTemp(Name), FCreationParamValues());

			UTEST_TRUE("Result should be valid", Result.HasValue());

			return true;
		});

		It("returns an error as invalid capture source factory is provided", [this]()
		{
			FString Name = TEXT("Name");
			FString InvalidCaptureSourceFactory = TEXT("Invalid");
			FCaptureSourceManager::FCaptureSourceResult Result = Manager.CreateCaptureSource(MoveTemp(InvalidCaptureSourceFactory), MoveTemp(Name), FCreationParamValues());

			UTEST_TRUE("Result should contain an error", Result.HasError());

			return true;
		});

		It("returns an error as number of arguments is invalid", [this]()
		{
			FString Name = TEXT("Name");

			FCreationParamValues ParamValues;
			ParamValues.Add(TEXT("Param"), MakePropertyValue<FString>(TEXT("Value")));

			FCaptureSourceManager::FCaptureSourceResult Result = Manager.CreateCaptureSource(FTestCaptureSourceFactory::Id, MoveTemp(Name), MoveTemp(ParamValues));

			UTEST_TRUE("Result should contain an error", Result.HasError());

			return true;
		});
	});

	Describe("DiscoverCaptureSources()", [this]()
	{
		It("runs discovery without problems as factory is discoverable", [this]()
		{
			FactoryPtr->SetDiscoverable(true);

			TSharedPtr<bool> CbResult = MakeShared<bool>(false);

			FCaptureSourceManager::FDiscoverCallback Cb = 
				FCaptureSourceManager::FDiscoverCallback::CreateLambda([this, CbResult](TArray<FCaptureSourceManager::FCaptureSourceResult> InResults)
			{
				if (!TestTrue("Result contains a single valid source", InResults[0].IsValid()))
				{
					*CbResult = false;
				}

				*CbResult = true;
			});

			Manager.DiscoverCaptureSources(MoveTemp(Cb));

			UTEST_TRUE("Result from callback should be true", *CbResult);

			return true;
		});

		It("returns an error as factory isn't discoverable", [this]()
		{
			FactoryPtr->SetDiscoverable(false);

			TSharedPtr<bool> CbResult = MakeShared<bool>(false);

			FCaptureSourceManager::FDiscoverCallback Cb =
				FCaptureSourceManager::FDiscoverCallback::CreateLambda([this, CbResult](TArray<FCaptureSourceManager::FCaptureSourceResult> InResults)
			{
				if (!TestTrue("Result contains an error", InResults.IsEmpty()))
				{
					*CbResult = false;
				}

				*CbResult = true;
			});

			Manager.DiscoverCaptureSources(MoveTemp(Cb));

			UTEST_TRUE("Result from callback should be true", *CbResult);

			return true;
		});
	});

	Describe("DiscoverCaptureSourcesForFactory()", [this]()
	{
		It("runs discovery without problems as factory is discoverable", [this]()
		{
			FactoryPtr->SetDiscoverable(true);

			TSharedPtr<bool> CbResult = MakeShared<bool>(false);

			FCaptureSourceManager::FDiscoverCallback Cb =
				FCaptureSourceManager::FDiscoverCallback::CreateLambda([this, CbResult](TArray<FCaptureSourceManager::FCaptureSourceResult> InResults)
			{
				if (!TestTrue("Result contains a single valid source", InResults[0].IsValid()))
				{
					*CbResult = false;
				}

				*CbResult = true;
			});

			Manager.DiscoverCaptureSourcesForFactory(FTestCaptureSourceFactory::Id, MoveTemp(Cb));

			UTEST_TRUE("Result from callback should be true", *CbResult);

			return true;
		});

		It("returns an error as factory isn't discoverable", [this]()
		{
			FactoryPtr->SetDiscoverable(false);

			TSharedPtr<bool> CbResult = MakeShared<bool>(false);

			FCaptureSourceManager::FDiscoverCallback Cb =
				FCaptureSourceManager::FDiscoverCallback::CreateLambda([this, CbResult](TArray<FCaptureSourceManager::FCaptureSourceResult> InResults)
			{
				if (!TestTrue("Result contains an error", InResults.IsEmpty()))
				{
					*CbResult = false;
				}

				*CbResult = true;
			});

			Manager.DiscoverCaptureSourcesForFactory(FTestCaptureSourceFactory::Id, MoveTemp(Cb));

			UTEST_TRUE("Result from callback should be true", *CbResult);

			return true;
		});
	});

	Describe("RemoveCaptureSource()", [this]()
	{
		It("removes all created capture sources", [this]()
		{
			TArray<FCaptureSourceId> Ids = Manager.GetCaptureSourceIds();

			for (FCaptureSourceId Id : Ids)
			{
				Manager.RemoveCaptureSource(Id);
			}

			UTEST_TRUE("Array of capture sources should be empty after removal", Manager.GetCaptureSourceIds().IsEmpty());

			return true;
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS