// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Utils/DMWidgetSlot.h"

#include "Layout/Children.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"

FSlotBase* FDMWidgetSlot::GetSlot() const
{
	return Slot;
}

void FDMWidgetSlot::SetSlot(FSlotBase* InSlot)
{
	if (Slot)
	{
		Slot->DetachWidget();
	}

	Slot = InSlot;

	if (Slot && Widget.IsValid())
	{
		Slot->AttachWidget(Widget.ToSharedRef());
	}
}

bool FDMWidgetSlot::IsValid() const
{
	return !bInvalidated && HasWidget();
}

bool FDMWidgetSlot::HasBeenInvalidated() const
{
	return bInvalidated;
}

void FDMWidgetSlot::Invalidate()
{
	bInvalidated = true;
}

bool FDMWidgetSlot::HasWidget() const
{
	return Widget.IsValid() && Widget != SNullWidget::NullWidget;
}

void FDMWidgetSlot::ClearWidget()
{
	Widget.Reset();
	bInvalidated = true;

	if (Slot)
	{
		Slot->DetachWidget();
	}
}

FSlotBase* FDMWidgetSlot::FindSlot(const TSharedRef<SWidget>& InParentWidget, int32 InChildSlot) const
{
	ensure(InChildSlot >= 0);

	FChildren* ParentChildren = InParentWidget->GetChildren();
	ensure(ParentChildren->Num() > InChildSlot);

	return &const_cast<FSlotBase&>(ParentChildren->GetSlotAt(InChildSlot));
}

void FDMWidgetSlot::AssignWidget(const TSharedRef<SWidget>& InWidget)
{
	Widget = InWidget;
	bInvalidated = InWidget == SNullWidget::NullWidget;

	if (Slot)
	{
		Slot->AttachWidget(InWidget);
	}
}

bool FDMWidgetSlot::operator==(const TSharedRef<SWidget>& InWidget) const
{
	return Widget == InWidget;
}
