// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SModularRigTreeView.h"
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
#include "ModularRig.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Editor/SModularRigModel.h"
#include "Settings/ControlRigSettings.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SModularRigTreeView"

TMap<FSoftObjectPath, FSlateBrush> SModularRigModelItem::IconPathToBrush;

//////////////////////////////////////////////////////////////
/// FModularRigTreeElement
///////////////////////////////////////////////////////////
FModularRigTreeElement::FModularRigTreeElement(const FString& InKey, TWeakPtr<SModularRigTreeView> InTreeView, bool InSupportsRename)
{
	Key = InKey;
	FString ShortNameStr = Key;
	FString ParentPath;
	ShortNameStr.Split(UModularRig::NamespaceSeparator, &ParentPath, &ShortNameStr, ESearchCase::Type::CaseSensitive, ESearchDir::FromEnd);
	ShortName = *ShortNameStr;
	
	if(InTreeView.IsValid())
	{
		if(const UModularRig* ModularRig = InTreeView.Pin()->GetRigTreeDelegates().GetModularRig())
		{
			RefreshDisplaySettings(ModularRig);
		}
	}
}

void FModularRigTreeElement::RefreshDisplaySettings(const UModularRig* InModularRig)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = SModularRigModelItem::GetBrushForElementType(InModularRig, Key);

	IconBrush = Result.Key;
	IconColor = Result.Value;
	TextColor = FSlateColor::UseForeground();
}

TSharedRef<ITableRow> FModularRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	return SNew(SModularRigModelItem, InOwnerTable, InRigTreeElement, InTreeView, bPinned);
}

void FModularRigTreeElement::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

//////////////////////////////////////////////////////////////
/// SModularRigModelItem
///////////////////////////////////////////////////////////
void SModularRigModelItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetRigTreeDelegates();

	if (InRigTreeElement->Key.IsEmpty())
	{
		STableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
			STableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
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

	STableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
		STableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
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
				.Text(this, &SModularRigModelItem::GetName, true)
				.OnVerifyTextChanged(this, &SModularRigModelItem::OnVerifyNameChanged)
				.OnTextCommitted(this, &SModularRigModelItem::OnNameCommitted)
				.ToolTipText(this, &SModularRigModelItem::GetItemTooltip)
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

	InRigTreeElement->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

void SModularRigModelItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		const FString OldKey = WeakRigTreeElement.Pin()->Key;

		Delegates.HandleRenameElement(OldKey, *NewName);
	}
}

bool SModularRigModelItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FName NewName = *InText.ToString();
	const FString OldPath = WeakRigTreeElement.Pin()->Key;
	return Delegates.HandleVerifyElementNameChanged(OldPath, NewName, OutErrorMessage);
}

FText SModularRigModelItem::GetName(bool bUseShortName) const
{
	if(bUseShortName)
	{
		return (FText::FromName(WeakRigTreeElement.Pin()->ShortName));
	}
	return (FText::FromString(WeakRigTreeElement.Pin()->Key));
}

FText SModularRigModelItem::GetItemTooltip() const
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
/// SModularRigTreeView
///////////////////////////////////////////////////////////

void SModularRigTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	bAutoScrollEnabled = InArgs._AutoScrollEnabled;

	STreeView<TSharedPtr<FModularRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SModularRigTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SModularRigTreeView::HandleGetChildrenForTree);
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.ItemHeight(24);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	
	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
	});
	SuperArgs.OnGeneratePinnedRow(this, &SModularRigTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
	{
		return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
	});

	STreeView<TSharedPtr<FModularRigTreeElement>>::Construct(SuperArgs);

	LastMousePosition = FVector2D::ZeroVector;
	TimeAtMousePosition = 0.0;
}

void SModularRigTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView<TSharedPtr<FModularRigTreeElement, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

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
				const TSharedPtr<FModularRigTreeElement>* Item = FindItemAtPosition(MousePosition);
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

	if (bRequestRenameSelected)
	{
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) {
			TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = GetSelectedItems();
			if (SelectedItems.Num() == 1)
			{
				SelectedItems[0]->RequestRename();
			}
			return EActiveTimerReturnType::Stop;
		}));
		bRequestRenameSelected = false;
	}
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey)
{
	for (TSharedPtr<FModularRigTreeElement> Root : RootElements)
	{
		if (TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, Root))
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey, TSharedPtr<FModularRigTreeElement> CurrentItem)
{
	if (CurrentItem->Key == InElementKey)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

bool SModularRigTreeView::AddElement(FString InKey, FString InParentKey)
{
	if(ElementMap.Contains(InKey))
	{
		return false;
	}

	if (!InKey.IsEmpty())
	{
		TSharedPtr<FModularRigTreeElement> NewItem = MakeShared<FModularRigTreeElement>(InKey, SharedThis(this), false);

		if (!InKey.IsEmpty())
		{
			ElementMap.Add(InKey, NewItem);
			if (!InParentKey.IsEmpty())
			{
				ParentMap.Add(InKey, InParentKey);
			}

			if (!InParentKey.IsEmpty())
			{
				TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InParentKey);
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

	return true;
}

bool SModularRigTreeView::AddElement(const FRigModuleInstance* InElement)
{
	check(InElement);
	
	if (ElementMap.Contains(InElement->GetPath()))
	{
		return false;
	}

	const UModularRig* ModularRig = Delegates.GetModularRig();

	if(!AddElement(InElement->GetPath(), FString()))
	{
		return false;
	}

	if (ElementMap.Contains(InElement->GetPath()))
	{
		if(ModularRig)
		{
			FString ParentKey = ModularRig->GetParentPath(InElement->GetPath());
			if (!ParentKey.IsEmpty())
			{
				if(const FRigModuleInstance* ParentElement = ModularRig->FindModule(ParentKey))
				{
					AddElement(ParentElement);

					if(ElementMap.Contains(ParentKey))
					{
						ReparentElement(InElement->GetPath(), ParentKey);
					}
				}
			}
		}
	}

	return true;
}

void SModularRigTreeView::AddSpacerElement()
{
	AddElement(FString(), FString());
}

bool SModularRigTreeView::ReparentElement(const FString InKey, const FString InParentKey)
{
	if (InKey.IsEmpty() || InKey == InParentKey)
	{
		return false;
	}

	TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (const FString* ExistingParentKey = ParentMap.Find(InKey))
	{
		if (*ExistingParentKey == InParentKey)
		{
			return false;
		}

		if (TSharedPtr<FModularRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InKey);
	}
	else
	{
		if (InParentKey.IsEmpty())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (!InParentKey.IsEmpty())
	{
		ParentMap.Add(InKey, InParentKey);

		TSharedPtr<FModularRigTreeElement>* FoundParent = ElementMap.Find(InParentKey);
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

void SModularRigTreeView::RefreshTreeView(bool bRebuildContent)
{
	TMap<FString, bool> ExpansionState;

	if(bRebuildContent)
	{
		for (TPair<FString, TSharedPtr<FModularRigTreeElement>> Pair : ElementMap)
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
		const UModularRig* ModularRig = Delegates.GetModularRig();
		if(ModularRig)
		{
			ModularRig->ForEachModule([&](const FRigModuleInstance* Element)
			{
				AddElement(Element);
				return true;
			});

			// expand all elements upon the initial construction of the tree
			if (ExpansionState.Num() < ElementMap.Num())
			{
				for (const TPair<FString, TSharedPtr<FModularRigTreeElement>>& Element : ElementMap)
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
			RootElements.RemoveAll([](TSharedPtr<FModularRigTreeElement> InElement)
			{
				return InElement.Get()->Key == FString();
			});
			AddSpacerElement();
		}
	}

	RequestTreeRefresh();
	{
		ClearSelection();
	}
}

TSharedRef<ITableRow> SModularRigTreeView::MakeTableRowWidget(TSharedPtr<FModularRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), bPinned);
}

void SModularRigTreeView::HandleGetChildrenForTree(TSharedPtr<FModularRigTreeElement> InItem,
	TArray<TSharedPtr<FModularRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FString> SModularRigTreeView::GetSelectedKeys() const
{
	TArray<FString> Keys;
	TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FModularRigTreeElement>& SelectedElement : SelectedElements)
	{
		Keys.Add(SelectedElement->Key);
	}
	return Keys;
}

void SModularRigTreeView::SetSelection(const TArray<TSharedPtr<FModularRigTreeElement>>& InSelection) 
{
	ClearSelection();
	SetItemSelection(InSelection, true, ESelectInfo::Direct);
}

const TSharedPtr<FModularRigTreeElement>* SModularRigTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && SListView<TSharedPtr<FModularRigTreeElement>>::HasValidItemsSource())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		const int32 Index = FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ArrangedChildren.IsValidIndex(Index))
		{
			TSharedRef<SModularRigModelItem> ItemWidget = StaticCastSharedRef<SModularRigModelItem>(ArrangedChildren[Index].Widget);
			if (ItemWidget->WeakRigTreeElement.IsValid())
			{
				const FString Key = ItemWidget->WeakRigTreeElement.Pin()->Key;
				const TSharedPtr<FModularRigTreeElement>* ResultPtr = SListView<TSharedPtr<FModularRigTreeElement>>::GetItems().FindByPredicate([Key](const TSharedPtr<FModularRigTreeElement>& Item) -> bool
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

TPair<const FSlateBrush*, FSlateColor> SModularRigModelItem::GetBrushForElementType(const UModularRig* InModularRig, const FString& InKey)
{
	const FSlateBrush* Brush = nullptr;
	FSlateColor Color = FSlateColor::UseForeground();

	if (const FRigModuleInstance* Module = InModularRig->FindModule(InKey))
	{
		if (Module->Rig.IsValid())
		{
			FSoftObjectPath IconPath = Module->Rig->GetRigModuleSettings().Icon;
			Brush = IconPathToBrush.Find(IconPath);
			if (!Brush)
			{
				if(UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad()))
				{
					Brush = &IconPathToBrush.Add(IconPath, UWidgetBlueprintLibrary::MakeBrushFromTexture(Icon, 16.0f, 16.0f));
				}
			}
		}
	}
	if (!Brush)
	{
		Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.RigidBody");
	}
	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

FLinearColor SModularRigModelItem::GetColorForControlType(ERigControlType InControlType, UEnum* InControlEnum)
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

//////////////////////////////////////////////////////////////
/// SSearchableModularRigTreeView
///////////////////////////////////////////////////////////

void SSearchableModularRigTreeView::Construct(const FArguments& InArgs)
{
	FModularRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	
	ChildSlot
	[
		SNew(SVerticalBox)
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
					SAssignNew(TreeView, SModularRigTreeView)
					.RigTreeDelegates(TreeDelegates)
				]
			]
		]
	];
}


#undef LOCTEXT_NAMESPACE
