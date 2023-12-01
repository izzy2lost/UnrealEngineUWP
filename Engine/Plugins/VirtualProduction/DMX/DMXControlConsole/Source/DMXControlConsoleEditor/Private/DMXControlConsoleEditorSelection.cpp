// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorSelection.h"

#include "Algo/Sort.h"
#include "Controllers/DMXControlConsoleElementController.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorSelection"

FDMXControlConsoleEditorSelection::FDMXControlConsoleEditorSelection(UDMXControlConsoleEditorModel* InEditorModel)
	: EditorModel(InEditorModel)
{}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (FaderGroup && FaderGroup->IsActive())
	{
		SelectedFaderGroups.AddUnique(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange)
{
	if (ElementController && ElementController->IsActive())
	{
		SelectedElementControllers.AddUnique(ElementController);

		UDMXControlConsoleFaderGroup& FaderGroup = ElementController->GetOwnerFaderGroupChecked();
		SelectedFaderGroups.AddUnique(&FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::AddToSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange)
{
	if (Objects.IsEmpty())
	{
		return;
	}

	for (UObject* Object : Objects)
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(Object))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Object))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddToSelection(ElementController, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::AddAllFadersFromFaderGroupToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bOnlyMatchingFilter, bool bNotifySelectionChange)
{
	if (FaderGroup && FaderGroup->IsActive())
	{
		const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroup->GetAllElementControllers();
		for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
		{
			if (!ElementController || !ElementController->IsActive())
			{
				continue;
			}

			if (bOnlyMatchingFilter && !ElementController->IsMatchingFilter())
			{
				continue;
			}

			SelectedElementControllers.AddUnique(ElementController);
		}

		SelectedFaderGroups.AddUnique(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (FaderGroup && SelectedFaderGroups.Contains(FaderGroup))
	{
		constexpr bool bNotifyFadersSelectionChange = false;
		ClearElementControllersSelection(FaderGroup, bNotifyFadersSelectionChange);
		SelectedFaderGroups.Remove(FaderGroup);

		UpdateMultiSelectAnchor(UDMXControlConsoleFaderGroup::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange)
{
	if (ElementController && SelectedElementControllers.Contains(ElementController))
	{
		SelectedElementControllers.Remove(ElementController);

		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());

		if (bNotifySelectionChange)
		{
			OnSelectionChanged.Broadcast();
		}
	}
}

void FDMXControlConsoleEditorSelection::RemoveFromSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange)
{
	if (Objects.IsEmpty())
	{
		return;
	}

	for (UObject* Object : Objects)
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(Object))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			RemoveFromSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Object))
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			RemoveFromSelection(ElementController, bNotifyFaderSelectionChange);
		}
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::Multiselect(UObject* ElementControllerOrFaderGroupObject)
{
	const UClass* MultiSelectClass = ElementControllerOrFaderGroupObject->GetClass();
	if (!ensureMsgf(MultiSelectClass == UDMXControlConsoleFaderGroup::StaticClass() || ElementControllerOrFaderGroupObject->IsA(UDMXControlConsoleElementController::StaticClass()), TEXT("Invalid type when trying to multiselect")))
	{
		return;
	}

	constexpr bool bNotifySelectionChange = false;
	RemoveInvalidObjectsFromSelection(bNotifySelectionChange);

	// Normal selection if nothing is selected or there's no valid anchor
	if (!MultiSelectAnchor.IsValid() ||
		(SelectedFaderGroups.IsEmpty() && SelectedElementControllers.IsEmpty()))
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = Cast<UDMXControlConsoleFaderGroup>(ElementControllerOrFaderGroupObject))
		{
			constexpr bool bNotifyFaderGroupSelectionChange = false;
			AddToSelection(FaderGroup, bNotifyFaderGroupSelectionChange);
		}
		else if (UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(ElementControllerOrFaderGroupObject))
		{
			constexpr bool bFaderSelectionChange = false;
			AddToSelection(ElementController, bFaderSelectionChange);
		}
		return;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	TArray<UObject*> ElementControllersAndFaderGroups;
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup> AnyFaderGroup : ActiveLayout->GetAllFaderGroups())
	{
		if (!AnyFaderGroup.IsValid())
		{
			continue;
		}

		ElementControllersAndFaderGroups.AddUnique(AnyFaderGroup.Get());
		for (UDMXControlConsoleElementController* AnyElementController : AnyFaderGroup->GetAllElementControllers())
		{
			ElementControllersAndFaderGroups.AddUnique(AnyElementController);
		}
	}

	const int32 IndexOfFaderGroupAnchor = ElementControllersAndFaderGroups.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});
	const int32 IndexOfFaderAnchor = ElementControllersAndFaderGroups.IndexOfByPredicate([this](const UObject* Object)
		{
			return MultiSelectAnchor == Object;
		});

	const int32 IndexOfAnchor = FMath::Max(IndexOfFaderGroupAnchor, IndexOfFaderAnchor);
	if (!ensureAlwaysMsgf(IndexOfAnchor != INDEX_NONE, TEXT("No previous selection when multi selecting, cannot multiselect.")))
	{
		return;
	}

	const int32 IndexOfSelection = ElementControllersAndFaderGroups.IndexOfByKey(ElementControllerOrFaderGroupObject);

	const int32 StartIndex = FMath::Min(IndexOfAnchor, IndexOfSelection);
	const int32 EndIndex = FMath::Max(IndexOfAnchor, IndexOfSelection);

	SelectedFaderGroups.Reset();
	SelectedElementControllers.Reset();
	for (int32 IndexToSelect = StartIndex; IndexToSelect <= EndIndex; IndexToSelect++)
	{
		if (!ensureMsgf(ElementControllersAndFaderGroups.IsValidIndex(IndexToSelect), TEXT("Invalid index when multiselecting")))
		{
			break;
		}

		if (UDMXControlConsoleFaderGroup* FaderGroupToSelect = Cast<UDMXControlConsoleFaderGroup>(ElementControllersAndFaderGroups[IndexToSelect]))
		{
			if (FaderGroupToSelect  && FaderGroupToSelect->IsActive() && FaderGroupToSelect->IsMatchingFilter())
			{
				SelectedFaderGroups.AddUnique(FaderGroupToSelect);
			}
		}
		else if (UDMXControlConsoleElementController* ElementControllerToSelect = Cast<UDMXControlConsoleElementController>(ElementControllersAndFaderGroups[IndexToSelect]))
		{
			if (ElementControllerToSelect && ElementControllerToSelect->IsActive() && ElementControllerToSelect->IsMatchingFilter())
			{
				SelectedElementControllers.AddUnique(ElementControllerToSelect);
			}
		}
	}
	if (!SelectedElementControllers.IsEmpty())
	{
		// Always select the fader group of the first selected element controller
		UDMXControlConsoleElementController* FirstSelectedElementController = CastChecked<UDMXControlConsoleElementController>(SelectedElementControllers[0]);
		SelectedFaderGroups.AddUnique(&FirstSelectedElementController->GetOwnerFaderGroupChecked());
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!FaderGroup || !IsSelected(FaderGroup))
	{
		return;
	}

	RemoveFromSelection(FaderGroup);

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllActiveFaderGroups = ActiveLayout->GetAllActiveFaderGroups();
	if (AllActiveFaderGroups.Num() <= 1)
	{
		return;
	}

	const int32 Index = AllActiveFaderGroups.IndexOfByKey(FaderGroup);

	int32 NewIndex = Index - 1;
	if (!AllActiveFaderGroups.IsValidIndex(NewIndex))
	{
		NewIndex = Index + 1;
	}

	const TWeakObjectPtr<UDMXControlConsoleFaderGroup> NewSelectedFaderGroup = AllActiveFaderGroups.IsValidIndex(NewIndex) ? AllActiveFaderGroups[NewIndex] : nullptr;
	if (!NewSelectedFaderGroup.IsValid())
	{
		return;
	}

	AddToSelection(NewSelectedFaderGroup.Get());
}

void FDMXControlConsoleEditorSelection::ReplaceInSelection(UDMXControlConsoleElementController* ElementController)
{
	if (!ElementController || !IsSelected(ElementController))
	{
		return;
	}

	RemoveFromSelection(ElementController);

	const UDMXControlConsoleFaderGroup& FaderGroup = ElementController->GetOwnerFaderGroupChecked();
	const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroup.GetAllElementControllers();
	if (AllElementControllers.Num() <= 1)
	{
		return;
	}

	const int32 IndexToReplace = AllElementControllers.IndexOfByKey(ElementController);
	int32 NewIndex = IndexToReplace - 1;
	if (!AllElementControllers.IsValidIndex(NewIndex))
	{
		NewIndex = IndexToReplace + 1;
	}

	UDMXControlConsoleElementController* NewSelectedElementController = AllElementControllers.IsValidIndex(NewIndex) ? AllElementControllers[NewIndex] : nullptr;
	AddToSelection(NewSelectedElementController);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	return SelectedFaderGroups.Contains(FaderGroup);
}

bool FDMXControlConsoleEditorSelection::IsSelected(UDMXControlConsoleElementController* ElementController) const
{
	return SelectedElementControllers.Contains(ElementController);
}

void FDMXControlConsoleEditorSelection::SelectAll(bool bOnlyMatchingFilter)
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	ClearSelection(false);

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (FaderGroup.IsValid() && FaderGroup->IsActive())
		{
			constexpr bool bNotifyFaderSelectionChange = false;
			AddAllFadersFromFaderGroupToSelection(FaderGroup.Get(), bOnlyMatchingFilter, bNotifyFaderSelectionChange);
		}
	}

	OnSelectionChanged.Broadcast();
}

void FDMXControlConsoleEditorSelection::RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroups.Remove(nullptr);
	SelectedElementControllers.Remove(nullptr);

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearElementControllersSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange)
{
	if (!FaderGroup || !SelectedFaderGroups.Contains(FaderGroup))
	{
		return;
	}

	TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroup->GetAllElementControllers();

	auto IsFaderGroupOwnerLambda = [AllElementControllers](const TWeakObjectPtr<UObject> SelectedObject)
	{
		const UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedObject);
		if (!SelectedElementController)
		{
			return true;
		}

		if (AllElementControllers.Contains(SelectedElementController))
		{
			return true;
		}

		return false;
	};

	SelectedElementControllers.RemoveAll(IsFaderGroupOwnerLambda);

	if (!MultiSelectAnchor.IsValid() || MultiSelectAnchor->IsA(UDMXControlConsoleElementController::StaticClass()))
	{
		UpdateMultiSelectAnchor(UDMXControlConsoleElementController::StaticClass());
	}

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

void FDMXControlConsoleEditorSelection::ClearSelection(bool bNotifySelectionChange)
{
	SelectedFaderGroups.Reset();
	SelectedElementControllers.Reset();

	if (bNotifySelectionChange)
	{
		OnSelectionChanged.Broadcast();
	}
}

UDMXControlConsoleFaderGroup* FDMXControlConsoleEditorSelection::GetFirstSelectedFaderGroup(bool bReverse) const
{
	TArray<TWeakObjectPtr<UObject>> CurrentSelectedFaderGroups = GetSelectedFaderGroups();
	if (CurrentSelectedFaderGroups.IsEmpty())
	{
		return nullptr;
	}

	auto SortSelectedFaderGroupsLambda = [](TWeakObjectPtr<UObject> FaderGroupObjectA, TWeakObjectPtr<UObject> FaderGroupObjectB)
		{
			const UDMXControlConsoleFaderGroup* FaderGroupA = Cast<UDMXControlConsoleFaderGroup>(FaderGroupObjectA);
			const UDMXControlConsoleFaderGroup* FaderGroupB = Cast<UDMXControlConsoleFaderGroup>(FaderGroupObjectB);

			if (!FaderGroupA || !FaderGroupB)
			{
				return false;
			}

			const int32 RowIndexA = FaderGroupA->GetOwnerFaderGroupRowChecked().GetRowIndex();
			const int32 RowIndexB = FaderGroupB->GetOwnerFaderGroupRowChecked().GetRowIndex();

			if (RowIndexA != RowIndexB)
			{
				return RowIndexA < RowIndexB;
			}

			const int32 IndexA = FaderGroupA->GetIndex();
			const int32 IndexB = FaderGroupB->GetIndex();

			return IndexA < IndexB;
		};

	Algo::Sort(CurrentSelectedFaderGroups, SortSelectedFaderGroupsLambda);
	const TWeakObjectPtr<UObject> FirstFaderGroup = bReverse ? CurrentSelectedFaderGroups.Last() : CurrentSelectedFaderGroups[0];
	return Cast<UDMXControlConsoleFaderGroup>(FirstFaderGroup);
}

UDMXControlConsoleElementController* FDMXControlConsoleEditorSelection::GetFirstSelectedElementController(bool bReverse) const
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return nullptr;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return nullptr;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
	if (AllFaderGroups.IsEmpty())
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<UObject>> CurrentSelectedElementControllers = GetSelectedElementControllers();
	if (CurrentSelectedElementControllers.IsEmpty())
	{
		return nullptr;
	}

	const auto SortSelectedElementControllerLambda = [AllFaderGroups](const TWeakObjectPtr<UObject>& ElementControllerObjectA, const TWeakObjectPtr<UObject>& ElementControllerObjectB)
		{
			const UDMXControlConsoleElementController* ElementControllerA = Cast<UDMXControlConsoleElementController>(ElementControllerObjectA);
			const UDMXControlConsoleElementController* ElementControllerB = Cast<UDMXControlConsoleElementController>(ElementControllerObjectB);
			if (!ElementControllerA || !ElementControllerB)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroup& FaderGroupA = ElementControllerA->GetOwnerFaderGroupChecked();
			const UDMXControlConsoleFaderGroup& FaderGroupB = ElementControllerB->GetOwnerFaderGroupChecked();

			const int32 FaderGroupIndexA = AllFaderGroups.IndexOfByKey(&FaderGroupA);
			const int32 FaderGroupIndexB = AllFaderGroups.IndexOfByKey(&FaderGroupB);

			if (FaderGroupIndexA != FaderGroupIndexB)
			{
				return FaderGroupIndexA < FaderGroupIndexB;
			}

			const int32 IndexA = FaderGroupA.GetAllElementControllers().IndexOfByKey(ElementControllerA);
			const int32 IndexB = FaderGroupB.GetAllElementControllers().IndexOfByKey(ElementControllerB);

			return IndexA < IndexB;
		};

	Algo::Sort(CurrentSelectedElementControllers, SortSelectedElementControllerLambda);
	const TWeakObjectPtr<UObject> FirstElementController = bReverse ? CurrentSelectedElementControllers.Last() : CurrentSelectedElementControllers[0];
	return Cast<UDMXControlConsoleElementController>(FirstElementController);
}

TArray<UDMXControlConsoleElementController*> FDMXControlConsoleEditorSelection::GetSelectedElementControllersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	TArray<UDMXControlConsoleElementController*> CurrentSelectedElementControllers;

	if (!FaderGroup)
	{
		return CurrentSelectedElementControllers;
	}

	TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroup->GetAllElementControllers();
	for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
	{
		if (!ElementController)
		{
			continue;
		}

		if (!SelectedElementControllers.Contains(ElementController))
		{
			continue;
		}

		CurrentSelectedElementControllers.Add(ElementController);
	}

	return CurrentSelectedElementControllers;
}

TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> FDMXControlConsoleEditorSelection::GetSelectedElements(bool bSort) const
{
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> SelectedElements;
	for (const TWeakObjectPtr<UObject> SelectedElementControllerObject : SelectedElementControllers)
	{
		UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
		if (!SelectedElementController || !SelectedElementController->IsMatchingFilter())
		{
			continue;
		}

		SelectedElements.Append(SelectedElementController->GetElements());
	}

	if (bSort)
	{
		const auto SortElementsByStartingAddressLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement)
			{
				return InElement->GetStartingAddress();
			};

		Algo::SortBy(SelectedElements, SortElementsByStartingAddressLambda);
	}

	return SelectedElements;
}

void FDMXControlConsoleEditorSelection::UpdateMultiSelectAnchor(UClass* PreferedClass)
{
	if (!ensureMsgf(PreferedClass == UDMXControlConsoleFaderGroup::StaticClass() || PreferedClass == UDMXControlConsoleElementController::StaticClass(), TEXT("Invalid class when trying to update multi select anchor")))
	{
		return;
	}

	if (PreferedClass == UDMXControlConsoleFaderGroup::StaticClass() && !SelectedFaderGroups.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroups.Last();
	}
	else if (!SelectedElementControllers.IsEmpty())
	{
		MultiSelectAnchor = SelectedElementControllers.Last();
	}
	else if (!SelectedFaderGroups.IsEmpty())
	{
		MultiSelectAnchor = SelectedFaderGroups.Last();
	}
	else
	{
		MultiSelectAnchor = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
