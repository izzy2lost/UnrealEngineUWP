// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTree.h"
#include "StateTreeViewModel.h"
#include "EditorSubsystem.h"

#include "StateTreeEditingSubsystem.generated.h"

class SWidget;
class FStateTreeViewModel;
class FUICommandList;
struct FStateTreeCompilerLog;

UCLASS()
class STATETREEEDITORMODULE_API UStateTreeEditingSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	UStateTreeEditingSubsystem() {}
	
	TSharedRef<FStateTreeViewModel> FindOrAddViewModel(const TNonNullPtr<UStateTree> InStateTree);
	
	static bool CompileStateTree(const TNonNullPtr<UStateTree>InStateTree,  FStateTreeCompilerLog& InOutLog);
	
	static TSharedRef<SWidget> GetStateTreeView(TSharedRef<FStateTreeViewModel> InViewModel, const TSharedRef<FUICommandList>& TreeViewCommandList);
	
	// Validates asset state
	static void ValidateStateTree(const TNonNullPtr<UStateTree> InStateTree);

	// Calculates editor data hash of the asset.
	static uint32 CalculateStateTreeHash(const TNonNullPtr<const UStateTree> InStateTree);
	
protected:
	TMap<TSoftObjectPtr<UStateTree>, TSharedPtr<FStateTreeViewModel>> StateTreeViewModels;
};
