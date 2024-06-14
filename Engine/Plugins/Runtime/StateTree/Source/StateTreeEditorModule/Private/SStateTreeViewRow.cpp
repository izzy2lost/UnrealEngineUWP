// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeViewRow.h"
#include "SStateTreeView.h"
#include "EditorFontGlyphs.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "StateTree.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
#include "StateTreeViewModel.h"
#include "Widgets/Views/SListView.h"
#include "TextStyleDecorator.h"
#include "Customizations/StateTreeEditorNodeUtils.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Editor
{
	FLinearColor LerpColorSRGB(const FLinearColor ColorA, FLinearColor ColorB, float T)
	{
		const FColor A = ColorA.ToFColorSRGB();
		const FColor B = ColorB.ToFColorSRGB();
		return FLinearColor(FColor(
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.R) * (1.f - T) + static_cast<float>(B.R) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.G) * (1.f - T) + static_cast<float>(B.G) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.B) * (1.f - T) + static_cast<float>(B.B) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.A) * (1.f - T) + static_cast<float>(B.A) * T))));
	}
} // UE:StateTree::Editor

void SStateTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TWeakObjectPtr<UStateTreeState> InState, const TSharedPtr<SScrollBox>& ViewBox, TSharedPtr<FStateTreeViewModel> InStateTreeViewModel)
{
	StateTreeViewModel = InStateTreeViewModel;
	WeakState = InState;
	const UStateTreeState* State = InState.Get();
	WeakEditorData = State != nullptr ? State->GetTypedOuter<UStateTreeEditorData>() : nullptr;

	ConstructInternal(STableRow::FArguments()
		.Padding(5.f)
		.OnDragDetected(this, &SStateTreeViewRow::HandleDragDetected)
		.OnCanAcceptDrop(this, &SStateTreeViewRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SStateTreeViewRow::HandleAcceptDrop)
		.Style(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("StateTree.Selection"))
		, InOwnerTableView);

	static const FLinearColor LinkBackground = FLinearColor(FColor(84, 84, 84));
	static constexpr FLinearColor IconTint = FLinearColor(1, 1, 1, 0.5f);

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)
		.MinDesiredWidth_Lambda([WeakOwnerViewBox = ViewBox.ToWeakPtr()]()
			{
				// Captured as weak ptr so we don't prevent our parent widget from being destroyed (circular pointer reference).
				if (const TSharedPtr<SScrollBox> OwnerViewBox = WeakOwnerViewBox.Pin())
				{
					// Make the row at least as wide as the view.
					// The -1 is needed or we'll see a scrollbar.
					return OwnerViewBox->GetTickSpaceGeometry().GetLocalSize().X - 1;
				}
				return 0.f;
			})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.ShouldDrawWires(true)
				.IndentAmount(32.f)
				.BaseIndentLevel(0)
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.Padding(FMargin(0.f, 4.f))
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(28.f)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FStateTreeEditorStyle::Get().GetBrush("StateTree.State.Border"))
					.BorderBackgroundColor(this, &SStateTreeViewRow::GetActiveStateColor)
					[
						SNew(SHorizontalBox)

						// Sub tree marker
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(4.f)
							.HeightOverride(28.f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SStateTreeViewRow::GetSubTreeMarkerColor)
							]
						]

						// State Box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(28.f)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(this, &SStateTreeViewRow::GetTitleColor, 1.0f, 0.0f)
								.Padding(FMargin(4.f, 0.f, 12.f, 0.f))
								.IsEnabled_Lambda([InState]
									{
										const UStateTreeState* State = InState.Get();
										return State != nullptr && State->bEnabled;
									})
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SHorizontalBox)

										// Warnings
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
											.Visibility(this, &SStateTreeViewRow::GetWarningsVisibility)
											[
												SNew(SImage)
												.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
												.ToolTipText(this, &SStateTreeViewRow::GetWarningsTooltipText)
											]
										]

										// Conditions icon
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
											.Visibility(this, &SStateTreeViewRow::GetConditionVisibility)
											[
												SNew(SImage)
												.ColorAndOpacity(IconTint)
												.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.StateConditions"))
												.ToolTipText(LOCTEXT("StateHasEnterConditions", "State selection is guarded with enter conditions."))
											]
										]

										// Selector icon
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
											[
												SNew(SImage)
												.Image(this, &SStateTreeViewRow::GetSelectorIcon)
												.ColorAndOpacity(IconTint)
												.ToolTipText(this, &SStateTreeViewRow::GetSelectorTooltip)
											]
										]

										// State Name
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SAssignNew(NameTextBlock, SInlineEditableTextBlock)
											.Style(FStateTreeEditorStyle::Get(), "StateTree.State.TitleInlineEditableText")
											.OnVerifyTextChanged_Lambda([](const FText& NewLabel, FText& OutErrorMessage)
												{
													return !NewLabel.IsEmptyOrWhitespace();
												})
											.OnTextCommitted(this, &SStateTreeViewRow::HandleNodeLabelTextCommitted)
											.Text(this, &SStateTreeViewRow::GetStateDesc)
											.ToolTipText(this, &SStateTreeViewRow::GetStateTypeTooltip)
											.Clipping(EWidgetClipping::ClipToBounds)
											.IsSelected(this, &SStateTreeViewRow::IsStateSelected)
										]

										// State ID
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Visibility_Lambda([]()
											{
												return UE::StateTree::Editor::GbDisplayItemIds ? EVisibility::Visible : EVisibility::Collapsed;
											})
											.Text(this, &SStateTreeViewRow::GetStateIDDesc)
											.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
										]
									]
									+ SOverlay::Slot()
									[
										SNew(SHorizontalBox)

										// State breakpoint box
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Top)
										.HAlign(HAlign_Left)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(-12.f, -6.f, 0.f, 0.f))
											[
												SNew(SImage)
												.DesiredSizeOverride(FVector2D(12.f, 12.f))
												.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
												.Visibility(this, &SStateTreeViewRow::GetStateBreakpointVisibility)
												.ToolTipText(this, &SStateTreeViewRow::GetStateBreakpointTooltipText)
											]
										]
									]
								]
							]
						]
						
						// Linked State box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Fill)
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(28.f)
							.VAlign(VAlign_Fill)
							.Visibility(this, &SStateTreeViewRow::GetLinkedStateVisibility)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
								.BorderBackgroundColor(LinkBackground)
								.Padding(FMargin(6.f, 0.f, 12.f, 0.f))
								[
									// Link icon
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
									[
										SNew(SImage)
										.ColorAndOpacity(IconTint)
										.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.StateLinked"))
									]

									// Linked State
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(STextBlock)
										.Text(this, &SStateTreeViewRow::GetLinkedStateDesc)
										.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Details")
									]
								]
							]
						]
						// Tasks
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Fill)
						.AutoWidth()
						[
							SNew(SBox)
							.VAlign(VAlign_Fill)
							.Visibility(this, &SStateTreeViewRow::GetTasksVisibility)
							[
								CreateTasksWidget()
							]
						]
					]
				]
			]

			// Completed transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(this, &SStateTreeViewRow::GetCompletedTransitionsIcon)
							.ColorAndOpacity(IconTint)
						]
						+ SOverlay::Slot()
						[
							// Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.f, -10.f, 0.f, 0.f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility(this, &SStateTreeViewRow::GetCompletedTransitionBreakpointVisibility)
									.ToolTipText_Lambda([this]
										{
											return FText::Format(LOCTEXT("TransitionBreakpointTooltip","Break when executing transition: {0}"),
												GetCompletedTransitionWithBreakpointDesc());
										})
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
					[
						SNew(SRichTextBlock)
						.Text(this, &SStateTreeViewRow::GetCompletedTransitionsDesc)
						.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
					]
				]
			]

			// Succeeded transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Succeeded"))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(this, &SStateTreeViewRow::GetSucceededTransitionIcon)
							.ColorAndOpacity(IconTint)
						]
						+ SOverlay::Slot()
						[
							// Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.f, -10.f, 0.f, 0.f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility(this, &SStateTreeViewRow::GetSucceededTransitionBreakpointVisibility)
									.ToolTipText_Lambda([this]
										{
											return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
												GetSucceededTransitionWithBreakpointDesc());
										})
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
					[
						SNew(SRichTextBlock)
						.Text(this, &SStateTreeViewRow::GetSucceededTransitionDesc)
						.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
					]
				]
			]

			// Failed transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.Visibility(this, &SStateTreeViewRow::GetFailedTransitionVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Failed"))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(this, &SStateTreeViewRow::GetFailedTransitionIcon)
							.ColorAndOpacity(IconTint)
						]
						+ SOverlay::Slot()
						[
							// Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.f, -10.f, 0.f, 0.f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility(this, &SStateTreeViewRow::GetFailedTransitionBreakpointVisibility)
									.ToolTipText_Lambda([this]
										{
											return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
												GetFailedTransitionWithBreakpointDesc());
										})
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
					[
						SNew(SRichTextBlock)
						.Text(this, &SStateTreeViewRow::GetFailedTransitionDesc)
						.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
					]
				]
			]

			// Transitions
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Condition"))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Goto"))
							.ColorAndOpacity(IconTint)
						]
						+ SOverlay::Slot()
						[
							// Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.f, -10.f, 0.f, 0.f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility(this, &SStateTreeViewRow::GetConditionalTransitionsBreakpointVisibility)
									.ToolTipText_Lambda([this]
										{
											return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
												GetConditionalTransitionsWithBreakpointDesc());
										})
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
					[
						SNew(SRichTextBlock)
						.Text(this, &SStateTreeViewRow::GetConditionalTransitionsDesc)
						.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
						+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
					]
				]
			]
		]
	];
}

TSharedRef<SHorizontalBox> SStateTreeViewRow::CreateTasksWidget()
{
	const TSharedRef<SHorizontalBox> TasksBox = SNew(SHorizontalBox);
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();
	const UStateTreeState* State = WeakState.Get();
	
	if (!EditorData || !State || State->Tasks.IsEmpty())
	{
		return TasksBox;
	}

	const int32 NumTasks = State->Tasks.Num();

	// The task descriptions can get long. Make some effort to limit how long they can get. 
	constexpr float ReferenceWidth = 1000.0f;
	const float MaxTaskWidth = FMath::RoundToFloat(FMath::Max(150.0f, ReferenceWidth / static_cast<float>(FMath::Max(1, NumTasks))));

	for (int32 TaskIndex = 0; TaskIndex < NumTasks; TaskIndex++)
	{
		const FStateTreeEditorNode& TaskNode = State->Tasks[TaskIndex];
		if (const FStateTreeTaskBase* Task = TaskNode.Node.GetPtr<FStateTreeTaskBase>())
		{
			FGuid TaskId = State->Tasks[TaskIndex].ID;
			auto IsTaskEnabledFunc = [WeakState=WeakState, TaskIndex]
				{
					const UStateTreeState* State = WeakState.Get();
					if (State != nullptr && State->Tasks.IsValidIndex(TaskIndex))
					{
						if (const FStateTreeTaskBase* Task = State->Tasks[TaskIndex].Node.GetPtr<FStateTreeTaskBase>())
						{
							return (State->bEnabled && Task->bTaskEnabled);
						}
					}
					return true;
				};

			auto IsTaskBreakpointEnabledFunc = [WeakEditorData = WeakEditorData, TaskId]
				{
#if WITH_STATETREE_TRACE_DEBUGGER
					const UStateTreeEditorData* EditorData = WeakEditorData.Get();
					if (EditorData != nullptr && EditorData->HasAnyBreakpoint(TaskId))
					{
						return EVisibility::Visible;
					}
#endif // WITH_STATETREE_TRACE_DEBUGGER
					return EVisibility::Hidden;
				};
			
			auto GetTaskBreakpointTooltipFunc = [WeakEditorData = WeakEditorData, TaskId]
				{
#if WITH_STATETREE_TRACE_DEBUGGER
					if (const UStateTreeEditorData* EditorData = WeakEditorData.Get())
					{
						const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(TaskId, EStateTreeBreakpointType::OnEnter);
						const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(TaskId, EStateTreeBreakpointType::OnExit);
						if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting task");
						}

						if (bHasBreakpointOnEnter)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnEnterTooltip","Break when entering task");
						}

						if (bHasBreakpointOnExit)
						{
							return LOCTEXT("StateTreeTaskBreakpointOnExitTooltip","Break when exiting task");
						}
					}
#endif // WITH_STATETREE_TRACE_DEBUGGER
					return FText::GetEmpty();
				};

			TasksBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(this, &SStateTreeViewRow::GetTitleColor, 0.25f, 0.25f)
					.Padding(0)
					.IsEnabled_Lambda(IsTaskEnabledFunc)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SBox)
							.MaxDesiredWidth(MaxTaskWidth)
							.Padding(FMargin(6.f, 0.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Left)
								.AutoWidth()
								[
									SNew(SBox)
									.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
									.Visibility(this, &SStateTreeViewRow::GetTaskIconVisibility, TaskId)
									[
										SNew(SImage)
										.Image(this, &SStateTreeViewRow::GetTaskIcon, TaskId)
										.ColorAndOpacity(this, &SStateTreeViewRow::GetTaskIconColor, TaskId)
									]
								]

								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Left)
								.AutoWidth()
								[
									SNew(SRichTextBlock)
									.Text(this, &SStateTreeViewRow::GetTaskDesc, TaskId, EStateTreeNodeFormatting::RichText)
									.ToolTipText(this, &SStateTreeViewRow::GetTaskDesc, TaskId, EStateTreeNodeFormatting::Text)
									.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("StateTree.Task.Title"))
									.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("StateTree.Task.Title")))
									+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("StateTree.Task.Title.Bold")))
									+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("StateTree.Task.Title.Subdued")))
								]
							]
						]
						+ SOverlay::Slot()
						[
							// Task Breakpoint box
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SBox)
								.Padding(FMargin(0.0f, -10.0f, 0.0f, 0.0f))
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(10.f, 10.f))
									.Image(FStateTreeEditorStyle::Get().GetBrush(TEXT("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid")))
									.Visibility_Lambda(IsTaskBreakpointEnabledFunc)
									.ToolTipText_Lambda(GetTaskBreakpointTooltipFunc)
								]
							]
						]
					]
				];
		}
	}

	return TasksBox;
}

void SStateTreeViewRow::RequestRename() const
{
	if (NameTextBlock)
	{
		NameTextBlock->EnterEditingMode();
	}
}

FSlateColor SStateTreeViewRow::GetTitleColor(const float Alpha, const float Lighten) const
{
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();

	FLinearColor Color(FColor(31, 151, 167));
	
	if (State != nullptr && EditorData != nullptr)
	{
		if (const FStateTreeEditorColor* FoundColor = EditorData->FindColor(State->ColorRef))
		{
			if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
			{
				Color = UE::StateTree::Editor::LerpColorSRGB(FoundColor->Color, FColor::Black, 0.25f);
			}
			Color = FoundColor->Color;
		}
	}

	if (Lighten > 0.0f)
	{
		Color = UE::StateTree::Editor::LerpColorSRGB(Color, FColor::White, Lighten);
	}
	
	return Color.CopyWithNewOpacity(Alpha);
}

FSlateColor SStateTreeViewRow::GetActiveStateColor() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (StateTreeViewModel && StateTreeViewModel->IsStateActiveInDebugger(*State))
		{
			return FLinearColor::Yellow;
		}
		if (StateTreeViewModel && StateTreeViewModel->IsSelected(State))
		{
			return FLinearColor(FColor(236, 134, 39));
		}
	}

	return FLinearColor::Transparent;
}

FSlateColor SStateTreeViewRow::GetSubTreeMarkerColor() const
{
	// Show color for subtree.
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (IsRootState() || State->Type == EStateTreeStateType::Subtree)
		{
			const FSlateColor TitleColor = GetTitleColor();
			return UE::StateTree::Editor::LerpColorSRGB(TitleColor.GetSpecifiedColor(), FLinearColor::White, 0.2f);
		}
	}

	return GetTitleColor();
}

FText SStateTreeViewRow::GetStateDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return FText::FromName(State->Name);
	}
	return FText::FromName(FName());
}

FText SStateTreeViewRow::GetStateIDDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return FText::FromString(*LexToString(State->ID));
	}
	return FText::FromName(FName());
}

EVisibility SStateTreeViewRow::GetConditionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return State->EnterConditions.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetStateBreakpointVisibility() const
{
#if WITH_STATETREE_TRACE_DEBUGGER
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		return (EditorData != nullptr && EditorData->HasAnyBreakpoint(State->ID)) ? EVisibility::Visible : EVisibility::Hidden;
	}
#endif // WITH_STATETREE_TRACE_DEBUGGER
	return EVisibility::Hidden;
}

FText SStateTreeViewRow::GetStateBreakpointTooltipText() const
{
#if WITH_STATETREE_TRACE_DEBUGGER
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(State->ID, EStateTreeBreakpointType::OnEnter);
		const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(State->ID, EStateTreeBreakpointType::OnExit);

		if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
		{
			return LOCTEXT("StateTreeStateBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting state");
		}

		if (bHasBreakpointOnEnter)
		{
			return LOCTEXT("StateTreeStateBreakpointOnEnterTooltip","Break when entering state");
		}

		if (bHasBreakpointOnExit)
		{
			return LOCTEXT("StateTreeStateBreakpointOnExitTooltip","Break when exiting state");
		}
	}
#endif // WITH_STATETREE_TRACE_DEBUGGER
	return FText::GetEmpty();
}

const FSlateBrush* SStateTreeViewRow::GetSelectorIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.SelectNone");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryEnterState)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder)
		{
			if (State->Children.IsEmpty()
				|| State->Type == EStateTreeStateType::Linked
				|| State->Type == EStateTreeStateType::LinkedAsset)
			{
				// Backwards compatible behavior
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");
			}
			else
			{
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectChildrenInOrder");
			}
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenAtUniformRandom)
		{
			if (State->Children.IsEmpty()
				|| State->Type == EStateTreeStateType::Linked
				|| State->Type == EStateTreeStateType::LinkedAsset)
			{
				// Backwards compatible behavior
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");
			}

			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectCHildrenAtRandom");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility)
		{
			if (State->Children.IsEmpty()
				|| State->Type == EStateTreeStateType::Linked
				|| State->Type == EStateTreeStateType::LinkedAsset)
			{
				// Backwards compatible behavior
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");
			}

			//place holder
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectChildrenInOrder");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenBasedOnRelativeUtility)
		{
			if (State->Children.IsEmpty()
				|| State->Type == EStateTreeStateType::Linked
				|| State->Type == EStateTreeStateType::LinkedAsset)
			{
				// Backwards compatible behavior
				return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");
			}

			//place holder
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectChildrenInOrder");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions)
		{
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryFollowTransitions");
		}
	}

	return nullptr;
}

FText SStateTreeViewRow::GetSelectorTooltip() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EStateTreeStateSelectionBehavior>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->SelectionBehavior);
		
		switch (State->SelectionBehavior)
		{
			case EStateTreeStateSelectionBehavior::None:
			case EStateTreeStateSelectionBehavior::TryEnterState:
			case EStateTreeStateSelectionBehavior::TryFollowTransitions:
				return Enum->GetToolTipTextByIndex(Index);
			case EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder:
			case EStateTreeStateSelectionBehavior::TrySelectChildrenAtUniformRandom:
			case EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility:
			case EStateTreeStateSelectionBehavior::TrySelectChildrenBasedOnRelativeUtility:
				if (State->Children.IsEmpty()
					|| State->Type == EStateTreeStateType::Linked
					|| State->Type == EStateTreeStateType::LinkedAsset)
				{
					const int32 EnterStateIndex = Enum->GetIndexByValue((int64)EStateTreeStateSelectionBehavior::TryEnterState);
					return FText::Format(LOCTEXT("ConvertedToEnterState", "{0}\nAutomatically converted from '{1}' because the State has no child States."),
						Enum->GetToolTipTextByIndex(EnterStateIndex), UEnum::GetDisplayValueAsText(State->SelectionBehavior));
				}
				else
				{
					return Enum->GetToolTipTextByIndex(Index);
				}
			default:
				check(false);
		}
	}

	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetStateTypeTooltip() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EStateTreeStateType>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->Type);
		return Enum->GetToolTipTextByIndex(Index);
	}

	return FText::GetEmpty();
}

const FStateTreeEditorNode* SStateTreeViewRow::GetTaskNodeByID(FGuid TaskID) const
{
	const UStateTreeState* State = WeakState.Get();
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();
	if (EditorData != nullptr
		&& State != nullptr)
	{
		return State->Tasks.FindByPredicate([&TaskID](const FStateTreeEditorNode& Node)
		{
			return Node.ID == TaskID;
		});
	}
	return nullptr;
}

EVisibility SStateTreeViewRow::GetTaskIconVisibility(FGuid TaskID) const
{
	bool bHasIcon = false;
	if (const FStateTreeEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FStateTreeNodeBase* BaseNode = TaskNode->Node.GetPtr<const FStateTreeNodeBase>())
		{
			bHasIcon = !BaseNode->GetIconName().IsNone();
		}
	}
	return bHasIcon ? EVisibility::Visible : EVisibility::Collapsed;  	
}

const FSlateBrush* SStateTreeViewRow::GetTaskIcon(FGuid TaskID) const
{
	if (const FStateTreeEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FStateTreeNodeBase* BaseNode = TaskNode->Node.GetPtr<const FStateTreeNodeBase>())
		{
			return UE::StateTreeEditor::EditorNodeUtils::ParseIcon(BaseNode->GetIconName()).GetIcon();
		}
	}
	return nullptr;	
}

FSlateColor SStateTreeViewRow::GetTaskIconColor(FGuid TaskID) const
{
	if (const FStateTreeEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FStateTreeNodeBase* BaseNode = TaskNode->Node.GetPtr<const FStateTreeNodeBase>())
		{
			return FLinearColor(BaseNode->GetIconColor());
		}
	}
	return FSlateColor::UseForeground();
}

FText SStateTreeViewRow::GetTaskDesc(FGuid TaskID, EStateTreeNodeFormatting Formatting) const
{
	FText TaskName;
	if (const UStateTreeEditorData* EditorData = WeakEditorData.Get())
	{
		if (const FStateTreeEditorNode* TaskNode = GetTaskNodeByID(TaskID))
		{
			if (UE::StateTree::Editor::GbDisplayItemIds)
			{
				TaskName = FText::Format(LOCTEXT("TaskNameWithID", "{0} ({1})"), EditorData->GetNodeDescription(*TaskNode, Formatting), FText::AsCultureInvariant(*LexToString(TaskID)));
			}
			else
			{
				TaskName = EditorData->GetNodeDescription(*TaskNode, Formatting);
			}
		}
	}
	return TaskName;
}

EVisibility SStateTreeViewRow::GetTasksVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		int32 ValidCount = 0;
		for (int32 i = 0; i < State->Tasks.Num(); i++)
		{
			if (State->Tasks[i].Node.GetPtr<FStateTreeTaskBase>())
			{
				ValidCount++;
			}
		}
		return ValidCount > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetLinkedStateVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return (State->Type == EStateTreeStateType::Linked || State->Type == EStateTreeStateType::LinkedAsset) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

bool SStateTreeViewRow::GetStateWarnings(FText* OutText) const
{
	bool bHasWarnings = false;
	
	const UStateTreeState* State = WeakState.Get();
	if (!State)
	{
		return bHasWarnings;
	}

	// Linked States cannot have children.
	if ((State->Type == EStateTreeStateType::Linked
		|| State->Type == EStateTreeStateType::LinkedAsset)
		&& State->Children.Num() > 0)
	{
		if (OutText)
		{
			*OutText = LOCTEXT("LinkedStateChildWarning", "Linked State cannot have child states, because the state selection will enter to the linked state on activation.");
		}
		bHasWarnings = true;
	}

	// Child states should not have any considerations if their parent doesn't use utility
	if (State->Considerations.Num() != 0)
	{
		if (!State->Parent 
			|| (State->Parent->SelectionBehavior != EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility
				&& State->Parent->SelectionBehavior != EStateTreeStateSelectionBehavior::TrySelectChildrenBasedOnRelativeUtility))
		{
			if (OutText)
			{
				*OutText = LOCTEXT("ChildStateUtilityConsiderationWarning", 
					"State has Utility Considerations but they don't have effect."
					"The Utility Considerations are used only when parent State's Selection Behavior is:"
					"\"Try Select Children with Highest Utility\" or \"Try Select Children Based on Relative Utility.");
			}
			bHasWarnings = true;
		}
	}

	return bHasWarnings;
}

FText SStateTreeViewRow::GetLinkedStateDesc() const
{
	const UStateTreeState* State = WeakState.Get();
	if (!State)
	{
		return FText::GetEmpty();
	}

	if (State->Type == EStateTreeStateType::Linked)
	{
		return FText::FromName(State->LinkedSubtree.Name);
	}
	else if (State->Type == EStateTreeStateType::LinkedAsset)
	{
		return FText::FromString(GetNameSafe(State->LinkedAsset.Get()));
	}
	
	return FText::GetEmpty();
}

EVisibility SStateTreeViewRow::GetWarningsVisibility() const
{
	return GetStateWarnings(nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetWarningsTooltipText() const
{
	FText Warnings = FText::GetEmpty();
	GetStateWarnings(&Warnings);
	return Warnings;
}

bool SStateTreeViewRow::HasParentTransitionForTrigger(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
	EStateTreeTransitionTrigger CombinedTrigger = EStateTreeTransitionTrigger::None;
	for (const UStateTreeState* ParentState = State.Parent; ParentState != nullptr; ParentState = ParentState->Parent)
	{
		for (const FStateTreeTransition& Transition : ParentState->Transitions)
		{
			CombinedTrigger |= Transition.Trigger;
		}
	}
	return EnumHasAllFlags(CombinedTrigger, Trigger);
}


FText SStateTreeViewRow::GetLinkDescription(const FStateTreeStateLink& Link)
{
	switch (Link.LinkType)
	{
	case EStateTreeTransitionType::None:
		return LOCTEXT("TransitionNoneStyled", "<i>None</>");
		break;
	case EStateTreeTransitionType::Succeeded:
		return LOCTEXT("TransitionTreeSucceededStyled", "<i>Succeeded</>");
		break;
	case EStateTreeTransitionType::Failed:
		return LOCTEXT("TransitionTreeFailedStyled", "<i>Failed</>");
		break;
	case EStateTreeTransitionType::NextState:
		return LOCTEXT("TransitionNextStateStyled", "<i>Next</>");
		break;
	case EStateTreeTransitionType::NextSelectableState:
		return LOCTEXT("TransitionNextSelectableStateStyled", "<i>Next Selectable</s>");
		break;
	case EStateTreeTransitionType::GotoState:
		return FText::FromName(Link.Name);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled transition type."));
		break;
	}

	return FText::GetEmpty();
};

bool SStateTreeViewRow::IsLeafState() const
{
	const UStateTreeState* State = WeakState.Get();
	return State
		&& State->Children.Num() == 0
		&& !IsRootState()
		&& (State->Type == EStateTreeStateType::State
			|| State->Type == EStateTreeStateType::Linked
			|| State->Type == EStateTreeStateType::LinkedAsset);
}

FText SStateTreeViewRow::GetTransitionsDesc(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions) const
{
	TArray<FText> DescItems;
	const UStateTreeEditorData* TreeEditorData = WeakEditorData.Get();

	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_STATETREE_TRACE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = TreeEditorData != nullptr && TreeEditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_STATETREE_TRACE_DEBUGGER

		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			DescItems.Add(GetLinkDescription(Transition.State));
		}
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent))
	{
		auto AddLinksFromStruct = [&DescItems](FStateTreeDataView Struct)
		{
			if (!Struct.IsValid())
			{
				return;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FStateTreeStateLink>::Get())
				{
					const FStateTreeStateLink& Link = *static_cast<const FStateTreeStateLink*>(It.Value());
					if (Link.LinkType != EStateTreeTransitionType::None)
					{
						DescItems.Add(GetLinkDescription(Link));
					}
				}
			}
		};
		
		for (const FStateTreeEditorNode& Task : State.Tasks)
		{
			AddLinksFromStruct(FStateTreeDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())));
			AddLinksFromStruct(Task.GetInstance());
		}

		AddLinksFromStruct(FStateTreeDataView(State.SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State.SingleTask.Node.GetMemory())));
		AddLinksFromStruct(State.SingleTask.GetInstance());
	}

	if (IsLeafState()
		&& DescItems.Num() == 0
		&& EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		if (HasParentTransitionForTrigger(State, Trigger))
		{
			DescItems.Add(LOCTEXT("TransitionActionHandleInParentStyled", "<i>Parent</>"));
		}
		else
		{
			DescItems.Add(LOCTEXT("TransitionActionRoot", "<i>Root</>"));
		}
	}
	
	return FText::Join(FText::FromString(TEXT(", ")), DescItems);
}

const FSlateBrush* SStateTreeViewRow::GetTransitionsIcon(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions) const
{
	enum EIconType
	{
		IconNone = 0,
		IconGoto =	1 << 0,
		IconNext =		1 << 1,
		IconParent =		1 << 2,
	};
	uint8 IconType = IconNone;
	
	const UStateTreeEditorData* EditorData = WeakEditorData.Get();
	
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_STATETREE_TRACE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = EditorData != nullptr && EditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_STATETREE_TRACE_DEBUGGER
		
		// The icons here depict "transition direction", not the type specifically.
		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			switch (Transition.State.LinkType)
			{
			case EStateTreeTransitionType::None:
				IconType |= IconGoto;
				break;
			case EStateTreeTransitionType::Succeeded:
				IconType |= IconGoto;
				break;
			case EStateTreeTransitionType::Failed:
				IconType |= IconGoto;
				break;
			case EStateTreeTransitionType::NextState:
			case EStateTreeTransitionType::NextSelectableState:
				IconType |= IconNext;
				break;
			case EStateTreeTransitionType::GotoState:
				IconType |= IconGoto;
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled transition type."));
				break;
			}
		}
	}

	if (FMath::CountBits(static_cast<uint64>(IconType)) > 1)
	{
		// Prune down to just one icon.
		IconType = IconGoto;
	}

	if (IsLeafState()
		&& IconType == IconNone
		&& EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		// Transition is handled on parent state, or implicit Root.
		IconType = IconParent;
	}

	switch (IconType)
	{
		case IconGoto:
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Goto");
		case IconNext:
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Next");
		case IconParent:
			return FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Parent");
		default:
			break;
	}
	
	return nullptr;
}

EVisibility SStateTreeViewRow::GetTransitionsVisibility(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
	// Handle completed, succeeded and failed transitions.
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
	{
		EStateTreeTransitionTrigger HandledTriggers = EStateTreeTransitionTrigger::None;
		bool bExactMatch = false;

		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			// Skip disabled transitions
			if (Transition.bTransitionEnabled == false)
			{
				continue;
			}

			HandledTriggers |= Transition.Trigger;
			bExactMatch |= (Transition.Trigger == Trigger);

			if (bExactMatch)
			{
				break;
			}
		}

		// Assume that leaf states should have completion transitions.
		if (!bExactMatch && IsLeafState())
		{
			// Find the missing transition type, note: Completed = Succeeded|Failed.
			const EStateTreeTransitionTrigger MissingTriggers = HandledTriggers ^ EStateTreeTransitionTrigger::OnStateCompleted;
			return MissingTriggers == Trigger ? EVisibility::Visible : EVisibility::Collapsed;
		}
		
		return bExactMatch ? EVisibility::Visible : EVisibility::Collapsed;
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent))
	{
		auto HasAnyLinksInStruct = [](FStateTreeDataView Struct) -> bool
		{
			if (!Struct.IsValid())
			{
				return false;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FStateTreeStateLink>::Get())
				{
					const FStateTreeStateLink& Link = *static_cast<const FStateTreeStateLink*>(It.Value());
					if (Link.LinkType != EStateTreeTransitionType::None)
					{
						return true;
					}
				}
			}
			return false;
		};
		
		for (const FStateTreeEditorNode& Task : State.Tasks)
		{
			if (HasAnyLinksInStruct(FStateTreeDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())))
				|| HasAnyLinksInStruct(Task.GetInstance()))
			{
				return EVisibility::Visible;
			}
		}

		if (HasAnyLinksInStruct(FStateTreeDataView(State.SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State.SingleTask.Node.GetMemory())))
			|| HasAnyLinksInStruct(State.SingleTask.GetInstance()))
		{
			return EVisibility::Visible;
		}
	}
	
	// Handle the test
	for (const FStateTreeTransition& Transition : State.Transitions)
	{
		// Skip disabled transitions
		if (Transition.bTransitionEnabled == false)
		{
			continue;
		}

		if (EnumHasAnyFlags(Trigger, Transition.Trigger))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetTransitionsBreakpointVisibility(const UStateTreeState& State, const EStateTreeTransitionTrigger Trigger) const
{
#if WITH_STATETREE_TRACE_DEBUGGER
	if (const UStateTreeEditorData* EditorData = WeakEditorData.Get())
	{
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			if (Transition.bTransitionEnabled && EnumHasAnyFlags(Trigger, Transition.Trigger))
			{
				if (EditorData->HasBreakpoint(Transition.ID, EStateTreeBreakpointType::OnTransition))
				{
					return GetTransitionsVisibility(State, Trigger);
				}
			}
		}
	}
#endif // WITH_STATETREE_TRACE_DEBUGGER
	
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetCompletedTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return EVisibility::Visible;
}

EVisibility SStateTreeViewRow::GetCompletedTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return EVisibility::Visible;
}

FText SStateTreeViewRow::GetCompletedTransitionsDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return LOCTEXT("Invalid", "Invalid");
}

FText SStateTreeViewRow::GetCompletedTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateCompleted, FilterOptions);
	}
	return FText::GetEmpty();
}

const FSlateBrush* SStateTreeViewRow::GetCompletedTransitionsIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateCompleted);
	}
	return nullptr;
}

EVisibility SStateTreeViewRow::GetSucceededTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetSucceededTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetSucceededTransitionDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetSucceededTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateSucceeded, FilterOptions);
	}
	return FText::GetEmpty();
}

const FSlateBrush* SStateTreeViewRow::GetSucceededTransitionIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateSucceeded);
	}
	return nullptr;
}

EVisibility SStateTreeViewRow::GetFailedTransitionVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetFailedTransitionBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetFailedTransitionDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return LOCTEXT("Invalid", "Invalid");
}

FText SStateTreeViewRow::GetFailedTransitionWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnStateFailed, FilterOptions);
	}
	return FText::GetEmpty();
}

const FSlateBrush* SStateTreeViewRow::GetFailedTransitionIcon() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsIcon(*State, EStateTreeTransitionTrigger::OnStateFailed);
	}
	return nullptr;
}

EVisibility SStateTreeViewRow::GetConditionalTransitionsVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsVisibility(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent);
	}
	return EVisibility::Collapsed;
}

EVisibility SStateTreeViewRow::GetConditionalTransitionsBreakpointVisibility() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		return GetTransitionsBreakpointVisibility(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent);
	}
	return EVisibility::Collapsed;
}

FText SStateTreeViewRow::GetConditionalTransitionsDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.bUseMask = true;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent, FilterOptions);
	}
	return FText::GetEmpty();
}

FText SStateTreeViewRow::GetConditionalTransitionsWithBreakpointDesc() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;
		FilterOptions.bUseMask = true;
		return GetTransitionsDesc(*State, EStateTreeTransitionTrigger::OnTick | EStateTreeTransitionTrigger::OnEvent, FilterOptions);
	}
	return FText::GetEmpty();
}

bool SStateTreeViewRow::IsRootState() const
{
	// Routines can be identified by not having parent state.
	const UStateTreeState* State = WeakState.Get();
	return State ? State->Parent == nullptr : false;
}

bool SStateTreeViewRow::IsStateSelected() const
{
	if (const UStateTreeState* State = WeakState.Get())
	{
		if (StateTreeViewModel)
		{
			return StateTreeViewModel->IsSelected(State);
		}
	}
	return false;
}

void SStateTreeViewRow::HandleNodeLabelTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const
{
	if (StateTreeViewModel)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			StateTreeViewModel->RenameState(State, FName(*FText::TrimPrecedingAndTrailing(NewLabel).ToString()));
		}
	}
}

FReply SStateTreeViewRow::HandleDragDetected(const FGeometry&, const FPointerEvent&) const
{
	return FReply::Handled().BeginDragDrop(FActionTreeViewDragDrop::New(WeakState.Get()));
}

TOptional<EItemDropZone> SStateTreeViewRow::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeState> TargetState) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		// Cannot drop on selection or child of selection.
		if (StateTreeViewModel && StateTreeViewModel->IsChildOfSelection(TargetState.Get()))
		{
			return TOptional<EItemDropZone>();
		}

		return DropZone;
	}

	return TOptional<EItemDropZone>();
}

FReply SStateTreeViewRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UStateTreeState> TargetState) const
{
	const TSharedPtr<FActionTreeViewDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FActionTreeViewDragDrop>();
	if (DragDropOperation.IsValid())
	{
		if (StateTreeViewModel)
		{
			if (DropZone == EItemDropZone::AboveItem)
			{
				StateTreeViewModel->MoveSelectedStatesBefore(TargetState.Get());
			}
			else if (DropZone == EItemDropZone::BelowItem)
			{
				StateTreeViewModel->MoveSelectedStatesAfter(TargetState.Get());
			}
			else
			{
				StateTreeViewModel->MoveSelectedStatesInto(TargetState.Get());
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
