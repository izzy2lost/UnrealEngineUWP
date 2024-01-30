// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaChannel.h"

#include "AvaMediaDefines.h"
#include "AvaMediaEditorStyle.h"
#include "AvaMediaEditorUtils.h"
#include "AvalancheBroadcast.h"
#include "Broadcast/AvaBroadcastEditor.h"
#include "Broadcast/ChannelGrid/DragDropOps/AvaOutputTileItemDragDropOp.h"
#include "Broadcast/OutputDevices/AvaOutputClassItem.h"
#include "Broadcast/OutputDevices/DragDropOps/AvaOutputTreeItemDragDropOp.h"
#include "Brushes/SlateImageBrush.h"
#include "ClassIconFinder.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IStructureDetailsView.h"
#include "MediaOutput.h"
#include "OutputDevices/Slate/SAvaCaptureImage.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STileView.h"

#define LOCTEXT_NAMESPACE "SAvaChannel"

void SAvaChannel::Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor)
{
	BroadcastEditorWeak = InBroadcastEditor;
	ChannelName = InArgs._ChannelName;
	CanMaximize = InArgs._CanMaximize;
	OnMaximizeClicked = InArgs._OnMaximizeClicked;
	
	RegisterCommands();
	
	FAvaOutputChannel::GetOnChannelChanged().AddRaw(this, &SAvaChannel::OnChannelChanged);
	InBroadcastEditor->OnOutputTileSelectionChanged.AddRaw(this, &SAvaChannel::OnOutputTileSelectionChanged);
	
	ChildSlot
	[
		SNew(SOverlay)
		.Clipping(EWidgetClipping::ClipToBounds)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("SceneOutliner.TableViewRow")).DropIndicator_Onto)
			.Visibility(this, &SAvaChannel::GetChannelDragVisibility)
		]
		+ SOverlay::Slot()
		.Padding(2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.WidgetBorder"))
			[
				MakeChannelVerticalBox()
			]
		]
		//Min/Maximize and X button
		+ SOverlay::Slot()
		.VAlign(EVerticalAlignment::VAlign_Top)
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.Padding(0.0f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ChannelSettingsMenuAnchor, SMenuAnchor)
				.Content()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &SAvaChannel::OnChannelSettingsButtonClicked)
					.ToolTipText(LOCTEXT("ChannelSettingToolTip", "Open the channel settings"))
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SAvaChannel::OnChannelPinButtonClicked)
				.ToolTipText(LOCTEXT("ChannelPinToolTip", "Pin the channel across all profiles."))
				[
					SNew(SImage)
					.Image(this, &SAvaChannel::GetChannelPinBrush)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SAvaChannel::OnChannelMaximizeButtonClicked)
				.ToolTipText(this, &SAvaChannel::GetChannelMaximizeRestoreTooltipText)
				[
					SNew(SImage)
					.Image(this, &SAvaChannel::GetChannelMaximizeRestoreBrush)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SAvaChannel::OnChannelRemoveButtonClicked)
				.IsEnabled(this, &SAvaChannel::CanEditChanges)
				.ToolTipText(LOCTEXT("ChannelRemoveToolTip", "Remove this channel."))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.X"))
				]
			]
		]
		//Status Button
		+ SOverlay::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Top)
		.Padding(5.f, 5.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SAvaChannel::OnChannelStatusButtonClicked)
				.ToolTipText(LOCTEXT("ChannelStatusChangeToolTip", "Change the status of this channel."))
				[
					SAssignNew(ChannelStatusOptions, SMenuAnchor)
					.OnGetMenuContent(this, &SAvaChannel::GetChannelStatusOptions)
					[
						SNew(SImage)
						.Image(this, &SAvaChannel::GetChannelStatusBrush)
					]				
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2.f, 0.f, 0.f, 0.f)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SAvaChannel::GetChannelStatusText)
				.ToolTipText_Lambda([this]()
					{
						return FText::Format(LOCTEXT("ChannelStatusToolTip", "Current Channel Status: {0}"), GetChannelStatusText());
					})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SAvaChannel::OnChannelTypeToggleButtonClicked)
				.ToolTipText(LOCTEXT("ChannelTypeToggleToolTip", "Toggles between \"Program\" or \"Preview\" channel type."))
				[
					SNew(SImage)
					.Image(this, &SAvaChannel::GetChannelTypeBrush)
				]
			]
		]
	];

	OutputTileListView->SetStyle(nullptr);	
	const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(ChannelName);
	OnChannelBroadcastStateChanged(Channel);
	OnChannelMediaOutputsChanged(Channel);
}

SAvaChannel::~SAvaChannel()
{
	FAvaOutputChannel::GetOnChannelChanged().RemoveAll(this);	
	if (TSharedPtr<FAvaBroadcastEditor> BroadcastEditor = BroadcastEditorWeak.Pin())
	{
		BroadcastEditor->OnOutputTileSelectionChanged.RemoveAll(this);
	}
}

void SAvaChannel::RegisterCommands()
{
	MediaOutputCommandList = MakeShared<FUICommandList>();
	
	MediaOutputCommandList->MapAction(FGenericCommands::Get().Delete
		, FExecuteAction::CreateSP(this, &SAvaChannel::DeleteSelectedOutputTiles)
		, FCanExecuteAction::CreateSP(this, &SAvaChannel::CanEditChanges));
}

void SAvaChannel::SetPosition(int32 InColumnIndex, int32 InRowIndex)
{
	ColumnIndex= InColumnIndex;
	RowIndex = InRowIndex;
}

void SAvaChannel::OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::State))
	{
		OnChannelBroadcastStateChanged(InChannel);
	}
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::RenderTarget))
	{
		OnChannelRenderTargetChanged(InChannel);
	}
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::MediaOutputs))
	{
		OnChannelMediaOutputsChanged(InChannel);
	}
}

void SAvaChannel::OnChannelBroadcastStateChanged(const FAvaOutputChannel& InChannel)
{	
	if (InChannel.IsValidChannel() && InChannel.GetChannelName() == ChannelName)
	{
		ChannelState = InChannel.GetState();
		ChannelStateText = FAvaMediaEditorUtils::GetChannelStatusText(ChannelState, InChannel.GetIssueSeverity());
		ChannelStatusBrush = FAvaMediaEditorUtils::GetChannelStatusBrush(ChannelState, InChannel.GetIssueSeverity());
		
		if (ChannelState != EAvaChannelState::Live)
		{
			ChannelPreviewBrush.Reset();
		}

		//Update ChannelPreviewBrush when Visibility Changes. It might be we haven't created it yet.
		OnChannelRenderTargetChanged(InChannel);
	}
}

void SAvaChannel::OnChannelRenderTargetChanged(const FAvaOutputChannel& InChannel)
{
	if (InChannel.IsValidChannel() && InChannel.GetChannelName() == ChannelName)
	{
		UTextureRenderTarget2D* const RenderTarget = InChannel.GetCurrentRenderTarget(true);
		
		//Only Invert Alpha if the Current Render Target isn't the Placeholder RT, since Placeholder uses Widget Rendering.
		//TODO: Need to check whether the Channel or Media Outputs is doing Invert Alpha rather than figuring it out here
		bShouldInvertAlpha = InChannel.GetPlaceholderRenderTarget() != RenderTarget;

		if (RenderTarget)
		{
			const FVector2D RenderTargetSize(RenderTarget->SizeX, RenderTarget->SizeY);
			
			//If Brush is invalid, or the Brush's Texture Target doesn't match the new Render Target, reset the Brush.
			if (!ChannelPreviewBrush.IsValid() || ChannelPreviewBrush->GetResourceObject() != RenderTarget)
			{
				ChannelPreviewBrush = MakeShared<FSlateImageBrush>(RenderTarget, RenderTargetSize);
			}
			//If Sizes mismatch, just resizes rather than recreating the Brush with same underlying Resource
			else if (ChannelPreviewBrush->GetImageSize() !=  RenderTargetSize)
			{
				ChannelPreviewBrush->SetImageSize(RenderTargetSize);
			}
		}
	}
}

void SAvaChannel::OnChannelMediaOutputsChanged(const FAvaOutputChannel& InChannel)
{
	if (InChannel.IsValidChannel() && InChannel.GetChannelName() == ChannelName)
	{
		TSet<UMediaOutput*> CurrentMediaOutputs(InChannel.GetMediaOutputs());
		
		TSet<UMediaOutput*> SeenMediaOutputs;
		SeenMediaOutputs.Reserve(OutputTileItems.Num());
		
		//Remove Items no longer in the Media Outputs List
		for (TArray<FAvaOutputTileItemPtr>::TIterator Iter = OutputTileItems.CreateIterator(); Iter; ++Iter)
		{
			const FAvaOutputTileItemPtr& Item = *Iter;

			//Remove if Item or Underlying Media Output is Invalid, 
			if (!Item.IsValid())
			{
				Iter.RemoveCurrent();
				continue;
			}

			UMediaOutput* const MediaOutput = Item->GetMediaOutput();
			if (!IsValid(MediaOutput) || !CurrentMediaOutputs.Contains(MediaOutput))
			{
				Iter.RemoveCurrent();
			}
			else
			{
				SeenMediaOutputs.Add(MediaOutput);
			}
		}

		//Add New Media Outputs as Item in the List
		{
			TArray<UMediaOutput*> NewMediaOutputs = CurrentMediaOutputs.Difference(SeenMediaOutputs).Array();
			OutputTileItems.Reserve(OutputTileItems.Num() + NewMediaOutputs.Num());
			
			for (UMediaOutput* const MediaOutput : NewMediaOutputs)
			{
				TSharedPtr<FAvaOutputTileItem> Item = MakeShared<FAvaOutputTileItem>(ChannelName, MediaOutput);
				OutputTileItems.Add(Item);
			}
		}
	
		if (OutputTileListView.IsValid())
		{
			OutputTileListView->RequestListRefresh();
		}
	}
}

void SAvaChannel::OnOutputTileSelectionChanged(const TSharedPtr<FAvaOutputTileItem>& Item)
{
	if (OutputTileListView.IsValid() && !OutputTileItems.Contains(Item))
	{
		//Clear Selection without Notifying
		OutputTileListView->Private_ClearSelection();
	}
}

void SAvaChannel::DeleteSelectedOutputTiles()
{
	if (OutputTileListView.IsValid() && OutputTileListView->GetNumItemsSelected() > 0)
	{
		TArray<FAvaOutputTileItemPtr> SelectedTiles = OutputTileListView->GetSelectedItems();
		TArray<UMediaOutput*> MediaOutputs;
		MediaOutputs.Reserve(SelectedTiles.Num());

		for (const FAvaOutputTileItemPtr& SelectedTile : SelectedTiles)
		{
			if (SelectedTile.IsValid())
			{
				MediaOutputs.Add(SelectedTile->GetMediaOutput());
			}
		}

		FScopedTransaction Transaction(LOCTEXT("RemoveMediaOutputs", "Remove Media Outputs"));
		
		UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
		Broadcast.Modify();
		
		const int32 RemovedCount = Broadcast.GetCurrentProfile().RemoveChannelMediaOutputs(ChannelName, MediaOutputs);
		
		if (RemovedCount == 0)
		{
			Transaction.Cancel();
		}
	}
}

FText SAvaChannel::GetChannelStatusText() const
{
	return ChannelStateText;
}

TSharedRef<SWidget> SAvaChannel::GetChannelStatusOptions()
{
	FMenuBuilder Builder(true, nullptr);
	
	Builder.BeginSection("Status", LOCTEXT("BroadcastStatus", "Broadcast Status"));
	{
		const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(ChannelName);
		// An offline channel can't be made live from here.
		if (Channel.GetState() != EAvaChannelState::Offline)
		{
			for (uint8 Index = 0; Index < static_cast<uint8>(EAvaChannelState::Max); ++Index)
			{
				const EAvaChannelState State = static_cast<EAvaChannelState>(Index);

				// Only add other states that the broadcast isn't currently in (or offline).
				if (Channel.GetState() != State && State != EAvaChannelState::Offline)
				{
					Builder.AddMenuEntry(StaticEnum<EAvaChannelState>()->GetDisplayNameTextByIndex(Index)
						, FText()
						, FSlateIcon()
						, FUIAction(FExecuteAction::CreateSP(this, &SAvaChannel::OnChannelStatusSelected, State)));	
				}
			}
		}
	}
	Builder.EndSection();
	
	return Builder.MakeWidget();
}

TSharedRef<SWidget> SAvaChannel::MakeChannelVerticalBox()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(0.1f)
		.Padding(0.f, 2.f, 0.f, 2.f)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.MaxHeight(30.f)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFitY)
			[
				SAssignNew(ChannelNameTextBlock, SInlineEditableTextBlock)
				.Text(this, &SAvaChannel::GetChannelNameText)
				.OnVerifyTextChanged(this, &SAvaChannel::OnVerifyChannelNameTextChanged)
				.OnTextCommitted(this, &SAvaChannel::OnChannelNameTextCommitted)
				.IsReadOnly(this, &SAvaChannel::IsReadOnly)
				.Justification(ETextJustify::Center)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.15f)
		.MaxHeight(50.f)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFitY)
				[
					MakeMediaOutputsTileView()
				]
			]
			+ SOverlay::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Visibility(this, &SAvaChannel::GetMediaOutputEmptyTextVisibility)
				.Text(LOCTEXT("NoMediaOutputsFound", "No Media Outputs. Drag and Drop from the Output Devices List."))
				.TextStyle(FAppStyle::Get(), "HintText")
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.75f)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SBorder)
			.Visibility(this, &SAvaChannel::GetChannelPreviewVisibility)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				[
					SNew(SAvaCaptureImage)
					.ImageArgs(SImage::FArguments()
						.Image(this, &SAvaChannel::GetChannelPreviewBrush))
					.ShouldInvertAlpha(this, &SAvaChannel::ShouldInvertAlpha)
					.EnableGammaCorrection(false)
					.EnableBlending(false)
				]
			]
		];		
}

TSharedRef<SWidget> SAvaChannel::MakeMediaOutputsTileView()
{
	return SAssignNew(OutputTileListView, SListView<FAvaOutputTileItemPtr>)
		.Orientation(EOrientation::Orient_Horizontal)
		.ScrollbarVisibility(EVisibility::Collapsed)
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource(&OutputTileItems)
		.OnGenerateRow(this, &SAvaChannel::OnGenerateMediaOutputTile)
		.OnSelectionChanged(this, &SAvaChannel::OnMediaOutputTileSelectionChanged)
		.OnContextMenuOpening(this, &SAvaChannel::OnMediaOutputTileContextMenuOpening)
	;
}

TSharedRef<ITableRow> SAvaChannel::OnGenerateMediaOutputTile(FAvaOutputTileItemPtr Item, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	check(Item.IsValid());
	return SNew(STableRow<FAvaOutputTileItemPtr>, InOwnerTable)
		.Padding(FMargin(5.f, 5.f))
		.OnDragDetected(Item.ToSharedRef(), &FAvaOutputTileItem::OnDragDetected)
		[
			Item->GenerateTile()
		];
}

void SAvaChannel::OnMediaOutputTileSelectionChanged(FAvaOutputTileItemPtr Item, ESelectInfo::Type SelectInfo)
{
	if (TSharedPtr<FAvaBroadcastEditor> BroadcastEditor = BroadcastEditorWeak.Pin())
	{
		BroadcastEditor->SelectOutputTile(Item);
	}
}

TSharedPtr<SWidget> SAvaChannel::OnMediaOutputTileContextMenuOpening() const
{
	FMenuBuilder MenuBuilder(true, MediaOutputCommandList);
	MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

void SAvaChannel::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SetDragging(false);
	if (CanEditChanges())
	{
		if (const TSharedPtr<FAvaOutputTreeItemDragDropOp> OutputClassDragDropOp = DragDropEvent.GetOperationAs<FAvaOutputTreeItemDragDropOp>())
		{
			if (OutputClassDragDropOp->IsValidToDropInChannel(ChannelName))
			{
				SetDragging(true);
			}
		}
		else if (const TSharedPtr<FAvaOutputTileItemDragDropOp> OutputTileDragDropOp = DragDropEvent.GetOperationAs<FAvaOutputTileItemDragDropOp>())
		{
			if (OutputTileDragDropOp->IsValidToDropInChannel(ChannelName))
			{
				SetDragging(true);
			}
		}
	}
}

void SAvaChannel::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SetDragging(false);
}

FReply SAvaChannel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SetDragging(false);
	if (CanEditChanges())
	{
		if (const TSharedPtr<FAvaOutputTreeItemDragDropOp> OutputItemDragDropOp = DragDropEvent.GetOperationAs<FAvaOutputTreeItemDragDropOp>())
		{
			OutputItemDragDropOp->OnChannelDrop(ChannelName);
		}
		else if (const TSharedPtr<FAvaOutputTileItemDragDropOp> OutputTileDragDropOp = DragDropEvent.GetOperationAs<FAvaOutputTileItemDragDropOp>())
		{
			OutputTileDragDropOp->OnChannelDrop(ChannelName);
		}
	}
	return FReply::Unhandled();
}

FReply SAvaChannel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (MediaOutputCommandList.IsValid() && MediaOutputCommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAvaChannel::SetDragging(bool bIsDragging)
{
	DragVisibility = bIsDragging
		? EVisibility::SelfHitTestInvisible
		: EVisibility::Hidden;
}

FText SAvaChannel::GetChannelNameText() const
{
	return FText::FromName(ChannelName);
}

bool SAvaChannel::OnVerifyChannelNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	return UAvalancheBroadcast::Get().CanRenameChannel(ChannelName, FName(InText.ToString()));
}

void SAvaChannel::OnChannelNameTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	const FName NewChannelName(InText.ToString());

	FScopedTransaction Transaction(LOCTEXT("RenameChannel", "Rename Channel"));
	UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
	Broadcast.Modify();
	
	if (Broadcast.RenameChannel(ChannelName, NewChannelName))
	{
		ChannelName = NewChannelName;
	}
	else
	{
		Transaction.Cancel();
	}
}

FReply SAvaChannel::OnChannelStatusButtonClicked()
{
	if (ChannelStatusOptions.IsValid() && !ChannelStatusOptions->IsOpen())
	{
		ChannelStatusOptions->SetIsOpen(true);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAvaChannel::OnChannelSettingsButtonClicked()
{
	if (!ChannelSettings.IsValid())
	{
		FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannelMutable(ChannelName);
		if (!Channel.IsValidChannel())
		{
			return FReply::Unhandled();
		}

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bShowOptions = false;

		TSharedRef<FStructOnScope> ChannelStruct = MakeShared<FStructOnScope>(FAvaOutputChannel::StaticStruct(), reinterpret_cast<uint8*>(&Channel));

		ChannelSettings = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, FStructureDetailsViewArgs(), ChannelStruct);

		ChannelSettings->GetOnFinishedChangingPropertiesDelegate().AddLambda(
			[ChannelStructWeak = TWeakPtr<FStructOnScope>(ChannelStruct)](const FPropertyChangedEvent&)
			{
				if (TSharedPtr<FStructOnScope> ChannelStruct = ChannelStructWeak.Pin())
				{
					const FAvaOutputChannel& Channel = *reinterpret_cast<const FAvaOutputChannel*>(ChannelStruct->GetStructMemory());
					FAvaOutputChannel::GetOnChannelChanged().Broadcast(Channel, EAvaChannelChange::Settings);
				}
			});

		ChannelSettingsMenuAnchor->SetMenuContent(SNew(SBox)
				.MaxDesiredHeight(500.f)
				.MinDesiredWidth(150.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
				    	ChannelSettings->GetWidget().ToSharedRef()
					]
				]
			);
	}

	ChannelSettingsMenuAnchor->SetIsOpen(true);
	return FReply::Handled();
}

FReply SAvaChannel::OnChannelPinButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("ToggleChannelPin", "Toggle Channel Pin"));
	
	UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();	
	Broadcast.Modify();
	if (!Broadcast.IsChannelPinned(ChannelName))
	{
		Broadcast.PinChannel(ChannelName, Broadcast.GetCurrentProfileName());
	}
	else
	{
		Broadcast.UnpinChannel(ChannelName);
	}
	Broadcast.RebuildProfiles();
	Broadcast.QueueNotifyChange(EAvaBroadcastChange::ChannelGrid);
	Broadcast.GetOnChannelsListChanged().Broadcast(Broadcast.GetCurrentProfile());
	
	return FReply::Handled();
}

FReply SAvaChannel::OnChannelTypeToggleButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("ToggleChannelType", "Toggle Channel Type"));

	UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
	Broadcast.Modify();

	const EAvaBroadcastChannelType NewChannelType = (Broadcast.GetChannelType(ChannelName) == EAvaBroadcastChannelType::Preview)
		? EAvaBroadcastChannelType::Program : EAvaBroadcastChannelType::Preview;
	Broadcast.SetChannelType(ChannelName, NewChannelType);
	Broadcast.QueueNotifyChange(EAvaBroadcastChange::ChannelType);
	Broadcast.GetOnChannelsListChanged().Broadcast(Broadcast.GetCurrentProfile());

	return FReply::Handled();
}

FReply SAvaChannel::OnChannelMaximizeButtonClicked()
{
	if (OnMaximizeClicked.IsBound())
	{
		OnMaximizeClicked.Execute(SharedThis(this));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAvaChannel::OnChannelRemoveButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("RemoveChannel", "Remove Channel"));
	
	UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
	Broadcast.Modify();
	
	if (Broadcast.GetCurrentProfile().RemoveChannel(ChannelName))
	{
		return FReply::Handled();
	}

	Transaction.Cancel();
	return FReply::Unhandled();
}

void SAvaChannel::OnChannelStatusSelected(EAvaChannelState State)
{
	FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannelMutable(ChannelName);
	if (Channel.IsValidChannel())
	{
		switch (State)
		{
		case EAvaChannelState::Idle:
			Channel.StopChannelBroadcast();
			break;
		case EAvaChannelState::Live:
			Channel.StartChannelBroadcast();
			break;
		}
	}
}

const FSlateBrush* SAvaChannel::GetChannelStatusBrush() const
{
	return ChannelStatusBrush;
}

const FSlateBrush* SAvaChannel::GetChannelPreviewBrush() const
{
	return ChannelPreviewBrush.Get();
}

const FSlateBrush* SAvaChannel::GetChannelPinBrush() const
{
	return UAvalancheBroadcast::Get().IsChannelPinned(ChannelName) ?
		FAppStyle::Get().GetBrush("Icons.Pinned") : FAppStyle::Get().GetBrush("Icons.Unpinned");
}

const FSlateBrush* SAvaChannel::GetChannelMaximizeRestoreBrush() const
{
	FName BrushName = TEXT("EditorViewport.Front");
	if (CanMaximize.IsBound() && !CanMaximize.Get())
	{
		BrushName = TEXT("LevelEditor.Tabs.Viewports");
	}
	return FAppStyle::Get().GetBrush(BrushName);
}

const FSlateBrush* SAvaChannel::GetChannelTypeBrush() const
{
	return UAvalancheBroadcast::Get().GetChannelType(ChannelName) == EAvaBroadcastChannelType::Preview ?
		FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.ChannelTypePreview")
		: FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.ChannelTypeProgram");
}

EVisibility SAvaChannel::GetMediaOutputEmptyTextVisibility() const
{
	return OutputTileItems.IsEmpty()
		? EVisibility::Visible
		: EVisibility::Hidden;
}

EVisibility SAvaChannel::GetChannelPreviewVisibility() const
{
	return UAvalancheBroadcast::Get().CanShowPreview()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SAvaChannel::GetChannelDragVisibility() const
{
	return DragVisibility;
}

bool SAvaChannel::ShouldInvertAlpha() const
{
	return bShouldInvertAlpha;
}

bool SAvaChannel::IsReadOnly() const
{
	return ChannelState == EAvaChannelState::Live;
}

bool SAvaChannel::CanEditChanges() const
{
	return !IsReadOnly();
}

FText SAvaChannel::GetChannelMaximizeRestoreTooltipText() const
{
	FText TooltipText = LOCTEXT("ChannelMaximizeToolTip", "Maximize this channel in the view. All other channels will be hidden.");
	if (CanMaximize.IsBound() && !CanMaximize.Get())
	{
		TooltipText = LOCTEXT("ChannelRestoreToolTip", "Restore this channel in the view. All other channels will be visible.");
	}
	return TooltipText;
}

#undef LOCTEXT_NAMESPACE
