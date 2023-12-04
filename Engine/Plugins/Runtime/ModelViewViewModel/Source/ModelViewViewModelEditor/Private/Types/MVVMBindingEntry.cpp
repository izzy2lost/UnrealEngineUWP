// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/MVVMBindingEntry.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewEvent.h"


#define LOCTEXT_NAMESPACE "BindingEntry"

namespace UE::MVVM
{

FMVVMBlueprintViewBinding* FBindingEntry::GetBinding(UMVVMBlueprintView* View) const
{
	return View->GetBinding(BindingId);
}

const FMVVMBlueprintViewBinding* FBindingEntry::GetBinding(const UMVVMBlueprintView* View) const
{
	return View->GetBinding(BindingId);
}

void FBindingEntry::SetGroup(FName WidgetName)
{
	check(RowType == ERowType::None);
	RowType = ERowType::Group;
	Name = WidgetName;
	bGroupIsWidget = true;
}

void FBindingEntry::SetGroup(FName ViewModelName, FGuid ViewModelId)
{
	if (!ViewModelId.IsValid())
	{
		SetGroup(ViewModelName);
	}
	else
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Group;
		Name = ViewModelName;
		BindingId = ViewModelId;
		bGroupIsWidget = false;
	}
}

void FBindingEntry::SetBindingId(FGuid Id)
{
	check(RowType == ERowType::None);
	RowType = ERowType::Binding;
	BindingId = Id;
}

void FBindingEntry::SetBindingParameter(FGuid Id, FName ParameterName)
{
	check(RowType == ERowType::None);
	RowType = ERowType::BindingParameter;
	BindingId = Id;
	Name = ParameterName;
}

UMVVMBlueprintViewEvent* FBindingEntry::GetEvent() const
{
	return Event.Get();
}

void FBindingEntry::SetEvent(UMVVMBlueprintViewEvent* InEvent)
{
	check(RowType == ERowType::None);
	RowType = ERowType::Event;
	Event = InEvent;
}

void FBindingEntry::SetEventParameter(UMVVMBlueprintViewEvent* InEvent, FName ParameterName)
{
	check(RowType == ERowType::None);
	RowType = ERowType::EventParameter;
	Event = InEvent;
	Name = ParameterName;
}

void FBindingEntry::AddChild(TSharedPtr<FBindingEntry> Child)
{
	AllChildren.Add(Child);
}
	
void FBindingEntry::AddFilteredChild(TSharedPtr<FBindingEntry> Child)
{
	FilteredChildren.Add(Child);
	bUseFilteredChildren = true;
}

void FBindingEntry::ResetChildren()
{
	AllChildren.Reset();
	FilteredChildren.Reset();
	bUseFilteredChildren = false;
}

void FBindingEntry::SetUseFilteredChildList()
{
	bUseFilteredChildren = true;
}

bool FBindingEntry::operator==(const FBindingEntry& Other) const
{
	return RowType == Other.RowType
		&& Name == Other.Name
		&& BindingId == Other.BindingId
		&& Event == Other.Event;
}

FString FBindingEntry::GetSearchNameString(UMVVMBlueprintView* View, UWidgetBlueprint* WidgetBP)
{
	FString RowToString;
	FString FunctionKeywords;

	switch (RowType)
	{
	case UE::MVVM::FBindingEntry::ERowType::Group:
	case UE::MVVM::FBindingEntry::ERowType::BindingParameter:
	case UE::MVVM::FBindingEntry::ERowType::EventParameter:
		RowToString = Name.ToString();
		break;
	case UE::MVVM::FBindingEntry::ERowType::Binding:
		{
			FMVVMBlueprintViewBinding* BindingInRow = GetBinding(View);
			check(BindingInRow);
			RowToString.Append(BindingInRow->GetSearchableString(WidgetBP));
		}
		break;
	case UE::MVVM::FBindingEntry::ERowType::Event:
		if (UMVVMBlueprintViewEvent* EventInRow = GetEvent())
		{
			RowToString.Append(EventInRow->GetSearchableString());
		}
		break;
	default:
		break;
	}

	RowToString.ReplaceInline(TEXT(" "), TEXT(""));

	return RowToString;
}

} // namespace

#undef LOCTEXT_NAMESPACE