// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include <catch2/catch_test_macros.hpp>

#include "Online/OnlineError.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineErrorEOSGS.h"

#define LOCTEXT_NAMESPACE "OnlineErrorsTest"
#define EOS_ERROR_TAG "[.EOSErrors]"
#define EOS_ERROR_TEST_CASE(x, ...) TEST_CASE(x, EOS_ERROR_TAG __VA_ARGS__)

// Dummy error category for testing mapping extensions.
namespace UE::Online::Errors
{
	UE_ONLINE_ERROR_CATEGORY(ExtensionTest, Engine, 0x999, "ExtensionTest")
	UE_ONLINE_ERROR(ExtensionTest, WrongAccount, 1, TEXT("WrongAccount"), LOCTEXT("WrongAccount", ""))
/* UE::Online::Errors */ }

UE::Online::FOnlineError MapExtensionEOSError(UE::Online::FOnlineError&& Error, EOS_EResult Result)
{
	switch (Result)
	{
	case EOS_EResult::EOS_Auth_WrongAccount:	return UE::Online::Errors::ExtensionTest::WrongAccount(MoveTemp(Error));
	default:									return UE::Online::Errors::MapCommonEOSError(MoveTemp(Error), Result);
	}
}

EOS_ERROR_TEST_CASE("Basic EOS error compilation test")
{
	using namespace UE::Online;
	using namespace UE::Online::Errors;

	FOnlineError TestError_Success = Errors::FromEOSResult(EOS_EResult::EOS_Success);
	FOnlineError TestError_InvalidParams = Errors::FromEOSResult(EOS_EResult::EOS_InvalidParameters);
	FOnlineError TestError_Canceled = Errors::FromEOSResult(EOS_EResult::EOS_Canceled);
	FOnlineError TestError_Big = Errors::FromEOSResult(EOS_EResult::EOS_PlayerDataStorage_DataLengthInvalid);
	FOnlineError TestError_ExtensionKnown = Errors::FromEOSResult(EOS_EResult::EOS_Auth_WrongAccount, &MapExtensionEOSError);
	FOnlineError TestError_ExtensionCommon = Errors::FromEOSResult(EOS_EResult::EOS_Success, &MapExtensionEOSError);

	CHECK(TestError_Success == Errors::Success());
	CHECK(TestError_Success == EOS_EResult::EOS_Success);
	CHECK(TestError_Success != EOS_EResult::EOS_InvalidParameters);
	CHECK(TestError_InvalidParams == Errors::InvalidParams());
	CHECK(TestError_InvalidParams == EOS_EResult::EOS_InvalidParameters);
	CHECK(TestError_InvalidParams != EOS_EResult::EOS_Canceled);
	CHECK(TestError_Canceled == Errors::Cancelled());
	CHECK(TestError_Canceled == EOS_EResult::EOS_Canceled);
	CHECK(TestError_Canceled != EOS_EResult::EOS_Success);
	CHECK(TestError_Big == Errors::Unknown());
	CHECK(TestError_Big == EOS_EResult::EOS_PlayerDataStorage_DataLengthInvalid);
	CHECK(TestError_ExtensionKnown == Errors::ExtensionTest::WrongAccount());
	CHECK(TestError_ExtensionKnown == EOS_EResult::EOS_Auth_WrongAccount);
	CHECK(TestError_ExtensionCommon == Errors::Success());
	CHECK(TestError_ExtensionCommon == EOS_EResult::EOS_Success);
}

#undef LOCTEXT_NAMESPACE