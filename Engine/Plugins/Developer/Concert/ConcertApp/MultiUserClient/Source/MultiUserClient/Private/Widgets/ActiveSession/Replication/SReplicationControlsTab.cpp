// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationControlsTab.h"

#include "ConcertFrontendUtils.h"
#include "SReplicationConnectionControls.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationControlsTab"

namespace UE::MultiUserClient
{
	namespace Private
	{
		static TSharedRef<SWidget> MakeExpandableArea(
			TSharedPtr<SExpandableArea>& OutArea,
			FOnBooleanValueChanged OnAreaExpansionChanged,
			FText Label,
			FText Tooltip,
			TSharedRef<SWidget> BodyContent,
			bool bInitiallyCollapsed
			)
		{
			return SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.0f)
				[
					SAssignNew(OutArea, SExpandableArea)
					.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
					.BorderImage_Lambda([&OutArea]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*OutArea); })
					.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.OnAreaExpansionChanged(MoveTemp(OnAreaExpansionChanged))
					.Padding(0.0f)
					.InitiallyCollapsed(bInitiallyCollapsed)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(MoveTemp(Label))
						.ToolTipText(MoveTemp(Tooltip))
						.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
					]
					.BodyContent()
					[
						MoveTemp(BodyContent)
					]
				];
		}
	}
	
	void SReplicationControlsTab::Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager)
	{
		constexpr bool bCollpaseInitially_ConnectionArea	= false;
		constexpr bool bCollpaseInitially_AttributesArea	= true;
		constexpr bool bCollpaseInitially_StreamsArea		= true;
		
		ChildSlot
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			// Connection area
			+SSplitter::Slot()
			.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SReplicationControlsTab::GetConnectionAreaSizeRule))
			.Value(0.3)
			[
				Private::MakeExpandableArea(
					ConnectionArea,
					FOnBooleanValueChanged::CreateSP(this, &SReplicationControlsTab::OnConnectionAreaExpansionChanged),
					LOCTEXT("ConnectionArea.Label", "Connection"),
					LOCTEXT("ConnectionArea.Tooltip", "Control the replication connection, e.g. join and leave"),
					SNew(SReplicationConnectionControls, InReplicationManager),
					bCollpaseInitially_ConnectionArea
					)
			]
			
			// Attributes area
			+SSplitter::Slot()
			.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SReplicationControlsTab::GetClientAttributesAreaSizeRule))
			.Value(0.3)
			[
				Private::MakeExpandableArea(
					ClientAttributesArea,
					FOnBooleanValueChanged::CreateSP(this, &SReplicationControlsTab::OnAClientsttributesAreaExpansionChanged),
					LOCTEXT("AttributesArea.Label", "Client Receive Attributes"),
					LOCTEXT("AttributesArea.Tooltip", "View and edit your client attributes, which determine the data received."),
					SNew(STextBlock)
						.Text(LOCTEXT("ToDo", "ToDo")),
					bCollpaseInitially_AttributesArea
					)
			]
			
			// Streams area
			+SSplitter::Slot()
			.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SReplicationControlsTab::GetStreamsAreaSizeRule))
			.Value(0.3)
			[
				Private::MakeExpandableArea(
					StreamsArea,
					FOnBooleanValueChanged::CreateSP(this, &SReplicationControlsTab::OnStreamsAreaExpansionChanged),
					LOCTEXT("StreamsArea.Label", "Streams"),
					LOCTEXT("StreamsArea.Tooltip", "View your replication streams as well as toggle sent objects and properties"),
					SNew(STextBlock)
						.Text(LOCTEXT("ToDo", "ToDo")),
					bCollpaseInitially_StreamsArea
					)
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE