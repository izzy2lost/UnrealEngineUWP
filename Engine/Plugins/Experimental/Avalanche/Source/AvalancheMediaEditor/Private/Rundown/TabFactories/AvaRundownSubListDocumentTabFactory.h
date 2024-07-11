// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "AvaMediaDefines.h"

class FAvaRundownEditor;
class UAvaRundown;

/** Base class for all Tab Factories in Ava SubListDocument Editor */
class FAvaRundownSubListDocumentTabFactory : public FDocumentTabFactory
{
public:
	static const FName FactoryId;
	static const FString BaseTabName;

	static FName GetTabId(const FAvaRundownPageListReference& InSubListReference);
	static FText GetTabLabel(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown);
	static FText GetTabDescription(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown);
	static FText GetTabTooltip(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown);

	FAvaRundownSubListDocumentTabFactory(const TSharedPtr<FAvaRundownEditor>& InSubListDocumentEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;

	TSharedRef<SDockTab> SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, const FAvaRundownPageListReference& InSubListReference);

protected:
	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;
	FAvaRundownPageListReference SubListReference;
	FText SubListDisplayName;
};

