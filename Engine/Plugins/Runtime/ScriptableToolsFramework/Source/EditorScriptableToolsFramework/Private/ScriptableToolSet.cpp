// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptableToolSet.h"

#include "ScriptableInteractiveTool.h"
#include "ScriptableToolBuilder.h"
#include "Tags/ScriptableToolGroupSet.h"
#include "Engine/AssetManager.h" // Singleton access to StreamableManager


#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"


void UScriptableToolSet::ReinitializeScriptableTools(FPreToolsLoadedDelegate PreDelegate, FToolsLoadedDelegate PostDelegate, FToolsLoadingUpdateDelegate UpdateDelegate, FScriptableToolGroupSet* TagsToFilter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("UScriptableToolSet::ReinitializeScriptableTools"))

	if (bActiveLoading)
	{
		AsyncLoadHandle->CancelHandle();
	}

	PreDelegate.ExecuteIfBound();

	bActiveLoading = true;
	Tools.Reset();
	ToolBuilders.Reset();

	UClass* ScriptableToolClass = UScriptableInteractiveTool::StaticClass();
	UScriptableInteractiveTool* ScriptableToolCDO = ScriptableToolClass->GetDefaultObject<UScriptableInteractiveTool>();

	// Iterate over Blueprint classes to try to find UScriptableInteractiveTool blueprints
	// Note that this code may not be fully reliable, but it appears to work so far...

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	// The asset registry is populated asynchronously at startup, so there's no guarantee it has finished.
	// This simple approach just runs a synchronous scan on the entire content directory.
	// Better solutions would be to specify only the path to where the relevant blueprints are,
	// or to register a callback with the asset registry to be notified of when it's finished populating.
	TArray< FString > ContentPaths;
	ContentPaths.Add(TEXT("/Game"));
	AssetRegistry.ScanPathsSynchronous(ContentPaths);

	FTopLevelAssetPath BaseClassPath = ScriptableToolClass->GetClassPathName();

	// Use the asset registry to get the set of all class names deriving from Base
	TArray<FTopLevelAssetPath> BaseModeClasses;
	BaseModeClasses.Add(BaseClassPath);
	TSet<FTopLevelAssetPath> DerivedClassPaths;
	AssetRegistry.GetDerivedClassNames(BaseModeClasses, TSet<FTopLevelAssetPath>(), DerivedClassPaths);

	TArray< FSoftObjectPath > ObjectPathsToLoad;
	for (const FTopLevelAssetPath& ClassPath : DerivedClassPaths)
	{
		// don't include Base tools  (note this will also catch EditorScriptableToolsFramework)
		if (ClassPath.ToString().Contains(TEXT("ScriptableToolsFramework")))
		{
			continue;
		}

		ObjectPathsToLoad.Add(ClassPath.ToString());
	}
	
	TSharedPtr<FScriptableToolGroupSet> TagsToFilterCopy;
	if (TagsToFilter)
	{
		TagsToFilterCopy = MakeShared<FScriptableToolGroupSet>(*TagsToFilter);
	}

	AsyncLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ObjectPathsToLoad, [this, PostDelegate, ObjectPathsToLoad, TagsToFilterCopy]() { PostToolLoad(PostDelegate, ObjectPathsToLoad, TagsToFilterCopy);  });

	if (AsyncLoadHandle)
	{
		AsyncLoadHandle->BindUpdateDelegate(UpdateDelegate);
	}
	else
	{
		PostToolLoad(PostDelegate, ObjectPathsToLoad, TagsToFilterCopy);
	}
}




void UScriptableToolSet::ForEachScriptableTool(
	TFunctionRef<void(UClass* ToolClass, UBaseScriptableToolBuilder* ToolBuilder)> ProcessToolFunc)
{
	if (bActiveLoading)
	{
		return;
	}

	for (FScriptableToolInfo& ToolInfo : Tools)
	{
		if (ToolInfo.ToolClass.IsValid() && ToolInfo.ToolBuilder.IsValid())
		{
			ProcessToolFunc(ToolInfo.ToolClass.Get(), ToolInfo.ToolBuilder.Get());
		}
	}
}


void UScriptableToolSet::PostToolLoad(FToolsLoadedDelegate Delegate, TArray< FSoftObjectPath > ObjectsLoaded, TSharedPtr<FScriptableToolGroupSet> TagsToFilter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("UScriptableToolSet::PostToolLoad"))

	TSet<UClass*> PotentialToolClasses;
	UClass* ScriptableToolClass = UScriptableInteractiveTool::StaticClass();

	for(FSoftObjectPath& ObjectPath : ObjectsLoaded)
	{
		TSoftClassPtr<UScriptableInteractiveTool> SoftClass = TSoftClassPtr<UScriptableInteractiveTool>(ObjectPath);
		if (UClass* Class = SoftClass.LoadSynchronous())
		{
			if (Class->HasAnyClassFlags(CLASS_Abstract) || (Class->GetAuthoritativeClass() != Class))
			{
				continue;
			}

			if (TagsToFilter)
			{
				if (!TagsToFilter->Matches(Cast<UScriptableInteractiveTool>(Class->GetDefaultObject())->GroupTags))
				{
					continue;
				}
			}

			PotentialToolClasses.Add(Class);
		}
	}

	// if the class is viable, create a ToolBuilder for it
	for (UClass* Class : PotentialToolClasses)
	{
		if (Class->IsChildOf(ScriptableToolClass))
		{
			FScriptableToolInfo ToolInfo;
			ToolInfo.ToolClass = Class;
			ToolInfo.ToolCDO = Class->GetDefaultObject<UScriptableInteractiveTool>();

			UBaseScriptableToolBuilder* ToolBuilder = ToolInfo.ToolCDO->GetNewCustomToolBuilderInstance(this);
			if (ToolBuilder == nullptr)
			{
				ToolBuilder = NewObject<UBaseScriptableToolBuilder>(this);
			}

			ToolBuilder->ToolClass = Class;
			ToolBuilders.Add(ToolBuilder);

			ToolInfo.ToolBuilder = ToolBuilder;
			Tools.Add(ToolInfo);
		}
	}

	bActiveLoading = false;
	Delegate.ExecuteIfBound();

	AsyncLoadHandle.Reset();
}


