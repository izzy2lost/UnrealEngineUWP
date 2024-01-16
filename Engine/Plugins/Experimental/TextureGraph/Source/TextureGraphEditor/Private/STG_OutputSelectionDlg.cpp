// Copyright Epic Games, Inc. All Rights Reserved.
#include "STG_OutputSelectionDlg.h"
#include "Widgets/Layout/SBox.h"
#include "TG_Pin.h"
#include "TG_Graph.h"
#include "Widgets/Layout/SSeparator.h"
#include "SPrimaryButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "Editor.h"
#include "EDGraph/STG_NodeThumbnail.h"
#include "STG_OutputSelector.h"
#include "TG_HelperFunctions.h"
#include "EdGraph/TG_EdGraphNode.h"
#include "Widgets/Colors/SColorBlock.h"
#define LOCTEXT_NAMESPACE "STG_OutputSelectionDlg"
void STG_OutputSelectionDlg::Construct(const FArguments& InArgs)
{
	OutputSettingsSet = InArgs._OutputSettingsSet;
	EdGraph = InArgs._EdGraph;
	SWindow::Construct(SWindow::FArguments()
		.Title(InArgs._Title)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		//.SizingRule( ESizingRule::Autosized )
		.ClientSize(FVector2D(350, 450))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // Add user input block
			.Padding(2, 2, 2, 4)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1)
					[
						SAssignNew(ScrollBox,SScrollBox)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(8.f, 16.f)
			[
				SNew(SUniformGridPanel)
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("Export", "Export"))
					.OnClicked(this, &STG_OutputSelectionDlg::OnButtonClick, EAppReturnType::Ok)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STG_OutputSelectionDlg::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		]);
	AddExportItems();
}
void STG_OutputSelectionDlg::AddExportItems()
{
	ScrollBox->ClearChildren();
	for (const auto& Info : OutputSettingsSet->OutputExpressionInfos)
	{
		auto OutputSetting = Info.OutputPtr->OutputSettings;
		auto Node = Cast<UTG_Node>(Info.OutputPtr->GetOuter());
		auto OutPinIds = Node->GetOutputPinIds();
		for (auto Id : OutPinIds)
		{
			//This is a work around for checking the type of the output
			//Probably we need to have a better solution for checking output type
			auto Pin = Node->GetGraph()->GetPin(Id);
			FTG_Variant Variant;
			Pin->GetValue(Variant);
			TSharedPtr<SWidget> ThumbnailWidget;
			if (Variant.IsTexture())
			{
				UTG_EdGraphNode* EdNode = EdGraph->GetViewModelNode(Node->GetId());
				TiledBlobPtr ThumbBlob = EdNode->GetCachedThumbBlob(Id);

				if (!ThumbBlob)
				{
					ThumbBlob = TextureHelper::GetBlack();
				}

				TSharedPtr<STG_NodeThumbnail> NodeThumbnail = SNew(STG_NodeThumbnail);
				ThumbBlob->OnFinalise()
					.then([ThumbBlob, NodeThumbnail]
					{
						NodeThumbnail->UpdateBlob(ThumbBlob);
					});

				ThumbnailWidget = NodeThumbnail;
			}
			// else if color do something
			else if (Variant.IsColor())
			{
				FLinearColor ColorValue;
				Pin->GetValue(ColorValue);
				ThumbnailWidget = SNew(SColorBlock)
									.Color(ColorValue); 
			}
			else
			{
				continue;
			}
			ScrollBox->AddSlot()
			.Padding(5)
			[
				SNew(STG_OutputSelector)
				.Name(FText::FromString(Info.OutputName.ToString()))
				.ThumbnailWidget(ThumbnailWidget)
				.OnOutputSelectionChanged(this,&STG_OutputSelectionDlg::OnOutputSelectionChanged)
				.bIsSelected(Info.bExport)
			];
			ScrollBox->AddSlot()
			[
				SNew(SSeparator)
				.Thickness(1)
			];
		}
	}
}
FReply STG_OutputSelectionDlg::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;
	if (ButtonID == EAppReturnType::Cancel || ButtonID == EAppReturnType::Ok)
	{
		// Only close the window if canceling or if the ok
		RequestDestroyWindow();
	}
	else
	{
		// reset the user response in case the window is closed using 'x'.
		UserResponse = EAppReturnType::Cancel;
	}
	return FReply::Handled();
}
EAppReturnType::Type STG_OutputSelectionDlg::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}
void STG_OutputSelectionDlg::OnOutputSelectionChanged(const FString ItemName, ECheckBoxState NewState)
{
	OutputSettingsSet->GetOutputExpressionInfo(*ItemName)->bExport = NewState == ECheckBoxState::Checked;
}
#undef LOCTEXT_NAMESPACE