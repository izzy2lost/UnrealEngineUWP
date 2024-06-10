// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include <catch2/catch_test_macros.hpp>

#include "OnlineCatchHelper.h"
#include "Helpers/Auth/AuthLogin.h"
#include "Helpers/Auth/AuthLogout.h"
#include "Helpers/Commerce/CommerceQueryOffersHelper.h"
#include "Helpers/Commerce/CommerceQueryOffersByIdHelper.h"
#include "Helpers/Commerce/CommerceGetOffersHelper.h"
#include "Helpers/Commerce/CommerceGetOffersByIdHelper.h"
#include "Helpers/Commerce/CommerceCheckoutHelper.h"
#include "Helpers/Commerce/CommerceQueryEntitlementsHelper.h"
#include "Helpers/Commerce/CommerceGetEntitlementsHelper.h"
#include "Online/CommerceCommon.h"


#define COMMERCE_TAG "[Commerce]"
#define COMMERCE_QUERYOFFERS_TAG COMMERCE_TAG "[queryoffers]"
#define COMMERCE_QUERYOFFERSBYID_TAG COMMERCE_TAG "[queryoffersbyid]"
#define COMMERCE_GETOFFERS_TAG COMMERCE_TAG "[getoffers]"
#define COMMERCE_GETOFFERSBYID_TAG COMMERCE_TAG "[getoffersbyid]"
#define COMMERCE_DISABLED_TAG COMMERCE_TAG "[commercedisabled]"
#define COMMERCE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, COMMERCE_TAG __VA_ARGS__)


COMMERCE_TEST_CASE("Basic compile test")
{
	TSharedPtr<ICommerce> Commerce;
	CHECK(!Commerce.IsValid());
}

// QueryOffers tests
COMMERCE_TEST_CASE("Verify that QueryOffers returns a fail message if the local user is not logged in", COMMERCE_QUERYOFFERS_TAG)
{
	FAccountId AccountId;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;
	QueryOffersHelperParams.ExpectedError = TOnlineResult<FCommerceQueryOffers>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffers returns a fail message of the given local user ID does not match the actual local user ID", COMMERCE_QUERYOFFERS_TAG)
{
	FAccountId AccountId;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;
	QueryOffersHelperParams.ExpectedError = TOnlineResult<FCommerceQueryOffers>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffers caches an empty list if there are no offers", COMMERCE_QUERYOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace0Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffers caches a list of one offer if there is only one existing offer", COMMERCE_QUERYOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 1;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace1Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffers caches the list of all offers if there are multiple existing offers", COMMERCE_QUERYOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 4;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

// QueryOffersById tests
COMMERCE_TEST_CASE("Verify that QueryOffersById returns a fail message if the local user is not logged in", COMMERCE_QUERYOFFERSBYID_TAG)
{
	FAccountId AccountId;

	FCommerceQueryOffersById::Params OpQueryOffersByIdParams;
	FCommerceQueryOffersByIdHelper::FHelperParams QueryOffersByIdHelperParams;
	QueryOffersByIdHelperParams.OpParams = &OpQueryOffersByIdParams;
	QueryOffersByIdHelperParams.ExpectedError = TOnlineResult<FCommerceQueryOffersById>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceQueryOffersByIdHelper>(MoveTemp(QueryOffersByIdHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns a fail message of the given local user ID does not match the actual local user ID", COMMERCE_QUERYOFFERSBYID_TAG)
{
	FAccountId AccountId;

	FCommerceQueryOffersById::Params OpQueryOffersByIdParams;
	FCommerceQueryOffersByIdHelper::FHelperParams QueryOffersByIdHelperParams;
	QueryOffersByIdHelperParams.OpParams = &OpQueryOffersByIdParams;
	QueryOffersByIdHelperParams.ExpectedError = TOnlineResult<FCommerceQueryOffersById>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersByIdHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersByIdHelper>(MoveTemp(QueryOffersByIdHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns an empty list when given an empty list of IDs and there are no existing offers", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns an empty list when given an empty list of IDs and there are multiple existing offers", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns a fail message when given a populated list of IDs and there are no existing offers", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns a fail message when given a populated list of IDs and none of them match existing offers", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns a fail message when given a populated list of IDs where one ID exists and another does not", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns the correct list of one offer when given the ID for one existing offer", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

COMMERCE_TEST_CASE("Verify that QueryOffersById returns the correct list of offers when given a populated list of multiple existing IDs", COMMERCE_DISABLED_TAG)
{
	// EOS QueryOffers interface doesn't support anything ID-specific, skipping the test
}

// GetOffers
COMMERCE_TEST_CASE("Verify that GetOffers returns a fail message if the local user is not logged in", COMMERCE_GETOFFERS_TAG)
{
	FAccountId AccountId;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;
	GetOffersHelperParams.ExpectedError = TOnlineResult<FCommerceGetOffers>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffers returns a fail message of the given local user ID does not match the actual local user ID", COMMERCE_GETOFFERS_TAG)
{
	FAccountId AccountId;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;
	GetOffersHelperParams.ExpectedError = TOnlineResult<FCommerceGetOffers>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetOffersHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffers returns an empty list if there are no cached offers", COMMERCE_GETOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace0Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffers returns a correct list of one offer if there is only one cached offer", COMMERCE_GETOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 1;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace1Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffers returns the correct list if there are cached offers", COMMERCE_GETOFFERS_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 4;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffers::Params OpGetOffersParams;
	FCommerceGetOffersHelper::FHelperParams GetOffersHelperParams;
	GetOffersHelperParams.OpParams = &OpGetOffersParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersHelper>(MoveTemp(GetOffersHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

// GetOffersById
COMMERCE_TEST_CASE("Verify that GetOffersById returns a fail message if the local user is not logged in", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;
	GetOffersByIdHelperParams.ExpectedError = TOnlineResult<FCommerceGetOffersById>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns a fail message of the given local user ID does not match the actual local user ID", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;
	GetOffersByIdHelperParams.ExpectedError = TOnlineResult<FCommerceGetOffersById>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetOffersByIdHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns an empty list when given an empty list of IDs and there are no offers cached", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace0Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->OfferIds = {};

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

}

COMMERCE_TEST_CASE("Verify that GetOffersById returns an empty list when given an empty list of IDs and there are multiple cached offers", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->OfferIds = {};

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns a fail message when given a populated list of IDs and there are no cached offers", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;
	GetOffersByIdHelperParams.ExpectedError = TOnlineResult<FCommerceGetOffersById>(Errors::NotFound());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	FString CatalogId;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferNamespace0Items"), CatalogId, GEngineIni);
	QueryOffersHelperParams.OpParams->OverrideCatalogNamespace = CatalogId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId1NotExisting;
	FOfferId OfferId2NotExisting;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId1NotExisting"), OfferId1NotExisting, GEngineIni);
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId2NotExisting"), OfferId2NotExisting, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = { OfferId1NotExisting, OfferId2NotExisting };

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns an empty list when given a populated list of IDs and none of them match any cached offers", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 0;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId1NotExisting;
	FOfferId OfferId2NotExisting;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId1NotExisting"), OfferId1NotExisting, GEngineIni);
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId2NotExisting"), OfferId2NotExisting, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = { OfferId1NotExisting, OfferId2NotExisting };

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns correct list of one offer when given a populated list of IDs where one ID is cached and another is not", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 1;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId1NotExisting;
	FOfferId OfferId2;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId1NotExisting"), OfferId1NotExisting, GEngineIni);
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId2"), OfferId2, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = { OfferId1NotExisting, OfferId2 };

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns the correct list of one offer when given the ID for one cached offer", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 1;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId1;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId1"), OfferId1, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = {OfferId1};

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetOffersById returns the correct list of offers when given a populated list of multiple cached IDs", COMMERCE_GETOFFERSBYID_TAG)
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 2;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;
	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId1;
	FOfferId OfferId2;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId1"), OfferId1, GEngineIni);
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId2"), OfferId2, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = { OfferId1, OfferId2 };

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum);

	RunToCompletion(bLogout);
}

// ShowStoreUI tests
COMMERCE_TEST_CASE("Verify that ShowStoreUI returns a fail message if the local user is not logged in", COMMERCE_DISABLED_TAG)
{
	// TODO
	// The ShowStoreUI EOS interface is not implemented
}

COMMERCE_TEST_CASE("Verify that ShowStoreUI returns a fail message of the given local user ID does not match the actual local user ID", COMMERCE_DISABLED_TAG)
{
	// TODO
	// The ShowStoreUI EOS interface is not implemented
}

COMMERCE_TEST_CASE("Verify that ShowStoreUI displays platform store UI", COMMERCE_DISABLED_TAG) // May not be possible to automate currently
{
	// TODO
	// The ShowStoreUI EOS interface is not implemented
}

// Checkout tests
COMMERCE_TEST_CASE("Verify that Checkout returns a fail message if the local user is not logged in")
{
	FAccountId AccountId;

	FCommerceCheckout::Params OpCheckoutParams;
	FCommerceCheckoutHelper::FHelperParams CheckoutHelperParams;
	CheckoutHelperParams.OpParams = &OpCheckoutParams;
	CheckoutHelperParams.ExpectedError = TOnlineResult<FCommerceCheckout>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	CheckoutHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceCheckoutHelper>(MoveTemp(CheckoutHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that Checkout returns a fail message of the given local user ID does not match the actual local user ID")
{
	FAccountId AccountId;

	FCommerceCheckout::Params OpCheckoutParams;
	FCommerceCheckoutHelper::FHelperParams CheckoutHelperParams;
	CheckoutHelperParams.OpParams = &OpCheckoutParams;
	CheckoutHelperParams.ExpectedError = TOnlineResult<FCommerceCheckout>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	CheckoutHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceCheckoutHelper>(MoveTemp(CheckoutHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that Checkout initiates the checkout process when given an Offers array with one offer")
{
	/*
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedOffersNum = 1;

	FCommerceQueryOffers::Params OpQueryOffersParams;
	FCommerceQueryOffersHelper::FHelperParams QueryOffersHelperParams;
	QueryOffersHelperParams.OpParams = &OpQueryOffersParams;

	FCommerceGetOffersById::Params OpGetOffersByIdParams;
	FCommerceGetOffersByIdHelper::FHelperParams GetOffersByIdHelperParams;
	GetOffersByIdHelperParams.OpParams = &OpGetOffersByIdParams;

	FCommerceCheckout::Params OpCheckoutParams;
	FCommerceCheckoutHelper::FHelperParams CheckoutHelperParams;
	CheckoutHelperParams.OpParams = &OpCheckoutParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);

	QueryOffersHelperParams.OpParams->LocalAccountId = AccountId;

	GetOffersByIdHelperParams.OpParams->LocalAccountId = AccountId;
	FOfferId OfferId2;
	GConfig->GetString(TEXT("EOSSDK.Platform.OnlineTests"), TEXT("OfferId2"), OfferId2, GEngineIni);
	GetOffersByIdHelperParams.OpParams->OfferIds = { OfferId2 };

	FPurchaseOffer Purchase = { OfferId2, 1};
	CheckoutHelperParams.OpParams->LocalAccountId = AccountId;
	CheckoutHelperParams.OpParams->Offers = { Purchase };

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryOffersHelper>(MoveTemp(QueryOffersHelperParams))
		.EmplaceStep<FCommerceGetOffersByIdHelper>(MoveTemp(GetOffersByIdHelperParams), ExpectedOffersNum)
		.EmplaceStep<FCommerceCheckoutHelper>(MoveTemp(CheckoutHelperParams));

	RunToCompletion(bLogout);
	*/
}

COMMERCE_TEST_CASE("Verify that Checkout initiates the checkout process when given an Offers array with multiple offers")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that Checkout does not initiate the checkout process when given an empty Offers array")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that Checkout returns the correct TransactionId after a completed purchase")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that Checkout does not return a TransactionId if the purchase is incomplete")
{
	// TODO
}

// QueryTransactionEntitlements tests
COMMERCE_TEST_CASE("Verify that QueryTransactionEntitlements returns a fail message if the local user is not logged in")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that QueryTransactionEntitlements returns a fail message of the given local user ID does not match the actual local user ID")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that QueryTransactionEntitlements returns the correct list of entitlements when given an existing TransactionId and caches them")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that QueryTransactionEntitlements returns a fail message when given a nonexisting TransactionId")
{
	// TODO
}

// QueryEntitlements tests
COMMERCE_TEST_CASE("Verify that QueryEntitlements returns a fail message if the local user is not logged in")
{
	FAccountId AccountId;

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FCommerceQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceQueryEntitlements>(Errors::NotLoggedIn());
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryEntitlements returns a fail message of the given local user ID does not match the actual local user ID")
{
	FAccountId AccountId;

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FCommerceQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;
	QueryEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceQueryEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that QueryEntitlements caches all entitlements")
{
	FAccountId AccountId;
	TOptional<uint32_t> ExpectedEntitlementsNum = 0;

	FCommerceQueryEntitlements::Params OpQueryEntitlementsParams;
	FCommerceQueryEntitlementsHelper::FHelperParams QueryEntitlementsHelperParams;
	QueryEntitlementsHelperParams.OpParams = &OpQueryEntitlementsParams;

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FCommerceGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	QueryEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;
	QueryEntitlementsHelperParams.OpParams->bIncludeRedeemed = true;
	GetEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceQueryEntitlementsHelper>(MoveTemp(QueryEntitlementsHelperParams))
		.EmplaceStep<FCommerceGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams), ExpectedEntitlementsNum);

	RunToCompletion(bLogout);
}

// GetEntitlements tests
COMMERCE_TEST_CASE("Verify that GetEntitlements returns a fail message if the local user is not logged in")
{
	FAccountId AccountId;

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FCommerceGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;
	GetEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceGetEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetEntitlementsHelperParams.OpParams->LocalAccountId = AccountId;

	bool bLogout = false;

	LoginPipeline
		.EmplaceStep<FAuthLogoutStep>(FPlatformMisc::GetPlatformUserForUserIndex(0))
		.EmplaceStep<FCommerceGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams));

	RunToCompletion(bLogout);
}

COMMERCE_TEST_CASE("Verify that GetEntitlements returns a fail message of the given local user ID does not match the actual local user ID")
{
	FAccountId AccountId;

	FCommerceGetEntitlements::Params OpGetEntitlementsParams;
	FCommerceGetEntitlementsHelper::FHelperParams GetEntitlementsHelperParams;
	GetEntitlementsHelperParams.OpParams = &OpGetEntitlementsParams;
	GetEntitlementsHelperParams.ExpectedError = TOnlineResult<FCommerceGetEntitlements>(Errors::NotLoggedIn());

	FTestPipeline& LoginPipeline = GetLoginPipeline(AccountId);
	GetEntitlementsHelperParams.OpParams->LocalAccountId = FAccountId();

	bool bLogout = true;

	LoginPipeline
		.EmplaceStep<FCommerceGetEntitlementsHelper>(MoveTemp(GetEntitlementsHelperParams));

	RunToCompletion(bLogout);
}


COMMERCE_TEST_CASE("Verify that GetEntitlements returns a list of all cached entitlements")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that GetEntitlements returns an empty list when no entitlements are cached")
{
	// TODO
}

// RedeemEntitlement tests
COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns a fail message if the local user is not logged in")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns a fail message of the given local user ID does not match the actual local user ID")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement redeems no entitlements when given an existing EntitlementId and Quantity 0")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement redeems 1 of the correct entitlements when given an existing EntitlementId and Quantity 1")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement redeems 2 of the correct entitlements when given an existing EntitlementId and Quantity 2")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement redeems all of the given entitlements when given a Quantity equal to the available number for that entitlement")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns a fail message when given a Quantity that is 1 more than the available number for the given entitlemenet")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns a fail message when given a Quantity that is 2 more than the available number for the given entitlemenet")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns a fail message when given a nonexisting EntitlementId")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RedeemEntitlement returns all of the given entitlements when given a Quantity matching the number of entitlements available")
{
	// TODO
}

// RetrieveS2SToken tests
COMMERCE_TEST_CASE("Verify that RetrieveS2SToken returns a fail message if the local user is not logged in")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RetrieveS2SToken returns a fail message of the given local user ID does not match the actual local user ID")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RetrieveS2SToken successfully returns the correct Token for the given TokenType")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that RetrieveS2SToken returns a fail message if the given TokenType does not exist")
{
	// TODO
}

// OnPurchaseCompleted tests
COMMERCE_TEST_CASE("Verify that OnPurchaseCompleted returns a fail message if the local user is not logged in")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that OnPurchaseCompleted returns a fail message of the given local user ID does not match the actual local user ID")
{
	// TODO
}

COMMERCE_TEST_CASE("Verify that OnPurchaseCompleted completes without an error")
{
	// TODO
}
