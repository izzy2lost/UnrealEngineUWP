// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include <catch2/catch_test_macros.hpp>

#include "Online/OnlineError.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/ErrorsOSSAdapter.h"
#include "OnlineError.h"

#define ADAPTER_ERROR_TAG "[AdapterErrors]"
#define ADAPTER_ERROR_TEST_CASE(x, ...) TEST_CASE(x, ADAPTER_ERROR_TAG __VA_ARGS__)
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.testing"
using FOnlineErrorOsvc = UE::Online::FOnlineError;

ADAPTER_ERROR_TEST_CASE("Basic adapter error compilation test")
{
	FOnlineErrorOss TestErrorOss_Success = ONLINE_ERROR(EOnlineErrorResult::Success, TEXT("success"));
	FOnlineErrorOss TestErrorOss_InvalidParams = ONLINE_ERROR(EOnlineErrorResult::InvalidParams, TEXT("invalid_params"));
	FOnlineErrorOss TestErrorOss_Cancelled = ONLINE_ERROR(EOnlineErrorResult::Canceled, TEXT("custom_text_abc"));

	using namespace UE::Online;
	using namespace UE::Online::Errors;

	FOnlineErrorOsvc TestError_Success = Errors::FromOssError(TestErrorOss_Success);
	FOnlineErrorOsvc TestError_InvalidParams = Errors::FromOssError(TestErrorOss_InvalidParams);
	FOnlineErrorOsvc TestError_Cancelled = Errors::FromOssError(TestErrorOss_Cancelled);

	CHECK(TestError_Success == Errors::Success());
	CHECK(TestError_Success == TestErrorOss_Success);
	CHECK(TestError_Success != TestErrorOss_Cancelled);
	CHECK(TestError_InvalidParams == Errors::InvalidParams());
	CHECK(TestError_InvalidParams == TestErrorOss_InvalidParams);
	CHECK(TestError_InvalidParams != TestError_Success);
	CHECK(TestError_Cancelled == Errors::Cancelled());
	CHECK(TestError_Cancelled == TestErrorOss_Cancelled);
	CHECK(TestError_Cancelled != TestErrorOss_InvalidParams);
	CHECK(TestError_Cancelled.GetLogString().Contains(TEXT("custom_text_abc")));
}

ADAPTER_ERROR_TEST_CASE("Check platform adapter is working properly")
{

	FOnlineErrorOss TestErrorOss_InvalidParams = ONLINE_ERROR(EOnlineErrorResult::InvalidParams, TEXT("WINDOWS_abcdef"));
	FOnlineErrorOss TestErrorOss_Cancelled = ONLINE_ERROR(EOnlineErrorResult::Canceled, TEXT("abcdef"));

	using namespace UE::Online;
	using namespace UE::Online::Errors;

	// this should obviously be done in a static initializer in final code, just doing it like this for quick tests
	TOssPlatformErrorHandler ErrorHandler = [&](const FOnlineErrorOsvc& Error, const FOnlineErrorOss& Error2)
	{
		if (Error2.GetErrorCode().Contains(TEXT("WINDOWS")))
		{
			return TOptional<FOnlineErrorOsvc>(Errors::MissingInterface(Error));
		}
		return TOptional<FOnlineErrorOsvc>();
	};
	AddOssPlatformErrorHandler(MoveTemp(ErrorHandler));

	FOnlineErrorOsvc TestError_InvalidParams = Errors::FromOssError(TestErrorOss_InvalidParams);
	FOnlineErrorOsvc TestError_Cancelled = Errors::FromOssError(TestErrorOss_Cancelled);

	CHECK(TestError_InvalidParams == Errors::MissingInterface());
	CHECK(TestError_Cancelled != Errors::MissingInterface());

	// If we make more than one test, we will need to empty out the platform functions array somehow, or else it will crash on a dangling delegate
	//Private::PlatformFunctions.Empty();
}

#undef ONLINE_ERROR_NAMESPACE