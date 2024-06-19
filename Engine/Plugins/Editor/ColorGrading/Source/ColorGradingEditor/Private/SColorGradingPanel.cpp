// Copyright Epic Games, Inc. All Rights Reserved.

#include "SColorGradingPanel.h"

#include "ColorGradingCommands.h"
#include "ColorGradingEditorDataModel.h"
#include "IColorGradingEditor.h"
#include "SColorGradingColorWheelPanel.h"
#include "SColorGradingObjectList.h"

#include "ColorCorrectRegion.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "ColorGradingEditor"

SColorGradingPanel::~SColorGradingPanel()
{
	for (const FColorGradingListItemRef& ColorGradingItem : ColorGradingItemList)
	{
		if (ColorGradingItem->Component.IsValid())
		{
			UnbindBlueprintCompiledDelegate(ColorGradingItem->Component->GetClass());
		}

		if (ColorGradingItem->Actor.IsValid())
		{
			UnbindBlueprintCompiledDelegate(ColorGradingItem->Actor->GetClass());
		}
	}

	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);

	if (GEngine)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SColorGradingPanel::Construct(const FArguments& InArgs)
{
	bIsInDrawer = InArgs._IsInDrawer;
	DockCallback = InArgs._OnDocked;
	OverrideWorld = InArgs._OverrideWorld;
	ActorFilter = InArgs._ActorFilter;

	ColorGradingDataModel = MakeShared<FColorGradingEditorDataModel>();
	ColorGradingDataModel->OnDataModelGenerated().AddSP(this, &SColorGradingPanel::OnColorGradingDataModelGenerated);

	FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, &SColorGradingPanel::OnObjectsReplaced);
	GEngine->OnLevelActorAdded().AddSP(this, &SColorGradingPanel::OnLevelActorAdded);
	GEngine->OnLevelActorDeleted().AddSP(this, &SColorGradingPanel::OnLevelActorDeleted);
	GEngine->OnWorldAdded().AddSP(this, &SColorGradingPanel::OnWorldAdded);
	GEngine->OnWorldDestroyed().AddSP(this, &SColorGradingPanel::OnWorldDestroyed);

	GEditor->RegisterForUndo(this);

	RefreshColorGradingList();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.0f, 0.0f))
		[
			// Splitter to divide the object list and the color panel
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.0f)

			// Splitter slot for object list
			+SSplitter::Slot()
			.Value(0.12f)
			[
				SNew(SBox)
				.Padding(FMargin(4.f))
				[
					SNew(SBorder)
					.Padding(FMargin(0.0f))
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					[
						SNew(SScrollBox)
						+SScrollBox::Slot()
						[
							SNew(SExpandableArea)
							.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
							.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
							.HeaderPadding(FMargin(4.0f, 2.0f))
							.InitiallyCollapsed(false)
							.AllowAnimatedTransition(false)
							.Visibility_Lambda([this]() { return ColorGradingItemList.Num() ? EVisibility::Visible : EVisibility::Collapsed; })
							.HeaderContent()
							[
								SNew(SBox)
								.HeightOverride(24.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("ColorGradingObjectListLabel", "Objects"))
										.TextStyle(FAppStyle::Get(), "ButtonText")
										.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
								]
							]
							.BodyContent()
							[
								SAssignNew(ColorGradingObjectListView, SColorGradingObjectList)
								.ColorGradingItemsSource(&ColorGradingItemList)
								.OnSelectionChanged(this, &SColorGradingPanel::OnListSelectionChanged)
							]
						]
					]
				]
			]

			// Splitter slot for color grading controls/details
			+SSplitter::Slot()
			.Value(0.88f)
			[
				SNew(SVerticalBox)

				// Toolbar slot for the main drawer toolbar
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 0)
				[
					SNew(SBorder)
					.Padding(FMargin(3))
					.BorderImage(bIsInDrawer ? FStyleDefaults::GetNoBrush() : FAppStyle::Get().GetBrush("Brushes.Panel"))
					[
						SNew(SBox)
						.HeightOverride(28.0f)
						[
							SNew(SHorizontalBox)

							// Slot for the color grading group toolbar
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SAssignNew(ColorGradingGroupToolBarBox, SHorizontalBox)
								.Visibility(this, &SColorGradingPanel::GetColorGradingGroupToolBarVisibility)
							]


							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SSpacer)
							]

							// Slot for the "Dock in Layout" button
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								CreateDockInLayoutButton()
							]
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.Thickness(2.0f)
				]

				// Slot for the color panel
				+SVerticalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
					.Padding(FMargin(2.0f, 2.0f, 2.0f, 0.0f))
					[
						SAssignNew(ColorWheelPanel, SColorGradingColorWheelPanel)
						.ColorGradingDataModelSource(ColorGradingDataModel)
					]
				]
			]
		]
	];
}

void SColorGradingPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bRefreshOnNextTick)
	{
		Refresh();

		bRefreshOnNextTick = false;
	}
}

void SColorGradingPanel::Refresh()
{
	FColorGradingPanelState PanelState;
	GetPanelState(PanelState);

	ColorGradingDataModel->Reset();

	RefreshColorGradingList();

	if (ColorWheelPanel.IsValid())
	{
		ColorWheelPanel->Refresh();
	}

	SetPanelState(PanelState);
}

void SColorGradingPanel::GetPanelState(FColorGradingPanelState& OutPanelState) const
{
	ColorGradingDataModel->GetPanelState(OutPanelState);

	if (ColorWheelPanel.IsValid())
	{
		ColorWheelPanel->GetPanelState(OutPanelState);
	}

	if (ColorGradingObjectListView.IsValid())
	{
		TArray<FColorGradingListItemRef> SelectedItems = ColorGradingObjectListView->GetSelectedItems();

		for (const FColorGradingListItemRef& SelectedItem : SelectedItems)
		{
			if (SelectedItem.IsValid())
			{
				if (SelectedItem->Component.IsValid())
				{
					OutPanelState.SelectedObjects.Add(SelectedItem->Component);
				}
				else if (SelectedItem->Actor.IsValid())
				{
					OutPanelState.SelectedObjects.Add(SelectedItem->Actor);
				}
			}
		}
	}
}

void SColorGradingPanel::SetPanelState(const FColorGradingPanelState& InPanelState)
{
	TArray<FColorGradingListItemRef> ItemsToSelect;

	for (const TWeakObjectPtr<UObject>& SelectedObject : InPanelState.SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			auto FindColorGradingItem = [&SelectedObject](const FColorGradingListItemRef& ColorGradingItem)
			{
				if (SelectedObject->IsA<AActor>())
				{
					return ColorGradingItem->Actor == SelectedObject && ColorGradingItem->Component == nullptr;
				}

				return ColorGradingItem->Actor == SelectedObject || ColorGradingItem->Component == SelectedObject;
			};

			if (FColorGradingListItemRef* FoundItem = ColorGradingItemList.FindByPredicate(FindColorGradingItem))
			{
				ItemsToSelect.Add(*FoundItem);
				break;
			}
		}
	}

	if (!ItemsToSelect.IsEmpty() && ColorGradingObjectListView.IsValid())
	{
		ColorGradingObjectListView->SetSelectedItems(ItemsToSelect);
	}

	ColorGradingDataModel->SetPanelState(InPanelState);

	if (ColorWheelPanel.IsValid())
	{
		ColorWheelPanel->SetPanelState(InPanelState);
	}
}

void SColorGradingPanel::SetSelectedObjects(const TArray<UObject*>& Objects)
{
	ColorGradingDataModel->SetObjects(Objects);

	FColorGradingPanelState PanelState;
	GetPanelState(PanelState);

	PanelState.SelectedObjects.Empty(Objects.Num());
	PanelState.SelectedObjects.Append(Objects);

	SetPanelState(PanelState);
}

TSharedRef<SWidget> SColorGradingPanel::CreateDockInLayoutButton()
{
	if (bIsInDrawer && DockCallback.IsBound())
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("DockInLayout_Tooltip", "Docks this panel in the current window, copying all settings from the drawer.\nThe drawer will still be usable."))
			.OnClicked(this, &SColorGradingPanel::DockInLayout)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.Layout"))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DockInLayout", "Dock in Layout"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	return SNullWidget::NullWidget;
}

UWorld* SColorGradingPanel::GetWorld()
{
	if (OverrideWorld.IsSet() && OverrideWorld.Get())
	{
		return OverrideWorld.Get();
	}

	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	if (!LevelEditorSubsystem)
	{
		return nullptr;
	}

	ULevel* Level = LevelEditorSubsystem->GetCurrentLevel();
	if (!Level)
	{
		return nullptr;
	}

	return Level->GetWorld();
}

void SColorGradingPanel::BindBlueprintCompiledDelegate(const UClass* Class)
{
	if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Class))
	{
		if (!Blueprint->OnCompiled().IsBoundToObject(this))
		{
			Blueprint->OnCompiled().AddSP(this, &SColorGradingPanel::OnBlueprintCompiled);
		}
	}
}

void SColorGradingPanel::UnbindBlueprintCompiledDelegate(const UClass* Class)
{
	if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Class))
	{
		Blueprint->OnCompiled().RemoveAll(this);
	}
}

void SColorGradingPanel::RefreshColorGradingList()
{
	for (const FColorGradingListItemRef& Item : ColorGradingItemList)
	{
		if (Item->Component.IsValid())
		{
			UnbindBlueprintCompiledDelegate(Item->Component->GetClass());
		}

		if (Item->Actor.IsValid())
		{
			UnbindBlueprintCompiledDelegate(Item->Actor->GetClass());
		}
	}

	ColorGradingItemList.Empty();

	UWorld* World = GetWorld();
	if (World && ColorGradingDataModel.IsValid())
	{
		// Sorter that sorts the list items alphabetically by their display name
		auto AlphabeticalSort = [](const FColorGradingListItemRef& A, const FColorGradingListItemRef& B)
		{
			if (A.IsValid() && B.IsValid())
			{
				return *A < *B;
			}
			else
			{
				return false;
			}
		};

		// Add all relevant actors
		const TSet<TSubclassOf<AActor>>& ListItemClasses = FColorGradingListItem::GetActorClassesWithListItemGenerators();

		for (TSubclassOf<AActor> ActorClass : ListItemClasses)
		{
			for (TActorIterator<AActor> ActorIter(World, ActorClass.Get()); ActorIter; ++ActorIter)
			{
				AActor* Actor = *ActorIter;

				if (ActorFilter && !ActorFilter(Actor))
				{
					continue;
				}

				TArray<FColorGradingListItemRef> ListItems = FColorGradingListItem::GenerateColorGradingListItems(Actor);

				if (ListItems.IsEmpty())
				{
					continue;
				}

				BindBlueprintCompiledDelegate(Actor->GetClass());
				ColorGradingItemList.Append(ListItems);
			}
		}

		ColorGradingItemList.Sort(AlphabeticalSort);
	}

	if (ColorGradingObjectListView.IsValid())
	{
		ColorGradingObjectListView->RefreshList();
	}
}

void SColorGradingPanel::FillColorGradingGroupToolBar()
{
	if (ColorGradingGroupToolBarBox.IsValid())
	{
		ColorGradingGroupToolBarBox->ClearChildren();
		ColorGradingGroupTextBlocks.Empty(ColorGradingDataModel->ColorGradingGroups.Num());

		for (int32 Index = 0; Index < ColorGradingDataModel->ColorGradingGroups.Num(); ++Index)
		{
			TSharedPtr<SInlineEditableTextBlock> TextBlock = nullptr;

			ColorGradingGroupToolBarBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.OnCheckStateChanged(this, &SColorGradingPanel::OnColorGradingGroupCheckedChanged, Index)
				.IsChecked(this, &SColorGradingPanel::IsColorGradingGroupSelected, Index)
				.OnGetMenuContent(this, &SColorGradingPanel::GetColorGradingGroupMenuContent, Index)
				[
					SNew(SBox)
					.HeightOverride(20.0)
					[
						SAssignNew(TextBlock, SInlineEditableTextBlock)
						.Text(this, &SColorGradingPanel::GetColorGradingGroupDisplayName, Index)
						.Font(this, &SColorGradingPanel::GetColorGradingGroupDisplayNameFont, Index)
						.OnTextCommitted(this, &SColorGradingPanel::OnColorGradingGroupRenamed, Index)
					]
				]
			];

			ColorGradingGroupTextBlocks.Add(TextBlock);
		}

		if (ColorGradingDataModel->ColorGradingGroupToolBarWidget.IsValid())
		{
			ColorGradingGroupToolBarBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				ColorGradingDataModel->ColorGradingGroupToolBarWidget.ToSharedRef()
			];
		}
	}
}

EVisibility SColorGradingPanel::GetColorGradingGroupToolBarVisibility() const
{
	if (ColorGradingDataModel->bShowColorGradingGroupToolBar)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

ECheckBoxState SColorGradingPanel::IsColorGradingGroupSelected(int32 GroupIndex) const
{
	return ColorGradingDataModel->GetSelectedColorGradingGroupIndex() == GroupIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SColorGradingPanel::OnColorGradingGroupCheckedChanged(ECheckBoxState State, int32 GroupIndex)
{
	if (State == ECheckBoxState::Checked)
	{
		ColorGradingDataModel->SetSelectedColorGradingGroup(GroupIndex);
	}
}

FText SColorGradingPanel::GetColorGradingGroupDisplayName(int32 GroupIndex) const
{
	if (ColorGradingDataModel->ColorGradingGroups.IsValidIndex(GroupIndex))
	{
		FText DisplayName = ColorGradingDataModel->ColorGradingGroups[GroupIndex].DisplayName;
		if (DisplayName.IsEmpty())
		{
			return LOCTEXT("ColorGradingGroupEmptyNameLabel", "Unnamed");
		}

		return DisplayName;
	}

	return FText::GetEmpty();
}

FSlateFontInfo SColorGradingPanel::GetColorGradingGroupDisplayNameFont(int32 GroupIndex) const
{
	if (ColorGradingDataModel->ColorGradingGroups.IsValidIndex(GroupIndex))
	{
		FText DisplayName = ColorGradingDataModel->ColorGradingGroups[GroupIndex].DisplayName;
		if (DisplayName.IsEmpty())
		{
			return FAppStyle::Get().GetFontStyle("NormalFontItalic");
		}
	}

	return FAppStyle::Get().GetFontStyle("NormalFont");
}

TSharedRef<SWidget> SColorGradingPanel::GetColorGradingGroupMenuContent(int32 GroupIndex)
{
	if (ColorGradingDataModel->ColorGradingGroups.IsValidIndex(GroupIndex))
	{
		const FColorGradingEditorDataModel::FColorGradingGroup& Group = ColorGradingDataModel->ColorGradingGroups[GroupIndex];

		FMenuBuilder MenuBuilder(true, nullptr);

		const FGenericCommands& GenericCommands = FGenericCommands::Get();

		if (Group.bCanBeRenamed)
		{
			MenuBuilder.AddMenuEntry(
				GenericCommands.Rename->GetLabel(),
				GenericCommands.Rename->GetDescription(),
				GenericCommands.Rename->GetIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SColorGradingPanel::OnColorGradingGroupRequestRename, GroupIndex))
			);
		}

		if (Group.bCanBeDeleted)
		{
			MenuBuilder.AddMenuEntry(
				GenericCommands.Delete->GetLabel(),
				GenericCommands.Delete->GetDescription(),
				GenericCommands.Delete->GetIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SColorGradingPanel::OnColorGradingGroupDeleted, GroupIndex))
			);
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

void SColorGradingPanel::OnColorGradingGroupDeleted(int32 GroupIndex)
{
	// If the group being deleted is in front of the currently selected one, we want to make sure
	// that the same group is selected even after the deletion, so preemptively adjust the
	// currently selected group index
	int32 SelectedGroupIndex = ColorGradingDataModel->GetSelectedColorGradingGroupIndex();
	if (SelectedGroupIndex > GroupIndex)
	{
		ColorGradingDataModel->SetSelectedColorGradingGroup(SelectedGroupIndex - 1);
	}

	ColorGradingDataModel->OnColorGradingGroupDeleted().Broadcast(GroupIndex);
}

void SColorGradingPanel::OnColorGradingGroupRequestRename(int32 GroupIndex)
{
	if (ColorGradingGroupTextBlocks.IsValidIndex(GroupIndex) && ColorGradingGroupTextBlocks[GroupIndex].IsValid())
	{
		ColorGradingGroupTextBlocks[GroupIndex]->EnterEditingMode();
	}
}

void SColorGradingPanel::OnColorGradingGroupRenamed(const FText& InText, ETextCommit::Type TextCommitType, int32 GroupIndex)
{
	ColorGradingDataModel->OnColorGradingGroupRenamed().Broadcast(GroupIndex, InText);
}

void SColorGradingPanel::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	bool bNeedsFullRefresh = false;
	bool bNeedsListRefresh = false;

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = ColorGradingDataModel->GetObjects();

	for (const TPair<UObject*, UObject*>& Pair : OldToNewInstanceMap)
	{
		if (Pair.Key && Pair.Value)
		{
			FColorGradingListItemRef* FoundColorGradingItemPtr = nullptr;

			// Must use GetEvenIfUnreachable on the weak pointers here because most of the time, the objects being replaced have already been marked for GC, and TWeakObjectPtr
			// will return nullptr from Get on GC-marked objects
			FoundColorGradingItemPtr = ColorGradingItemList.FindByPredicate([&Pair](const FColorGradingListItemRef& ColorGradingItem)
			{
				return ColorGradingItem->Actor.GetEvenIfUnreachable() == Pair.Key || ColorGradingItem->Component.GetEvenIfUnreachable() == Pair.Key;
			});

			if (FoundColorGradingItemPtr)
			{
				FColorGradingListItemRef FoundColorGradingItem = *FoundColorGradingItemPtr;
				if (FoundColorGradingItem->Actor.GetEvenIfUnreachable() == Pair.Key)
				{
					FoundColorGradingItem->Actor = Cast<AActor>(Pair.Value);
				}
				else if (FoundColorGradingItem->Component.GetEvenIfUnreachable() == Pair.Key)
				{
					FoundColorGradingItem->Component = Cast<UActorComponent>(Pair.Value);
				}

				bNeedsListRefresh = true;
			}

			if (SelectedObjects.Contains(Pair.Key))
			{
				bNeedsFullRefresh = true;
			}
		}
	}

	if (bNeedsFullRefresh)
	{
		Refresh();
	}
	else if (bNeedsListRefresh && ColorGradingObjectListView)
	{
		ColorGradingObjectListView->RefreshList();
	}
}

void SColorGradingPanel::OnLevelActorAdded(AActor* Actor)
{
	// Only refresh when the actor being added is being added to the edited world
	if (UWorld* World = GetWorld())
	{
		if (World == Actor->GetWorld())
		{
			if (Actor->IsA<APostProcessVolume>() || Actor->IsA<AColorCorrectRegion>())
			{
				// Wait to refresh, as this event can be fired off for several actors in a row in certain cases, such as when the root actor is recompiled after a property change
				bRefreshOnNextTick = true;
			}
		}
	}
}

void SColorGradingPanel::OnLevelActorDeleted(AActor* Actor)
{
	auto ContainsActorRef = [Actor](const FColorGradingListItemRef& ColorGradingItem)
	{
		return ColorGradingItem->Actor.GetEvenIfUnreachable() == Actor;
	};

	if (ColorGradingItemList.ContainsByPredicate(ContainsActorRef))
	{
		// Must wait for next tick to refresh because the actor has not actually been removed from the level at this point
		bRefreshOnNextTick = true;
	}
}

void SColorGradingPanel::OnWorldAdded(UWorld* World)
{
	bRefreshOnNextTick = true;
}

void SColorGradingPanel::OnWorldDestroyed(UWorld* World)
{
	auto NoLongerValid = [World](const FColorGradingListItemRef& ColorGradingItem)
	{
		return !ColorGradingItem->Actor.IsValid() || ColorGradingItem->Actor->GetWorld() == World;
	};

	// Immediately remove any affected actors so we don't try to access them
	ColorGradingItemList.RemoveAll(NoLongerValid);
}

void SColorGradingPanel::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	Refresh();
}

void SColorGradingPanel::OnColorGradingDataModelGenerated()
{
	FillColorGradingGroupToolBar();

	if (ColorWheelPanel.IsValid())
	{
		ColorWheelPanel->Refresh();
	}
}

void SColorGradingPanel::OnListSelectionChanged(TSharedRef<SColorGradingObjectList> SourceList, FColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		TArray<FColorGradingListItemRef> SelectedObjects = SourceList->GetSelectedItems();
		TArray<UObject*> ObjectsToColorGrade;
		for (const FColorGradingListItemRef& SelectedObject : SelectedObjects)
		{
			if (SelectedObject->Component.IsValid())
			{
				ObjectsToColorGrade.Add(SelectedObject->Component.Get());
			}
			else if (SelectedObject->Actor.IsValid())
			{
				ObjectsToColorGrade.Add(SelectedObject->Actor.Get());
			}
		}

		SetSelectedObjects(ObjectsToColorGrade);
	}
}

FReply SColorGradingPanel::DockInLayout()
{
	DockCallback.ExecuteIfBound();

	return FReply::Handled();
}