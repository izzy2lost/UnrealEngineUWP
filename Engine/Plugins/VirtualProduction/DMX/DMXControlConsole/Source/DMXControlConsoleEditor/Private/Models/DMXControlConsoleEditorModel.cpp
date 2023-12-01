// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorModel.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Editor.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXLibrary.h"
#include "Models/Filter/FilterModel.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"
#include "Toolkits/DMXControlConsoleEditorToolkit.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorModel"

void UDMXControlConsoleEditorModel::Initialize(const TSharedPtr<UE::DMX::Private::FDMXControlConsoleEditorToolkit>& InToolkit)
{
	checkf(InToolkit.IsValid(), TEXT("Invalid control console toolkit, can't initialize toolkit correctly."));
	WeakToolkit = InToolkit;
	ControlConsole = InToolkit->GetControlConsole();

	InitializeEditorLayouts();
	InitializeEditorData();
	BindToDMXLibraryChanges();
	LoadConfig();
}

UDMXControlConsoleData* UDMXControlConsoleEditorModel::GetControlConsoleData() const
{ 
	return ControlConsole.IsValid() ? ControlConsole->GetControlConsoleData() : nullptr;
}

UDMXControlConsoleEditorData* UDMXControlConsoleEditorModel::GetControlConsoleEditorData() const
{
	return ControlConsole.IsValid() ? Cast<UDMXControlConsoleEditorData>(ControlConsole->ControlConsoleEditorData) : nullptr;
}

UDMXControlConsoleEditorLayouts* UDMXControlConsoleEditorModel::GetControlConsoleLayouts() const
{
	return ControlConsole.IsValid() ? Cast<UDMXControlConsoleEditorLayouts>(ControlConsole->ControlConsoleEditorLayouts) : nullptr;
}

TSharedRef<FDMXControlConsoleEditorSelection> UDMXControlConsoleEditorModel::GetSelectionHandler()
{
	if (!SelectionHandler.IsValid())
	{
		SelectionHandler = MakeShared<FDMXControlConsoleEditorSelection>(this);
	}

	return SelectionHandler.ToSharedRef();
}

TSharedRef<UE::DMX::Private::FFilterModel> UDMXControlConsoleEditorModel::GetFilterModel()
{
	using namespace UE::DMX::Private;
	if (!FilterModel.IsValid())
	{
		FilterModel = MakeShared<FFilterModel>(this);
		FilterModel->Initialize();
	}

	return FilterModel.ToSharedRef();
}

void UDMXControlConsoleEditorModel::ScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	OnScrollFaderGroupIntoView.Broadcast(FaderGroup);
}

void UDMXControlConsoleEditorModel::RequestUpdateEditorModel()
{
	if (!UpdateEditorModelTimerHandle.IsValid())
	{
		UpdateEditorModelTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UDMXControlConsoleEditorModel::UpdateEditorModel));
	}
}

void UDMXControlConsoleEditorModel::PostInitProperties()
{
	Super::PostInitProperties();

	// Deffer initialization to engine being fully loaded
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UDMXControlConsoleEditorModel::OnEnginePreExit);
}

void UDMXControlConsoleEditorModel::BeginDestroy()
{
	Super::BeginDestroy();

	UnregisterEditorLayouts();
	UnbindFromDMXLibraryChanges();

	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (ControlConsoleData && ControlConsoleData->IsSendingDMX())
	{
		ControlConsoleData->StopSendingDMX();
	}
}

void UDMXControlConsoleEditorModel::UpdateEditorModel()
{
	UpdateEditorModelTimerHandle.Invalidate();

	OnEditorModelUpdated.Broadcast();
}

void UDMXControlConsoleEditorModel::BindToDMXLibraryChanges()
{
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (ControlConsoleData && !ControlConsoleData->GetOnDMXLibraryChanged().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnDMXLibraryChanged().AddUObject(this, &UDMXControlConsoleEditorModel::OnDMXLibraryChanged);
	}
}

void UDMXControlConsoleEditorModel::UnbindFromDMXLibraryChanges()
{
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (ControlConsoleData && ControlConsoleData->GetOnDMXLibraryChanged().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnDMXLibraryChanged().RemoveAll(this);
	}
}

void UDMXControlConsoleEditorModel::InitializeEditorData() const
{
	if (!ControlConsole.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorData* ControlConsoleEditorData = Cast<UDMXControlConsoleEditorData>(ControlConsole->ControlConsoleEditorData);
	if (!ControlConsoleEditorData)
	{
		ControlConsoleEditorData = NewObject<UDMXControlConsoleEditorData>(ControlConsole.Get(), NAME_None, RF_Transactional);
		ControlConsole->ControlConsoleEditorData = ControlConsoleEditorData;
	}
}

void UDMXControlConsoleEditorModel::InitializeEditorLayouts() const
{
	if (!ControlConsole.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = Cast<UDMXControlConsoleEditorLayouts>(ControlConsole->ControlConsoleEditorLayouts);
	if (!ControlConsoleLayouts)
	{
		ControlConsoleLayouts = NewObject<UDMXControlConsoleEditorLayouts>(ControlConsole.Get(), NAME_None, RF_Transactional);
		ControlConsole->ControlConsoleEditorLayouts = ControlConsoleLayouts;

		ControlConsoleLayouts->UpdateDefaultLayout();
		ControlConsoleLayouts->SetActiveLayout(&ControlConsoleLayouts->GetDefaultLayoutChecked());
	}

	UDMXControlConsoleEditorGlobalLayoutBase& DefaultLayout = ControlConsoleLayouts->GetDefaultLayoutChecked();
	if (DefaultLayout.GetLayoutRows().IsEmpty())
	{
		ControlConsoleLayouts->UpdateDefaultLayout();
		ControlConsoleLayouts->SetActiveLayout(&DefaultLayout);
	}

	RegisterEditorLayouts();
}

void UDMXControlConsoleEditorModel::RegisterEditorLayouts() const
{
	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase& DefaultLayout = ControlConsoleLayouts->GetDefaultLayoutChecked();
	if (!DefaultLayout.IsRegistered())
	{
		DefaultLayout.Register(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorModel::UnregisterEditorLayouts() const
{
	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase& DefaultLayout = ControlConsoleLayouts->GetDefaultLayoutChecked();
	if (DefaultLayout.IsRegistered())
	{
		DefaultLayout.Unregister(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorModel::OnDMXLibraryChanged()
{
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	// Clear user layouts from patched fader groups
	const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
	for (UDMXControlConsoleEditorGlobalLayoutBase* UserLayout : UserLayouts)
	{
		if (!UserLayout)
		{
			continue;
		}

		UserLayout->PreEditChange(nullptr);
		constexpr bool bOnlyPatchedFaderGroups = true;
		UserLayout->ClearAll(bOnlyPatchedFaderGroups);
		UserLayout->PostEditChange();
	}

	// Regenerate control console data with new library data
	ControlConsoleData->PreEditChange(nullptr);
	ControlConsoleData->ClearPatchedFaderGroups();
	ControlConsoleData->GenerateFromDMXLibrary();
	ControlConsoleData->PostEditChange();

	// Update current console default layout
	ControlConsoleLayouts->PreEditChange(nullptr);
	ControlConsoleLayouts->UpdateDefaultLayout();
	ControlConsoleLayouts->PostEditChange();

	RequestUpdateEditorModel();
}

void UDMXControlConsoleEditorModel::OnEnginePreExit()
{
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (ControlConsoleData)
	{
		ControlConsoleData->StopSendingDMX();
	}
}

#undef LOCTEXT_NAMESPACE
