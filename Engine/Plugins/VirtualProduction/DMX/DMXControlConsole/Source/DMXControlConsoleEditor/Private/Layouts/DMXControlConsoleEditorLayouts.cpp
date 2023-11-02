// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorLayouts.h"

#include "Algo/Find.h"
#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorLayouts"

UDMXControlConsoleEditorLayouts::UDMXControlConsoleEditorLayouts()
{
	DefaultLayout = CreateDefaultSubobject<UDMXControlConsoleEditorGlobalLayoutBase>(TEXT("DefaultLayout"));
}

UDMXControlConsoleEditorGlobalLayoutBase* UDMXControlConsoleEditorLayouts::AddUserLayout(const FString& LayoutName)
{
	UDMXControlConsoleEditorGlobalLayoutBase* UserLayout = NewObject<UDMXControlConsoleEditorGlobalLayoutBase>(this, NAME_None, RF_Transactional);
	FString NewLayoutName = LayoutName;
	if (NewLayoutName.IsEmpty() || FindUserLayoutByName(NewLayoutName))
	{
		const int32 Index = UserLayouts.Num();
		NewLayoutName = FString::Format(TEXT("Layout {0}"), { Index });
	}
	UserLayout->LayoutName = NewLayoutName;

	UserLayouts.Add(UserLayout);

	return UserLayout;
}

void UDMXControlConsoleEditorLayouts::DeleteUserLayout(UDMXControlConsoleEditorGlobalLayoutBase* UserLayout)
{
	if (!ensureMsgf(UserLayout, TEXT("Invalid layout, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(UserLayouts.Contains(UserLayout), TEXT("'%s' is not owner of '%s'. Cannot delete layout correctly."), *GetName(), *UserLayout->LayoutName))
	{
		return;
	}

	UserLayouts.Remove(UserLayout);
}

UDMXControlConsoleEditorGlobalLayoutBase* UDMXControlConsoleEditorLayouts::FindUserLayoutByName(const FString& LayoutName) const
{
	const TObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>* UserLayout = Algo::FindByPredicate(UserLayouts, [LayoutName](const UDMXControlConsoleEditorGlobalLayoutBase* CustomLayout)
		{
			return IsValid(CustomLayout) && CustomLayout->LayoutName == LayoutName;
		});

	return UserLayout ? UserLayout->Get() : nullptr;
}

void UDMXControlConsoleEditorLayouts::ClearUserLayouts()
{
	ActiveLayout = DefaultLayout;
	UserLayouts.Reset();
}

UDMXControlConsoleEditorGlobalLayoutBase& UDMXControlConsoleEditorLayouts::GetDefaultLayoutChecked() const
{
	checkf(DefaultLayout, TEXT("Invalid default layout, cannot get from '%s'."), *GetName());
	return *DefaultLayout;
}

void UDMXControlConsoleEditorLayouts::SetActiveLayout(UDMXControlConsoleEditorGlobalLayoutBase* InLayout)
{
	if (InLayout && InLayout != ActiveLayout)
	{
		ActiveLayout = InLayout;
		ActiveLayout->SetActiveFaderGroupsInLayout(true);

		OnActiveLayoutChanged.Broadcast();
		OnLayoutModeChanged.Broadcast();
	}
}

void UDMXControlConsoleEditorLayouts::UpdateDefaultLayout()
{
	if (!DefaultLayout)
	{
		return;
	}

	const UDMXControlConsole* OwnerConsole = Cast<UDMXControlConsole>(GetOuter());
	if (!ensureMsgf(OwnerConsole, TEXT("Invalid outer for '%s', cannot update layouts correctly."), *GetName()))
	{
		return;
	}

	DefaultLayout->Modify();
	DefaultLayout->GenerateLayoutByControlConsoleData(OwnerConsole->GetControlConsoleData());
}

void UDMXControlConsoleEditorLayouts::BeginDestroy()
{
	Super::BeginDestroy();

	if (!DefaultLayout || !DefaultLayout->IsRegistered())
	{  
		return;
	}

	const UDMXControlConsole* OwnerConsole = Cast<UDMXControlConsole>(GetOuter());
	if (!ensureMsgf(OwnerConsole, TEXT("Invalid outer for '%s', cannot destroy layouts correctly."), *GetName()))
	{
		return;
	}

	if (UDMXControlConsoleData* ControlConsoleData = OwnerConsole->GetControlConsoleData())
	{
		DefaultLayout->Unregister(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorLayouts::PostLoad()
{
	Super::PostLoad();

	UserLayouts.Remove(nullptr);

	if (!DefaultLayout)
	{
		DefaultLayout = NewObject<UDMXControlConsoleEditorGlobalLayoutBase>(this, TEXT("DefaultLayout"), RF_Transactional);
	}

	if (!ActiveLayout)
	{
		SetActiveLayout(DefaultLayout);
	}
}

#undef LOCTEXT_NAMESPACE
