// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScriptableToolSet.generated.h"

class UScriptableInteractiveTool;
class UBaseScriptableToolBuilder;
struct FScriptableToolGroupSet;
class UClass;

DECLARE_DELEGATE(FPreToolsLoadedDelegate);
DECLARE_DELEGATE(FToolsLoadedDelegate);
DECLARE_DELEGATE_OneParam(FToolsLoadingUpdateDelegate, TSharedRef<struct FStreamableHandle>);
/**
 * UScriptableToolSet represents a set of UScriptableInteractiveTool types.
 */
UCLASS()
class EDITORSCRIPTABLETOOLSFRAMEWORK_API UScriptableToolSet : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Find all UScriptableInteractiveTool classes in the current project.
	 * (Currently no support for filtering/etc)
	 */
	void ReinitializeScriptableTools(FPreToolsLoadedDelegate PreDelegate, FToolsLoadedDelegate PostDelegate, FToolsLoadingUpdateDelegate UpdateDelegate, FScriptableToolGroupSet* TagsToFilter = nullptr);

	/**
	 * Allow external code to process each UScriptableInteractiveTool in the current ToolSet
	 */
	void ForEachScriptableTool(
		TFunctionRef<void(UClass* ToolClass, UBaseScriptableToolBuilder* ToolBuilder)> ProcessToolFunc);

protected:

	void PostToolLoad(FToolsLoadedDelegate Delegate, TArray< FSoftObjectPath > ObjectsLoaded, TSharedPtr<FScriptableToolGroupSet> TagsToFilter);

	bool bActiveLoading = false;
	TSharedPtr<FStreamableHandle> AsyncLoadHandle;

	struct FScriptableToolInfo
	{
		TWeakObjectPtr<UClass> ToolClass = nullptr;
		TWeakObjectPtr<UScriptableInteractiveTool> ToolCDO;
		TWeakObjectPtr<UBaseScriptableToolBuilder> ToolBuilder;
	};
	TArray<FScriptableToolInfo> Tools;

	UPROPERTY()
	TArray<TObjectPtr<UBaseScriptableToolBuilder>> ToolBuilders;
};
