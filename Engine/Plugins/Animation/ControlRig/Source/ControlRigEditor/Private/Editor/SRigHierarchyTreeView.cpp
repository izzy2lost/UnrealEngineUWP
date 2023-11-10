// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SRigHierarchyTreeView.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "ControlRigEditorStyle.h"
#include "Editor/SRigHierarchy.h"
#include "Settings/ControlRigSettings.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SRigHierarchyTreeView"

//////////////////////////////////////////////////////////////
/// FRigTreeDelegates
///////////////////////////////////////////////////////////
FRigTreeDisplaySettings FRigTreeDelegates::DefaultDisplaySettings;

//////////////////////////////////////////////////////////////
/// FRigTreeElement
///////////////////////////////////////////////////////////
FRigTreeElement::FRigTreeElement(const FRigElementKey& InKey, TWeakPtr<SRigHierarchyTreeView> InTreeView, bool InSupportsRename, ERigTreeFilterResult InFilterResult)
{
	Key = InKey;
	ShortName = InKey.Name;
	ChannelName = NAME_None;
	bIsTransient = false;
	bIsAnimationChannel = false;
	bIsProcedural = false;
	bSupportsRename = InSupportsRename;
	FilterResult = InFilterResult;

	if(InTreeView.IsValid())
	{
		if(const URigHierarchy* Hierarchy = InTreeView.Pin()->GetRigTreeDelegates().GetHierarchy())
		{
			ShortName = Hierarchy->GetNameMetadata(InKey, URigHierarchy::ShortNameMetadataName, ShortName);
			
			const FRigTreeDisplaySettings& Settings = InTreeView.Pin()->GetRigTreeDelegates().GetDisplaySettings();
			RefreshDisplaySettings(Hierarchy, Settings);
		}
	}
}


TSharedRef<ITableRow> FRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	return SNew(SRigHierarchyItem, InOwnerTable, InRigTreeElement, InTreeView, InSettings, bPinned);
}

void FRigTreeElement::RequestRename()
{
	if(bSupportsRename)
	{
		OnRenameRequested.ExecuteIfBound();
	}
}

void FRigTreeElement::RefreshDisplaySettings(const URigHierarchy* InHierarchy, const FRigTreeDisplaySettings& InSettings)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = SRigHierarchyItem::GetBrushForElementType(InHierarchy, Key);

	if(const FRigBaseElement* Element = InHierarchy->Find(Key))
	{
		bIsProcedural = Element->IsProcedural();
				
		if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			bIsTransient = ControlElement->Settings.bIsTransientControl;
			bIsAnimationChannel = ControlElement->IsAnimationChannel();
			if(bIsAnimationChannel)
			{
				ChannelName = ControlElement->GetDisplayName();
			}
		}
	}

	IconBrush = Result.Key;
	IconColor = Result.Value;
	if(IconColor.IsColorSpecified() && InSettings.bShowIconColors)
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? Result.Value : FSlateColor(Result.Value.GetSpecifiedColor() * 0.5f);
	}
	else
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Gray * 0.5f);
	}

	TextColor = FilterResult == ERigTreeFilterResult::Shown ?
		(InHierarchy->IsProcedural(Key) ? FSlateColor(FLinearColor(0.9f, 0.8f, 0.4f)) : FSlateColor::UseForeground()) :
		(InHierarchy->IsProcedural(Key) ? FSlateColor(FLinearColor(0.9f, 0.8f, 0.4f) * 0.5f) : FSlateColor(FLinearColor::Gray * 0.5f));
}

//////////////////////////////////////////////////////////////
/// SRigHierarchyItem
///////////////////////////////////////////////////////////
void SRigHierarchyItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetRigTreeDelegates();
	FRigTreeDisplaySettings DisplaySettings = Delegates.GetDisplaySettings();

	if (!InRigTreeElement->Key.IsValid())
	{
		STableRow<TSharedPtr<FRigTreeElement>>::Construct(
			STableRow<TSharedPtr<FRigTreeElement>>::FArguments()
			.ShowSelection(false)
			.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
			.OnAcceptDrop(Delegates.OnAcceptDrop)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(200.f)
				[
					SNew(SSpacer)
				]
			], OwnerTable);
		return;
	}

	TSharedPtr< SInlineEditableTextBlock > InlineWidget;
	TSharedPtr< SHorizontalBox > HorizontalBox;

	STableRow<TSharedPtr<FRigTreeElement>>::Construct(
		STableRow<TSharedPtr<FRigTreeElement>>::FArguments()
		.OnDragDetected(Delegates.OnDragDetected)
		.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
		.OnAcceptDrop(Delegates.OnAcceptDrop)
		.ShowWires(true)
		.Content()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.MaxWidth(18)
			.FillWidth(1.0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconBrush;
					}
					return nullptr;
				})
				.ColorAndOpacity_Lambda([this]()
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconColor;
					}
					return FSlateColor::UseForeground();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Text(this, &SRigHierarchyItem::GetName, DisplaySettings.bUseShortName)
				.ToolTipText(this, &SRigHierarchyItem::GetItemTooltip)
				.OnVerifyTextChanged(this, &SRigHierarchyItem::OnVerifyNameChanged)
				.OnTextCommitted(this, &SRigHierarchyItem::OnNameCommitted)
				.MultiLine(false)
				.ColorAndOpacity_Lambda([this]()
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->TextColor;
					}
					return FSlateColor::UseForeground();
				})
			]
		], OwnerTable);

	for(const SRigHierarchyTagWidget::FArguments& TagArguments : InRigTreeElement->Tags)
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		[
			SArgumentNew(TagArguments, SRigHierarchyTagWidget)
		];
	}

	InRigTreeElement->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

FText SRigHierarchyItem::GetName(bool bUseShortName) const
{
	if(WeakRigTreeElement.Pin()->bIsTransient)
	{
		static const FText TemporaryControl = FText::FromString(TEXT("Temporary Control"));
		return TemporaryControl;
	}
	if(WeakRigTreeElement.Pin()->bIsAnimationChannel)
	{
		return FText::FromName(WeakRigTreeElement.Pin()->ChannelName);
	}
	if(bUseShortName)
	{
		return (FText::FromName(WeakRigTreeElement.Pin()->ShortName));
	}
	return (FText::FromName(WeakRigTreeElement.Pin()->Key.Name));
}

FText SRigHierarchyItem::GetItemTooltip() const
{
	const FText FullName = GetName(false);
	const FText ShortName = GetName(true);
	if(FullName.EqualTo(ShortName))
	{
		return FText();
	}
	return FullName;
}

//////////////////////////////////////////////////////////////
/// SRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	bAutoScrollEnabled = InArgs._AutoScrollEnabled;

	STreeView<TSharedPtr<FRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SRigHierarchyTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SRigHierarchyTreeView::HandleGetChildrenForTree);
	SuperArgs.OnSelectionChanged(FOnRigTreeSelectionChanged::CreateRaw(&Delegates, &FRigTreeDelegates::HandleSelectionChanged));
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	SuperArgs.OnSetExpansionRecursive(Delegates.OnSetExpansionRecursive);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.ItemHeight(24);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	
	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
	});
	SuperArgs.OnGeneratePinnedRow(this, &SRigHierarchyTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
	{
		return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
	});

	STreeView<TSharedPtr<FRigTreeElement>>::Construct(SuperArgs);

	LastMousePosition = FVector2D::ZeroVector;
	TimeAtMousePosition = 0.0;
}

void SRigHierarchyTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView<TSharedPtr<FRigTreeElement, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FGeometry PaintGeometry = GetPaintSpaceGeometry();
	const FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();

	if(PaintGeometry.IsUnderLocation(MousePosition))
	{
		const FVector2D WidgetPosition = PaintGeometry.AbsoluteToLocal(MousePosition);

		static constexpr float SteadyMousePositionTolerance = 5.f;

		if(LastMousePosition.Equals(MousePosition, SteadyMousePositionTolerance))
		{
			TimeAtMousePosition += InDeltaTime;
		}
		else
		{
			LastMousePosition = MousePosition;
			TimeAtMousePosition = 0.0;
		}

		static constexpr float AutoScrollStartDuration = 0.5f; // in seconds
		static constexpr float AutoScrollDistance = 24.f; // in pixels
		static constexpr float AutoScrollSpeed = 150.f;

		if(TimeAtMousePosition > AutoScrollStartDuration && FSlateApplication::Get().IsDragDropping())
		{
			if((WidgetPosition.Y < AutoScrollDistance) || (WidgetPosition.Y > PaintGeometry.Size.Y - AutoScrollDistance))
			{
				if(bAutoScrollEnabled)
				{
					const bool bScrollUp = (WidgetPosition.Y < AutoScrollDistance);

					const float DeltaInSlateUnits = (bScrollUp ? -InDeltaTime : InDeltaTime) * AutoScrollSpeed; 
					ScrollBy(GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No);
				}
			}
			else
			{
				const TSharedPtr<FRigTreeElement>* Item = FindItemAtPosition(MousePosition);
				if(Item && Item->IsValid())
				{
					if(!IsItemExpanded(*Item))
					{
						SetItemExpansion(*Item, true);
					}
				}
			}
		}
	}
}

TSharedPtr<FRigTreeElement> SRigHierarchyTreeView::FindElement(const FRigElementKey& InElementKey, TSharedPtr<FRigTreeElement> CurrentItem)
{
	if (CurrentItem->Key == InElementKey)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FRigTreeElement> Found = FindElement(InElementKey, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FRigTreeElement>();
}

bool SRigHierarchyTreeView::AddElement(FRigElementKey InKey, FRigElementKey InParentKey)
{
	if(ElementMap.Contains(InKey))
	{
		return false;
	}

	// skip transient controls
	if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
	{
		if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InKey))
		{
			if(ControlElement->Settings.bIsTransientControl)
			{
				return false;
			}
		}
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	const bool bSupportsRename = Delegates.OnRenameElement.IsBound();

	const FString FilteredString = Settings.FilterText.ToString();
	if (FilteredString.IsEmpty() || !InKey.IsValid())
	{
		TSharedPtr<FRigTreeElement> NewItem = MakeShared<FRigTreeElement>(InKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::Shown);

		if (InKey.IsValid())
		{
			ElementMap.Add(InKey, NewItem);
			if (InParentKey)
			{
				ParentMap.Add(InKey, InParentKey);
			}

			if (InParentKey)
			{
				TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InParentKey);
				check(FoundItem);
				FoundItem->Get()->Children.Add(NewItem);
			}
			else
			{
				RootElements.Add(NewItem);
			}
		}
		else
		{
			RootElements.Add(NewItem);
		}
	}
	else
	{
		FString FilteredStringUnderScores = FilteredString.Replace(TEXT(" "), TEXT("_"));
		if (InKey.Name.ToString().Contains(FilteredString) || InKey.Name.ToString().Contains(FilteredStringUnderScores))	
		{
			TSharedPtr<FRigTreeElement> NewItem = MakeShared<FRigTreeElement>(InKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::Shown);
			ElementMap.Add(InKey, NewItem);
			RootElements.Add(NewItem);

			if (!Settings.bFlattenHierarchyOnFilter && !Settings.bHideParentsOnFilter)
			{
				if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
				{
					TSharedPtr<FRigTreeElement> ChildItem = NewItem;
					FRigElementKey ParentKey = Hierarchy->GetFirstParent(InKey);
					while (ParentKey.IsValid())
					{
						if (!ElementMap.Contains(ParentKey))
						{
							TSharedPtr<FRigTreeElement> ParentItem = MakeShared<FRigTreeElement>(ParentKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::ShownDescendant);							
							ElementMap.Add(ParentKey, ParentItem);
							RootElements.Add(ParentItem);

							ReparentElement(ChildItem->Key, ParentKey);

							ChildItem = ParentItem;
							ParentKey = Hierarchy->GetFirstParent(ParentKey);
						}
						else
						{
							ReparentElement(ChildItem->Key, ParentKey);
							break;
						}						
					}
				}
			}
		}
	}

	return true;
}

bool SRigHierarchyTreeView::AddElement(const FRigBaseElement* InElement)
{
	check(InElement);
	
	if (ElementMap.Contains(InElement->GetKey()))
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	const URigHierarchy* Hierarchy = Delegates.GetHierarchy();

	switch(InElement->GetType())
	{
		case ERigElementType::Bone:
		{
			if(!Settings.bShowBones)
			{
				return false;
			}

			const FRigBoneElement* BoneElement = CastChecked<FRigBoneElement>(InElement);
			if (!Settings.bShowImportedBones && BoneElement->BoneType == ERigBoneType::Imported)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Null:
		{
			if(!Settings.bShowNulls)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Control:
		{
			if(!Settings.bShowControls)
			{
				return false;
			}
			break;
		}
		case ERigElementType::RigidBody:
		{
			if(!Settings.bShowRigidBodies)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Reference:
		{
			if(!Settings.bShowReferences)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Curve:
		{
			return false;
		}
		case ERigElementType::Connector:
		{
			if(Hierarchy)
			{
				// add the connector as a tag rather than its own element in the tree
				if(UControlRig* ControlRig = Hierarchy->GetTypedOuter<UControlRig>())
				{
					FRigElementKeyRedirector& Redirector = ControlRig->GetElementKeyRedirector();
					if(const FCachedRigElement* Cache = Redirector.Find(InElement->GetKey()))
					{
						if(const_cast<FCachedRigElement*>(Cache)->UpdateCache(Hierarchy))
						{
							if(const TSharedPtr<FRigTreeElement>* TargetElementPtr = ElementMap.Find(Cache->GetKey()))
							{
								const FRigElementKey ConnectorKey = InElement->GetKey();

								SRigHierarchyTagWidget::FArguments TagArguments;
								TagArguments.Text(FText::FromString(InElement->GetName()));
								TagArguments.TextColor(FSlateColor(FLinearColor::White));
								TagArguments.Color(FLinearColor(0.0, 112.f/255.f, 224.f/255.f));
								TagArguments.AllowDragDrop(true);
								FString Identifier;
								FRigElementKey::StaticStruct()->ExportText(Identifier, &ConnectorKey, nullptr, nullptr, PPF_None, nullptr);
								TagArguments.Identifier(Identifier);

								TagArguments.OnClicked_Lambda([ConnectorKey, this]()
								{
									Delegates.RequestDetailsInspection(ConnectorKey);
								});

								TargetElementPtr->Get()->Tags.Add(TagArguments);

								AddConnectorResolveWarningTag(*TargetElementPtr, InElement, Hierarchy);
								return true;
							}
						}
					}
				}
			}
			break;
		}
		case ERigElementType::Socket:
		{
			if(!Settings.bShowSockets)
			{
				return false;
			}
			break;
		}
		default:
		{
			break;
		}
	}

	if(!AddElement(InElement->GetKey(), FRigElementKey()))
	{
		return false;
	}

	if (ElementMap.Contains(InElement->GetKey()))
	{
		if(Hierarchy)
		{
			if(InElement->GetType() == ERigElementType::Connector)
			{
				AddConnectorResolveWarningTag(ElementMap.FindChecked(InElement->GetKey()), InElement, Hierarchy);
			}
			
			FRigElementKey ParentKey = Hierarchy->GetFirstParent(InElement->GetKey());
			if(InElement->GetType() == ERigElementType::Connector)
			{
				ParentKey = Delegates.GetResolvedKey(InElement->GetKey());
				if(ParentKey == InElement->GetKey())
				{
					ParentKey.Reset();
				}
			}

			TArray<FRigElementWeight> ParentWeights = Hierarchy->GetParentWeightArray(InElement->GetKey());
			if(ParentWeights.Num() > 0)
			{
				TArray<FRigElementKey> ParentKeys = Hierarchy->GetParents(InElement->GetKey());
				check(ParentKeys.Num() == ParentWeights.Num());
				for(int32 ParentIndex=0;ParentIndex<ParentKeys.Num();ParentIndex++)
				{
					if(ParentWeights[ParentIndex].IsAlmostZero())
					{
						continue;
					}
					ParentKey = ParentKeys[ParentIndex];
					break;
				}
			}

			if (ParentKey.IsValid())
			{
				if(const FRigBaseElement* ParentElement = Hierarchy->Find(ParentKey))
				{
					AddElement(ParentElement);

					if(ElementMap.Contains(ParentKey))
					{
						ReparentElement(InElement->GetKey(), ParentKey);
					}
				}
			}
		}
	}

	return true;
}

void SRigHierarchyTreeView::AddSpacerElement()
{
	AddElement(FRigElementKey(), FRigElementKey());
}

bool SRigHierarchyTreeView::ReparentElement(FRigElementKey InKey, FRigElementKey InParentKey)
{
	if (!InKey.IsValid() || InKey == InParentKey)
	{
		return false;
	}

	if(InKey.Type == ERigElementType::Connector)
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();

	TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (!Settings.FilterText.IsEmpty() && Settings.bFlattenHierarchyOnFilter)
	{
		return false;
	}

	if (const FRigElementKey* ExistingParentKey = ParentMap.Find(InKey))
	{
		if (*ExistingParentKey == InParentKey)
		{
			return false;
		}

		if (TSharedPtr<FRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InKey);
	}
	else
	{
		if (!InParentKey.IsValid())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (InParentKey)
	{
		ParentMap.Add(InKey, InParentKey);

		TSharedPtr<FRigTreeElement>* FoundParent = ElementMap.Find(InParentKey);
		if(FoundParent)
		{
			FoundParent->Get()->Children.Add(*FoundItem);
		}
	}
	else
	{
		RootElements.Add(*FoundItem);
	}

	return true;
}

bool SRigHierarchyTreeView::RemoveElement(FRigElementKey InKey)
{
	TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	ReparentElement(InKey, FRigElementKey());

	RootElements.Remove(*FoundItem);
	return ElementMap.Remove(InKey) > 0;
}

void SRigHierarchyTreeView::RefreshTreeView(bool bRebuildContent)
{
		TMap<FRigElementKey, bool> ExpansionState;

	if(bRebuildContent)
	{
		for (TPair<FRigElementKey, TSharedPtr<FRigTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();

		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();
	}

	if(bRebuildContent)
	{
		const URigHierarchy* Hierarchy = Delegates.GetHierarchy();
		if(Hierarchy)
		{
			TArray<FRigConnectorElement*> Connectors;
			Hierarchy->Traverse([&](FRigBaseElement* Element, bool& bContinue)
			{
				if(FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Element))
				{
					Connectors.Add(Connector);
				}
				else
				{
					AddElement(Element);
				}
				bContinue = true;
			});

			// add all of the connectors. their parent relationship in the tree represents resolval
			for(FRigConnectorElement* Connector : Connectors)
			{
				AddElement(Connector);
			}

			// expand all elements upon the initial construction of the tree
			if (ExpansionState.Num() == 0)
			{
				for (TSharedPtr<FRigTreeElement> RootElement : RootElements)
				{
					SetExpansionRecursive(RootElement, false, true);
				}
			}
			else if (ExpansionState.Num() < ElementMap.Num())
			{
				for (const TPair<FRigElementKey, TSharedPtr<FRigTreeElement>>& Element : ElementMap)
				{
					if (!ExpansionState.Contains(Element.Key))
					{
						SetItemExpansion(Element.Value, true);
					}
				}
			}

			for (const auto& Pair : ElementMap)
			{
				RestoreSparseItemInfos(Pair.Value);
			}

			if(Delegates.OnCompareKeys.IsBound())
			{
				Algo::Sort(RootElements, [&](const TSharedPtr<FRigTreeElement>& A, const TSharedPtr<FRigTreeElement>& B)
				{
					return Delegates.OnCompareKeys.Execute(A->Key, B->Key);
				});
			}

			if (RootElements.Num() > 0)
			{
				AddSpacerElement();
			}
		}
	}
	else
	{
		if (RootElements.Num()> 0)
		{
			// elements may be added at the end of the list after a spacer element
			// we need to remove the spacer element and re-add it at the end
			RootElements.RemoveAll([](TSharedPtr<FRigTreeElement> InElement)
			{
				return InElement.Get()->Key == FRigElementKey();
			});
			AddSpacerElement();
		}
	}

	RequestTreeRefresh();
	{
		ClearSelection();

		if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
		{
			TArray<FRigElementKey> Selection = Hierarchy->GetSelectedKeys();
			for (const FRigElementKey& Key : Selection)
			{
				for (int32 RootIndex = 0; RootIndex < RootElements.Num(); ++RootIndex)
				{
					TSharedPtr<FRigTreeElement> Found = FindElement(Key, RootElements[RootIndex]);
					if (Found.IsValid())
					{
						SetItemSelection(Found, true, ESelectInfo::OnNavigation);
					}
				}
			}
		}
	}
}

void SRigHierarchyTreeView::SetExpansionRecursive(TSharedPtr<FRigTreeElement> InElement, bool bTowardsParent,
	bool bShouldBeExpanded)
{
	SetItemExpansion(InElement, bShouldBeExpanded);

	if (bTowardsParent)
	{
		if (const FRigElementKey* ParentKey = ParentMap.Find(InElement->Key))
		{
			if (TSharedPtr<FRigTreeElement>* ParentItem = ElementMap.Find(*ParentKey))
			{
				SetExpansionRecursive(*ParentItem, bTowardsParent, bShouldBeExpanded);
			}
		}
	}
	else
	{
		for (int32 ChildIndex = 0; ChildIndex < InElement->Children.Num(); ++ChildIndex)
		{
			SetExpansionRecursive(InElement->Children[ChildIndex], bTowardsParent, bShouldBeExpanded);
		}
	}
}

TSharedRef<ITableRow> SRigHierarchyTreeView::MakeTableRowWidget(TSharedPtr<FRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), Settings, bPinned);
}

void SRigHierarchyTreeView::HandleGetChildrenForTree(TSharedPtr<FRigTreeElement> InItem,
	TArray<TSharedPtr<FRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FRigElementKey> SRigHierarchyTreeView::GetSelectedKeys() const
{
	TArray<FRigElementKey> Keys;
	TArray<TSharedPtr<FRigTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FRigTreeElement>& SelectedElement : SelectedElements)
	{
		Keys.Add(SelectedElement->Key);
	}
	return Keys;
}

const TSharedPtr<FRigTreeElement>* SRigHierarchyTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && SListView<TSharedPtr<FRigTreeElement>>::HasValidItemsSource())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		const int32 Index = FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ArrangedChildren.IsValidIndex(Index))
		{
			TSharedRef<SRigHierarchyItem> ItemWidget = StaticCastSharedRef<SRigHierarchyItem>(ArrangedChildren[Index].Widget);
			if (ItemWidget->WeakRigTreeElement.IsValid())
			{
				const FRigElementKey Key = ItemWidget->WeakRigTreeElement.Pin()->Key;
				const TSharedPtr<FRigTreeElement>* ResultPtr = SListView<TSharedPtr<FRigTreeElement>>::GetItems().FindByPredicate([Key](const TSharedPtr<FRigTreeElement>& Item) -> bool
					{
						return Item->Key == Key;
					});

				if (ResultPtr)
				{
					return ResultPtr;
				}
			}
		}
	}
	return nullptr;
}

void SRigHierarchyTreeView::AddConnectorResolveWarningTag(TSharedPtr<FRigTreeElement> InTreeElement,
	const FRigBaseElement* InRigElement, const URigHierarchy* InHierarchy)
{
	check(InTreeElement.IsValid());
	check(InRigElement);
	check(InRigElement->GetType() == ERigElementType::Connector);

	if(UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>())
	{
		TWeakObjectPtr<UControlRig> ControlRigPtr(ControlRig);
		const FRigElementKey ConnectorKey = InRigElement->GetKey();
		
		TAttribute<FText> GetTooltipText = TAttribute<FText>::CreateSP(this,
			&SRigHierarchyTreeView::GetConnectorWarningMessage, InTreeElement, ControlRigPtr, ConnectorKey);

		SRigHierarchyTagWidget::FArguments TagArguments;
		TagArguments.Visibility_Lambda([GetTooltipText]() -> EVisibility
		{
			return GetTooltipText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		});
		TagArguments.Text(LOCTEXT("ConnectorWarningTagLabel", "Warning"));
		TagArguments.ToolTipText(GetTooltipText);
		TagArguments.TextColor(FSlateColor(FLinearColor::Black));
		TagArguments.Color(FLinearColor(0.728f, 0.364f, 0.003f));
		TagArguments.Icon(FAppStyle::GetBrush("AnimSlotManager.Warning"));
		InTreeElement->Tags.Add(TagArguments);
	}
}

FText SRigHierarchyTreeView::GetConnectorWarningMessage(TSharedPtr<FRigTreeElement> InTreeElement,
	TWeakObjectPtr<UControlRig> InControlRigPtr, const FRigElementKey InConnectorKey) const
{
	if(UControlRig* ControlRig = InControlRigPtr.Get())
	{
		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
				
		static const FText NotResolvedWarning = LOCTEXT("ConnectorWarningConnectorNotResolved", "Connector is not resolved.");

		FRigElementKeyRedirector& Redirector = ControlRig->GetElementKeyRedirector();
		if(const FCachedRigElement* Cache = Redirector.Find(InConnectorKey))
		{
			if(const_cast<FCachedRigElement*>(Cache)->UpdateCache(Hierarchy))
			{
				if(const FRigConnectorElement* ConnectorElement = Hierarchy->Find<FRigConnectorElement>(InConnectorKey))
				{
					const FRigConnectionInfo ConnectionInfo(&ControlRig->GetElementKeyRedirector(), Hierarchy);
					FString FailureReason;
					if(!ConnectorElement->CanConnect(&ConnectionInfo, &FailureReason))
					{
						return FText::FromString(FailureReason);
					}
				}
			}
			else
			{
				return NotResolvedWarning;
			}
		}
		else
		{
			return NotResolvedWarning;
		}
	}
	return FText();
}

bool SRigHierarchyItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FString NewName = InText.ToString();
	const FRigElementKey OldKey = WeakRigTreeElement.Pin()->Key;
	return Delegates.HandleVerifyElementNameChanged(OldKey, NewName, OutErrorMessage);
}

TPair<const FSlateBrush*, FSlateColor> SRigHierarchyItem::GetBrushForElementType(const URigHierarchy* InHierarchy, const FRigElementKey& InKey)
{
	const FSlateBrush* Brush = nullptr;
	FSlateColor Color = FSlateColor::UseForeground();
	switch (InKey.Type)
	{
		case ERigElementType::Control:
		{
			if(const FRigControlElement* Control = InHierarchy->Find<FRigControlElement>(InKey))
			{
				FLinearColor ShapeColor = FLinearColor::White;
				
				if(Control->Settings.SupportsShape())
				{
					if(Control->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
					{
						Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.ProxyControl");
					}
					else
					{
						Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Control");
					}
					ShapeColor = Control->Settings.ShapeColor;
				}
				else
				{
					static FName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
					Brush = FAppStyle::GetBrush(TypeIcon);
					ShapeColor = GetColorForControlType(Control->Settings.ControlType, Control->Settings.ControlEnum);
				}
				
				// ensure the alpha is always visible
				ShapeColor.A = 1.f;
				Color = FSlateColor(ShapeColor);
			}
			else
			{
				Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Control");
			}
			break;
		}
		case ERigElementType::Null:
		{
			Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Null");
			break;
		}
		case ERigElementType::Bone:
		{
			ERigBoneType BoneType = ERigBoneType::User;

			if(InHierarchy)
			{
				const FRigBoneElement* BoneElement = InHierarchy->Find<FRigBoneElement>(InKey);
				if(BoneElement)
				{
					BoneType = BoneElement->BoneType;
				}
			}

			switch (BoneType)
			{
				case ERigBoneType::Imported:
				{
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.BoneImported");
					break;
				}
				case ERigBoneType::User:
				default:
				{
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.BoneUser");
					break;
				}
			}

			break;
		}
		case ERigElementType::RigidBody:
		{
			Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.RigidBody");
			break;
		}
		case ERigElementType::Reference:
		case ERigElementType::Socket:
		{
			Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Socket");
			break;
		}
		case ERigElementType::Connector:
		{
			Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Connector");
			break;
		}
		default:
		{
			break;
		}
	}

	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

FLinearColor SRigHierarchyItem::GetColorForControlType(ERigControlType InControlType, UEnum* InControlEnum)
{
	FEdGraphPinType PinType;
	switch(InControlType)
	{
		case ERigControlType::Bool:
		{
			PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::BoolTypeName, nullptr);
			break;
		}
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
		{
			PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::FloatTypeName, nullptr);
			break;
		}
		case ERigControlType::Integer:
		{
			if(InControlEnum)
			{
				PinType = RigVMTypeUtils::PinTypeFromCPPType(NAME_None, InControlEnum);
			}
			else
			{
				PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::Int32TypeName, nullptr);
			}
			break;
		}
		case ERigControlType::Vector2D:
		{
			UScriptStruct* Struct = TBaseStructure<FVector2D>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			UScriptStruct* Struct = TBaseStructure<FVector>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Rotator:
		{
			UScriptStruct* Struct = TBaseStructure<FRotator>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		default:
		{
			UScriptStruct* Struct = TBaseStructure<FTransform>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
	}
	const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();
	return Schema->GetPinTypeColor(PinType);
}

void SRigHierarchyItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		const FRigElementKey OldKey = WeakRigTreeElement.Pin()->Key;

		const FName NewSanitizedName = Delegates.HandleRenameElement(OldKey, NewName);
		if (NewSanitizedName.IsNone())
		{
			return;
		}
		NewName = NewSanitizedName.ToString();

		if (WeakRigTreeElement.IsValid())
		{
			WeakRigTreeElement.Pin()->Key.Name = *NewName;
		}
	}
}

//////////////////////////////////////////////////////////////
/// SSearchableRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SSearchableRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	FRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	SuperGetRigTreeDisplaySettings = TreeDelegates.OnGetDisplaySettings;

	TreeDelegates.OnGetDisplaySettings.BindSP(this, &SSearchableRigHierarchyTreeView::GetDisplaySettings);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		.Padding(0.0f)
		[
			SAssignNew(SearchBox, SSearchBox)
			.InitialText(InArgs._InitialFilterText)
			.OnTextChanged(this, &SSearchableRigHierarchyTreeView::OnFilterTextChanged)
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		.Padding(0.0f, 0.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SRigHierarchyTreeView)
					.RigTreeDelegates(TreeDelegates)
				]
			]
		]
	];
}

const FRigTreeDisplaySettings& SSearchableRigHierarchyTreeView::GetDisplaySettings()
{
	if(SuperGetRigTreeDisplaySettings.IsBound())
	{
		Settings = SuperGetRigTreeDisplaySettings.Execute();
	}
	Settings.FilterText = FilterText;
	return Settings;
}

void SSearchableRigHierarchyTreeView::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	GetTreeView()->RefreshTreeView();
}


#undef LOCTEXT_NAMESPACE
