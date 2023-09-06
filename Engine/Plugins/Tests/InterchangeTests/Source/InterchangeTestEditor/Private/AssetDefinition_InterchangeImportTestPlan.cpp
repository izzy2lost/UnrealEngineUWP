// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_InterchangeImportTestPlan.h"

#include "InterchangeImportTestStepImport.h"
#include "InterchangePipelineBase.h"
#include "InterchangeTestFunction.h"
#include "ImportTestFunctions/MaterialImportTestFunctions.h"

#include "Algo/AnyOf.h"
#include "Algo/AllOf.h"
#include "ContentBrowserMenuContexts.h"
#include "Hal/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectIterator.h"

TConstArrayView<FAssetCategoryPath> UAssetDefinition_InterchangeImportTestPlan::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(NSLOCTEXT("AssetDefinition_InterchangeImportTestPlan_Category", "Name", "Interchange Import Test Plan")) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_InterchangeImportTestPlan::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UInterchangeImportTestPlan* TestPlan : OpenArgs.LoadObjects<UInterchangeImportTestPlan>())
	{
		FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, TestPlan);
	}

	return EAssetCommandResult::Handled;
}
