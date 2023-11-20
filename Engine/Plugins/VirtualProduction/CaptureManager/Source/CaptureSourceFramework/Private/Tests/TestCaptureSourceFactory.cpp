// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceFactory.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCaptureSourceFactoryTest, "Plugin.CaptureSourceFramework.CaptureSourceFactory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::MediumPriority)

class FTestCaptureSource final : public FCaptureSource
{
public:
	FTestCaptureSource(const FString& InName, const FString& InParamValue) 
		: FCaptureSource(FTestCaptureSourceFactory::Id, InName)
		, ParamValue(InParamValue)
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

	FString ParamValue;
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

		DiscoveredCaptureSources.Add(MakeValue(MakeUnique<FTestCaptureSource>(TEXT("DefaultName"), TEXT("DefaultCreationParam"))));

		InCallback.ExecuteIfBound(MoveTemp(DiscoveredCaptureSources));

		OutResult = MakeValue();
	}

	virtual FCaptureSourceResult CreateCaptureSourceImpl(const FString& InName, const TMap<FString, FPropertyValue>& InCreationParamsValue) override
	{
		if (InCreationParamsValue.Num() != 1 && !InCreationParamsValue.Contains(CreationParam))
		{
			return MakeError(FCaptureSourceError(ECaptureSourceError::InvalidArgument));
		}

		return MakeValue(MakeUnique<FTestCaptureSource>(InName, InCreationParamsValue[CreationParam].Get<FString>()));
	}

	virtual void CreateCaptureSourceDescriptorImpl(FCaptureSourceDescriptor& OutDescriptor) const override
	{
		OutDescriptor.SetDiscoverable(bIsDiscoverable);

		OutDescriptor.EmplaceCreationParam(CreationParam, FPropertyDesc::EType::String);
	}

	bool bIsDiscoverable;
};

TUniquePtr<FTestCaptureSourceFactory> CaptureSourceFactory;

END_DEFINE_SPEC(FCaptureSourceFactoryTest)

const FString FCaptureSourceFactoryTest::FTestCaptureSourceFactory::Id = TEXT("TestCaptureSourceFactoryId");
const FString FCaptureSourceFactoryTest::FTestCaptureSourceFactory::CreationParam = TEXT("TestCreationParam");

void FCaptureSourceFactoryTest::Define()
{
	CaptureSourceFactory = MakeUnique<FTestCaptureSourceFactory>();

	Describe("Discover()", [this]()
	{
		It("runs discovery without problems as factory is discoverable", [this]()
		{
			CaptureSourceFactory->SetDiscoverable(true);

			TSharedPtr<bool> CbResult = MakeShared<bool>(false);

			FCaptureSourceFactory::FDiscoverCallback Cb =
				FCaptureSourceFactory::FDiscoverCallback::CreateLambda(
					[this, CbResult](TArray<FCaptureSourceFactory::FCaptureSourceResult> InResults)
			{
				check(InResults.Num() == 1);
				const FCaptureSourceFactory::FCaptureSourceResult& Result = InResults[0];

				if (!TestTrue("Capture source should be discovered (no error)", Result.IsValid()))
				{
					*CbResult = false;
				}

				*CbResult = true;
			});

			FCaptureVoidResult Result = CaptureSourceFactory->Discover(MoveTemp(Cb));

			UTEST_TRUE("Starting discovery should pass", Result.HasValue());
			UTEST_TRUE("Result from callback should be true", *CbResult);

			return true;
		});

		It("returns an error as factory isn't discoverable", [this]()
		{
			CaptureSourceFactory->SetDiscoverable(false);
			FCaptureVoidResult Result = CaptureSourceFactory->Discover(FCaptureSourceFactory::FDiscoverCallback());

			UTEST_TRUE("Result should contain an error", Result.HasError());
			UTEST_EQUAL("Resulting error should be 'Not Supported'", Result.GetError().GetErrorCode(), ECaptureSourceError::NotSupported);

			return true;
		});	
	});

	Describe("CreateCaptureSource()", [this]()
	{
		It("returns a valid capture source", [this]()
		{
			FString Name = TEXT("CaptureSourceName");
			FString CreationParamValue = TEXT("Value");

			TMap<FString, FPropertyValue> CreationParamsValue;
			CreationParamsValue.Add(FTestCaptureSourceFactory::CreationParam, MakePropertyValue<FString>(CreationParamValue));
			
			FCaptureSourceFactory::FCaptureSourceResult Result = CaptureSourceFactory->CreateCaptureSource(Name, MoveTemp(CreationParamsValue));

			UTEST_TRUE("Result should be valid", Result.HasValue());

			TUniquePtr<FCaptureSource> CaptureSource = Result.StealValue();

			UTEST_TRUE("Result should contain a valid capture source", CaptureSource.IsValid());

			FTestCaptureSource* TestCaptureSource = static_cast<FTestCaptureSource*>(CaptureSource.Get());
			UTEST_EQUAL("Capture source should contain valid creation param", TestCaptureSource->ParamValue, CreationParamValue);

			return true;
		});

		It("returns an error because parameters are missing", [this]()
		{
			FString Name = TEXT("CaptureSourceName");

			TMap<FString, FPropertyValue> CreationParamsValue; // Empty

			FCaptureSourceFactory::FCaptureSourceResult Result = CaptureSourceFactory->CreateCaptureSource(Name, MoveTemp(CreationParamsValue));

			UTEST_TRUE("Result should contain an error", Result.HasError());

			return true;
		});
	});

	Describe("CreateCaptureSourceDescriptor()", [this]()
	{
		It("returns a valid descriptor containing one creation param", [this]()
		{
			FCaptureSourceDescriptor Descriptor = CaptureSourceFactory->CreateCaptureSourceDescriptor();

			const TArray<FPropertyDesc>& CreationParams = Descriptor.GetCreationParams();

			UTEST_EQUAL("Creation params should contain 1 param", CreationParams.Num(), 1);

			UTEST_EQUAL("Creation param name should match", CreationParams[0].Name, FTestCaptureSourceFactory::CreationParam);
			UTEST_EQUAL("Creation param type should match", CreationParams[0].Type, FPropertyDesc::EType::String);

			return true;
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS