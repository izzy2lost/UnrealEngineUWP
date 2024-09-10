// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionEditorSettings.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeEditorData.h"

UAvaTransitionEditorSettings::UAvaTransitionEditorSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName  = TEXT("Transition Logic");
}

UAvaTransitionTreeEditorData* UAvaTransitionEditorSettings::LoadDefaultTemplateEditorData() const
{
	if (UAvaTransitionTree* TemplateTree = DefaultTemplate.LoadSynchronous())
	{
		return Cast<UAvaTransitionTreeEditorData>(TemplateTree->EditorData);
	}
	return nullptr;
}
