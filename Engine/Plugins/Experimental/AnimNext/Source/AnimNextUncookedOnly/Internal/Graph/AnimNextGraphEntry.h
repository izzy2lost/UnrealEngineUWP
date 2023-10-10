// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextGraphEntry.generated.h"

class UAnimNextGraph_EditorData;
enum class ERigVMGraphNotifType : uint8;
class URigVMGraph;

namespace UE::AnimNext::Editor
{
	struct FUtils;
}

/** A single entry in an AnimNext graph asset */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextGraphEntry : public UObject
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::Editor::FUtils;
	
	void Initialize(UAnimNextGraph_EditorData* InEditorData);

	void HandleRigVMGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	
	// Get the name to be displayed in the UI for this entry
	virtual FText GetDisplayName() const;

	// Get the tooltip to be displayed for the name in the UI for this entry
	virtual FText GetDisplayNameTooltip() const;

	// UObject interface
	virtual bool IsAsset() const override;

protected:
	void BroadcastModified();

	/** The name of the graph */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName GraphName;

	/** Graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> Graph;
};