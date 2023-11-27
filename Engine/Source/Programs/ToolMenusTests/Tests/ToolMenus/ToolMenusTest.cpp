// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolMenus.h"

#include "TestHarness.h"

UToolMenus* CreateUniqueUToolMenusInstance()
{
	// Create a unique ToolMenus instance for use in a single test.
	UToolMenus* ToolMenus = NewObject<UToolMenus>();
	ToolMenus->AddToRoot();
	return ToolMenus;
}

TEST_CASE("Developer::ToolMenus::Can create menu", "[ToolMenus]")
{
	UToolMenus* ToolMenus = CreateUniqueUToolMenusInstance();

	UToolMenu* ToolMenu = ToolMenus->RegisterMenu("MyMenu");

	CHECK(ToolMenu);
}

TEST_CASE("Developer::ToolMenus::Non-registered menu is not registered", "[ToolMenus]")
{
	UToolMenus* ToolMenus = CreateUniqueUToolMenusInstance();

	CHECK_FALSE(ToolMenus->IsMenuRegistered("MyMenu"));
}

TEST_CASE("Developer::ToolMenus::Removed menu is not registered", "[ToolMenus]")
{
	UToolMenus* ToolMenus = CreateUniqueUToolMenusInstance();

	ToolMenus->RegisterMenu("MyMenu");

	CHECK(ToolMenus->IsMenuRegistered("MyMenu"));

	ToolMenus->RemoveMenu("MyMenu");

	CHECK_FALSE(ToolMenus->IsMenuRegistered("MyMenu"));
}

TEST_CASE("Developer::ToolMenus::GenerateWidget calls dynamic section lambdas", "[ToolMenus]")
{
	UToolMenus* ToolMenus = CreateUniqueUToolMenusInstance();

	UToolMenu* ToolMenu = ToolMenus->RegisterMenu("MyMenu");

	REQUIRE(ToolMenu);

	bool bWasLambdaCalled = false;
	ToolMenu->AddDynamicSection("MySection", FNewToolMenuDelegateLegacy::CreateLambda([&bWasLambdaCalled](FMenuBuilder&, UToolMenu*) {
		bWasLambdaCalled = true;
	}));

	ToolMenus->GenerateWidget("MyMenu", FToolMenuContext());

	CHECK(bWasLambdaCalled);
}