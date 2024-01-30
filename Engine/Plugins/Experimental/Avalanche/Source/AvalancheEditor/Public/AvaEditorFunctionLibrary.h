// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/WeakObjectPtrFwd.h"

class UAvalancheBlueprint;

class FAvaEditorFunctionLibrary
{
public:
	/**
	 * Creates World Assets based on the Avalanche Blueprints provided
	 * @param InWeakBlueprints the Avalanche Blueprints to export
	 */
	AVALANCHEEDITOR_API static void ExportAvaBlueprintsToWorld(TArray<TWeakObjectPtr<UAvalancheBlueprint>> InWeakBlueprints);
};
