// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaBroadcastChannels.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaEditorSettings.h"
#include "Broadcast/AvaBroadcastEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SAvaBroadcastProfileEntry.h"
#include "SAvaChannel.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SGridPanel.h"

#define LOCTEXT_NAMESPACE "SAvaBroadcastChannels"

SAvaBroadcastChannels::FAvaChannelMaximizer::FAvaChannelMaximizer()
{
	MaximizeSequence.AddCurve(0.f, 0.25f, ECurveEaseFunction::CubicInOut);
}

void SAvaBroadcastChannels::FAvaChannelMaximizer::Reset()
{
	ChannelWidgetWeak.Reset();
	bMaximizing = false;
}

void SAvaBroadcastChannels::FAvaChannelMaximizer::ToggleMaximize(const TSharedRef<SAvaChannel>& InChannelWidget)
{
	ChannelWidgetWeak = InChannelWidget;
	bMaximizing = !bMaximizing;

	//Do reverse here so that we don't have to calculate 1.f - Lerp everytime
	if (bMaximizing)
	{
		MaximizeSequence.PlayReverse(InChannelWidget);
	}
	else
	{
		MaximizeSequence.Play(InChannelWidget);
	}
}

float SAvaBroadcastChannels::FAvaChannelMaximizer::GetRowFill(int32 InRowIndex) const
{
	if (!ChannelWidgetWeak.IsValid() || ChannelWidgetWeak.Pin()->GetRowIndex() == InRowIndex)
	{
		return 1.f;
	}

	if (MaximizeSequence.IsPlaying())
	{
		return FMath::Max(KINDA_SMALL_NUMBER, MaximizeSequence.GetLerp());
	}

	return bMaximizing ? KINDA_SMALL_NUMBER : 1.f;
}

float SAvaBroadcastChannels::FAvaChannelMaximizer::GetColumnFill(int32 InColumnIndex) const
{
	if (!ChannelWidgetWeak.IsValid() || ChannelWidgetWeak.Pin()->GetColumnIndex() == InColumnIndex)
	{
		return 1.f;
	}

	if (MaximizeSequence.IsPlaying())
	{
		return FMath::Max(KINDA_SMALL_NUMBER, MaximizeSequence.GetLerp());
	}

	return bMaximizing ? KINDA_SMALL_NUMBER : 1.f;
}

void SAvaBroadcastChannels::Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor)
{
	BroadcastEditorWeak = InBroadcastEditor;
	check(InBroadcastEditor.IsValid());
	
	UAvalancheBroadcast* const Broadcast = InBroadcastEditor->GetBroadcastObject();
	BroadcastWeak = Broadcast;
	check(IsValid(Broadcast));
	
	Broadcast->AddChangeListener(FOnAvaBroadcastChanged::FDelegate::CreateSP(this
		, &SAvaBroadcastChannels::OnBroadcastChanged));
	
	ChannelGrid = SNew(SGridPanel);
	
	ChildSlot
	.HAlign(EHorizontalAlignment::HAlign_Fill)
	.VAlign(EVerticalAlignment::VAlign_Fill)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			MakeChannelsToolbar()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			ChannelGrid.ToSharedRef()
		]
	];

	RefreshChannelGrid();
}

SAvaBroadcastChannels::~SAvaBroadcastChannels()
{
	if (BroadcastWeak.IsValid())
	{
		BroadcastWeak->RemoveChangeListener(this);
	}
}

bool SAvaBroadcastChannels::CanAddChannel()
{
	const int32 ChannelCount = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannels().Num();
					
	const UAvalancheMediaEditorSettings& MediaSettings = UAvalancheMediaEditorSettings::Get();
	
	return !MediaSettings.bBroadcastEnforceMaxChannelCount
		|| ChannelCount < MediaSettings.BroadcastMaxChannelCount;
}

void SAvaBroadcastChannels::AddChannel()
{
	if (UAvalancheBroadcast* const Broadcast = BroadcastWeak.Get())
	{
		FScopedTransaction Transaction(LOCTEXT("AddChannel", "Add Channel"));
		Broadcast->Modify();
		Broadcast->GetCurrentProfile().AddChannel();
	}
}

float SAvaBroadcastChannels::GetRowFill(int32 RowId) const
{
	return ChannelMaximizer.GetRowFill(RowId);
}

float SAvaBroadcastChannels::GetColumnFill(int32 ColumnId) const
{
	return ChannelMaximizer.GetColumnFill(ColumnId);
}

TSharedRef<SWidget> SAvaBroadcastChannels::MakeChannelsToolbar()
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None);
	
	ToolBarBuilder.BeginSection(TEXT("Channels"));
	{
		const FUIAction AddChannelAction(FExecuteAction::CreateSP(this, &SAvaBroadcastChannels::AddChannel)
			, FCanExecuteAction::CreateSP(this, &SAvaBroadcastChannels::CanAddChannel));
		
		ToolBarBuilder.AddToolBarButton(AddChannelAction
			, NAME_None
			, LOCTEXT("NewChannel_Label", "New Channel")
			, LOCTEXT("NewChannel_ToolTip", "New Channel")
			, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
		);		
	}
	ToolBarBuilder.EndSection();
	
	return ToolBarBuilder.MakeWidget();
}

bool SAvaBroadcastChannels::CanMaximizeChannel() const
{
	const int32 ChannelCount = Channels.Num();
	return ChannelCount == 1 || (ChannelCount > 1 && !ChannelMaximizer.bMaximizing);
}

void SAvaBroadcastChannels::ToggleMaximizeChannel(const TSharedRef<SAvaChannel>& InWidget)
{
	if (Channels.Num() > 1)
	{
		ChannelMaximizer.ToggleMaximize(InWidget);
	}
}

void SAvaBroadcastChannels::OnBroadcastChanged(EAvaBroadcastChange ChangedEvent)
{
	if (EnumHasAnyFlags(ChangedEvent, EAvaBroadcastChange::CurrentProfile))
	{
		Channels.Reset();
		ChannelGrid->ClearChildren();
		ChannelMaximizer.Reset();
	}
	RefreshChannelGrid();
}

void SAvaBroadcastChannels::RefreshChannelGrid()
{
	if (BroadcastWeak.IsValid())
	{
		const FAvaBroadcastProfile& Profile = BroadcastWeak->GetCurrentProfile();
		
		//Remove Items that are not in the New Channel List
		for (TMap<FName, TSharedPtr<SAvaChannel>>::TIterator Iter = Channels.CreateIterator(); Iter; ++Iter)
		{
			//Remove Invalid Channels
			if (!Iter->Value.IsValid())
			{
				Iter.RemoveCurrent();
				continue;
			}
			
			//TODO: Currently Remove all Slots and Re-add them in Order
			ChannelGrid->RemoveSlot(Iter->Value.ToSharedRef());

			const FAvaOutputChannel& Channel = Profile.GetChannel(Iter->Key);
			
			//Refresh the Media Tiles as they might've been deleted in an Undo operation
			Iter->Value->OnChannelMediaOutputsChanged(Channel);

			const bool bInvalidChannel = !Channel.IsValidChannel();
			const bool bMismatchChannelName = Iter->Key != Iter->Value->GetChannelName();
			
			if (bInvalidChannel || bMismatchChannelName)
			{
				//Reset Maximizer State if we're removing the Channel that is Maximized
				if (Iter->Value == ChannelMaximizer.ChannelWidgetWeak)
				{
					ChannelMaximizer.Reset();
				}
				Iter.RemoveCurrent();
			}
		}

		int32 ItemCount = 0;

		const TArray<FAvaOutputChannel*>& BroadcastChannels = Profile.GetChannels();
		
		const int32 ItemsPerRow = FMath::RoundHalfToZero(FMath::Sqrt(static_cast<float>(BroadcastChannels.Num())));

		TSharedPtr<FAvaBroadcastEditor> BroadcastEditor = BroadcastEditorWeak.Pin();

		ChannelGrid->ClearFill();
		
		//Re-add existing Widgets or add new ones
		for (const FAvaOutputChannel* InChannel : BroadcastChannels)
		{
			const FName ChannelName = InChannel->GetChannelName();
			if (!Channels.Contains(ChannelName))
			{
				TSharedRef<SAvaChannel> NewChannel = SNew(SAvaChannel, BroadcastEditor)
					.ChannelName(ChannelName)
					.CanMaximize(this, &SAvaBroadcastChannels::CanMaximizeChannel)
					.OnMaximizeClicked(this, &SAvaBroadcastChannels::ToggleMaximizeChannel);
				
				Channels.Add(ChannelName, NewChannel);
			}
			
			const int32 Row = ItemCount / ItemsPerRow;
			const int32 Column = ItemCount % ItemsPerRow;
			
			TSharedRef<SAvaChannel> Channel = Channels.Find(ChannelName)->ToSharedRef();
			Channel->SetPosition(Column, Row);

			ChannelGrid->AddSlot(Column, Row)
				.VAlign(EVerticalAlignment::VAlign_Fill)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					Channel
				];

			++ItemCount;
		}

		//Set Fill Attributes
		if (ItemsPerRow > 0)
		{
			const int32 ColumnCount = ItemsPerRow;
			const int32 RowCount = FMath::CeilToInt(static_cast<float>(ItemCount) / static_cast<float>(ItemsPerRow));
			
			for (int32 Index = 0; Index < ColumnCount; ++Index)
			{
				ChannelGrid->SetColumnFill(Index, TAttribute<float>::CreateSP(this, &SAvaBroadcastChannels::GetColumnFill, Index));
			}
			
			for (int32 Index = 0; Index < RowCount; ++Index)
			{
				ChannelGrid->SetRowFill(Index, TAttribute<float>::CreateSP(this, &SAvaBroadcastChannels::GetRowFill, Index));
			}
		}		
	}
}

FReply SAvaBroadcastChannels::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && BroadcastEditorWeak.IsValid())
	{
		BroadcastEditorWeak.Pin()->SelectOutputTile(nullptr);
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
