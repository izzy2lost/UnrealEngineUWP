// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineCatchHelper.h"

#define COMMERCE_TAG "[suite_commerce]"

#define COMMERCE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, COMMERCE_TAG __VA_ARGS__)

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