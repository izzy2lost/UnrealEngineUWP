// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownSubListDocumentTabFactory.h"

#include "Rundown/AvaRundownEditor.h"
#include "Rundown/Pages/Slate/SAvaRundownInstancedPageList.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AvaRundownSubListDocumentTabFactory"

const FName FAvaRundownSubListDocumentTabFactory::FactoryId = "AvaSubListTabFactory";
const FString FAvaRundownSubListDocumentTabFactory::BaseTabName(TEXT("AvaSubListDocument"));

FName FAvaRundownSubListDocumentTabFactory::GetTabId(const FAvaRundownPageListReference& InSubListReference)
{
	return FName(BaseTabName + "_" + InSubListReference.SubListId.ToString());
}

FText FAvaRundownSubListDocumentTabFactory::GetTabLabel(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown)
{
	FText SubListIdAsText;
	
	if (IsValid(InRundown))
	{
		const FAvaRundownSubList& SubList = InRundown->GetSubList(InSubListReference);
		if (SubList.IsValid())
		{
			if (!SubList.Name.IsEmpty())
			{
				return SubList.Name;
			}
			
			// If the sublist has no display name, we will use it's index in the sublists.
			SubListIdAsText = FText::AsNumber(InRundown->GetSubListIndex(SubList) + 1);
		}
	}

	// If the rundown is not available for some reason, use the id.
	if (SubListIdAsText.IsEmpty())
	{
		SubListIdAsText = FText::FromString(InSubListReference.SubListId.ToString());
	}
	
	return FText::Format(LOCTEXT("RundownSubListDocument_TabLabel", "Page View {0}"), SubListIdAsText);
}

FText FAvaRundownSubListDocumentTabFactory::GetTabDescription(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown)
{
	const FText SubListId = FText::FromString(InSubListReference.SubListId.ToString());
	const FText SubListLabel = GetTabLabel(InSubListReference, InRundown);
	return FText::Format(LOCTEXT("RundownSubListDocument_ViewMenu_Desc", "{0} Id: {1}"), SubListLabel, SubListId);
}

FText FAvaRundownSubListDocumentTabFactory::GetTabTooltip(const FAvaRundownPageListReference& InSubListReference, const UAvaRundown* InRundown)
{
	const FText SubListId = FText::FromString(InSubListReference.SubListId.ToString());
	const FText SubListLabel = GetTabLabel(InSubListReference, InRundown);
	return FText::Format(LOCTEXT("RundownSubListDocument_ViewMenu_ToolTip", "{0} Id: {1}"), SubListLabel, SubListId);
}

FAvaRundownSubListDocumentTabFactory::FAvaRundownSubListDocumentTabFactory(const TSharedPtr<FAvaRundownEditor>& InRundownEditor)
	: FDocumentTabFactory(FactoryId, InRundownEditor)
	, RundownEditorWeak(InRundownEditor)
{
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");
}

TSharedRef<SWidget> FAvaRundownSubListDocumentTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	if (!SubListReference.SubListId.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SAvaRundownInstancedPageList, RundownEditorWeak.Pin(), SubListReference);
}

TSharedRef<SDockTab> FAvaRundownSubListDocumentTabFactory::SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, const FAvaRundownPageListReference& InSubListReference)
{
	SubListReference = InSubListReference;
	TabIdentifier = GetTabId(InSubListReference);
	
	const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();
	const UAvaRundown* Rundown = RundownEditor ? RundownEditor->GetRundown() : nullptr;
	TabLabel = GetTabLabel(InSubListReference, Rundown);
	ViewMenuDescription = GetTabDescription(InSubListReference, Rundown);
	ViewMenuTooltip = GetTabTooltip(InSubListReference, Rundown);

	TSharedRef<SDockTab> NewTab = SpawnTab(InInfo);

	const TSharedRef<SAvaRundownInstancedPageList> PageList = StaticCastSharedRef<SAvaRundownInstancedPageList>(NewTab->GetContent());
	NewTab->SetOnTabActivated(SDockTab::FOnTabActivatedCallback::CreateSP(PageList, &SAvaRundownInstancedPageList::OnTabActivated));
	PageList->SetMyTab(NewTab);

	return NewTab;
}

#undef LOCTEXT_NAMESPACE
