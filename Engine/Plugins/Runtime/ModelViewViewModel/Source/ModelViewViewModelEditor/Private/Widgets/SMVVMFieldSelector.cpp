// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMFieldSelector.h"

#include "WidgetBlueprint.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Styling/MVVMEditorStyle.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMFieldSelector"

namespace UE::MVVM
{

namespace Private
{

FBindingSource GetSourceFromPath(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Path)
{
	return FBindingSource::CreateFromPropertyPath(WidgetBlueprint, Path);
}

} // namespace Private

void SFieldSelector::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	check(InWidgetBlueprint);
	
	TextStyle = InArgs._TextStyle;
	OnGetLinkedValue = InArgs._OnGetLinkedValue;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	OnGetSelectionContext = InArgs._OnGetSelectionContext;
	OnDragEnterEvent = InArgs._OnDragEnter;
	OnDropEvent = InArgs._OnDrop;
	bIsBindingToEvent = InArgs._IsBindingToEvent;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(200.0f)
		[
			SAssignNew(ComboButton, SComboButton)
			.ComboButtonStyle(FMVVMEditorStyle::Get(), "FieldSelector.ComboButton")
			.OnGetMenuContent(this, &SFieldSelector::HandleGetMenuContent)
			.ContentPadding(FMargin(4.0f, 2.0f))
			.ButtonContent()
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex(this, &SFieldSelector::GetCurrentDisplayIndex)
				//0-Property/Function (from Widget or Viewmodel).
				//0-Property/Function argument of conversion function
				+ SWidgetSwitcher::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCachedViewBindingPropertyPath, WidgetBlueprint.Get())
					.TextStyle(TextStyle)
					.ShowContext(InArgs._ShowContext)
					.OnGetPropertyPath(this, &SFieldSelector::HandleGetPropertyPath)
				]

				//1-Conversion Function
				+ SWidgetSwitcher::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCachedViewBindingConversionFunction, WidgetBlueprint.Get())
					.TextStyle(TextStyle)
					.OnGetConversionFunction(this, &SFieldSelector::HandleGetConversionFunction)
				]

				//2-Nothing selected.
				+ SWidgetSwitcher::Slot()
				[
					SNew(SBox)
					.Padding(FMargin(8.0f, 0.0f, 8.0f, 0.0f))
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "HintText")
						.Text(LOCTEXT("None", "No field selected"))
					]
				]
			]
		]
	];
}

int32 SFieldSelector::GetCurrentDisplayIndex() const
{
	if (OnGetLinkedValue.IsBound())
	{
		FMVVMLinkedPinValue LinkedValue = OnGetLinkedValue.Execute();
		if (LinkedValue.IsConversionFunction() || LinkedValue.IsConversionNode())
		{
			return 1;
		}
		else if (LinkedValue.IsPropertyPath())
		{
			return 0;
		}
	}
	return 2;
}

TSharedRef<SWidget> SFieldSelector::HandleGetMenuContent()
{
	const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
	if (!WidgetBlueprintPtr)
	{
		return SNullWidget::NullWidget;
	}

	TOptional<FMVVMLinkedPinValue> CurrentSelected;
	if (OnGetLinkedValue.IsBound())
	{
		CurrentSelected = OnGetLinkedValue.Execute();
	}

	FFieldSelectionContext SelectionContext;
	if (OnGetSelectionContext.IsBound())
	{
		SelectionContext = OnGetSelectionContext.Execute();
	}

	TSharedRef<SFieldSelectorMenu> Menu = SNew(SFieldSelectorMenu, WidgetBlueprintPtr)
		.CurrentSelected(CurrentSelected)
		.OnSelectionChanged(this, &SFieldSelector::HandleFieldSelectionChanged)
		.OnMenuCloseRequested(this, &SFieldSelector::HandleMenuClosed)
		.SelectionContext(SelectionContext)
		.IsBindingToEvent(bIsBindingToEvent)
		;

	ComboButton->SetMenuContentWidgetToFocus(Menu->GetWidgetToFocus());

	return Menu;
}

void SFieldSelector::HandleFieldSelectionChanged(FMVVMLinkedPinValue LinkedValue)
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(LinkedValue);
	}
}

FMVVMBlueprintPropertyPath SFieldSelector::HandleGetPropertyPath() const
{
	if (OnGetLinkedValue.IsBound())
	{
		FMVVMLinkedPinValue LinkedValue = OnGetLinkedValue.Execute();
		if (LinkedValue.IsPropertyPath())
		{
			return LinkedValue.GetPropertyPath();
		}
	}
	return FMVVMBlueprintPropertyPath();
}

TVariant<const UFunction*, TSubclassOf<UK2Node>, FEmptyVariantState> SFieldSelector::HandleGetConversionFunction() const
{
	using FReturnType = TVariant<const UFunction*, TSubclassOf<UK2Node>, FEmptyVariantState>;
	if (OnGetLinkedValue.IsBound())
	{
		FMVVMLinkedPinValue LinkedValue = OnGetLinkedValue.Execute();
		if (ensure(LinkedValue.IsConversionFunction() || LinkedValue.IsConversionNode()))
		{
			if (LinkedValue.IsConversionFunction())
			{
				return FReturnType(TInPlaceType<const UFunction*>(), LinkedValue.GetConversionFunction());
			}
			else
			{
				check(LinkedValue.IsConversionNode());
				return FReturnType(TInPlaceType<TSubclassOf<UK2Node>>(), LinkedValue.GetConversionNode());
			}
		}
	}
	return FReturnType(TInPlaceType<FEmptyVariantState>());
}

void SFieldSelector::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnDragEnterEvent.IsBound())
	{
		OnDragEnterEvent.Execute(MyGeometry, DragDropEvent);
	}
}

void SFieldSelector::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>())
	{
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
}

FReply SFieldSelector::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnDropEvent.IsBound())
	{
		return OnDropEvent.Execute(MyGeometry, DragDropEvent);
	}
	return FReply::Unhandled();
}

void SFieldSelector::HandleMenuClosed()
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
