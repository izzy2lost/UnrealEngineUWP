// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureActionColumn.h"
#include "RCSignatureAction.h"
#include "RCSignatureActionType.h"
#include "SRCSignatureActionBox.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "RCSignatureActionColumn"

FRCSignatureActionColumn::FRCSignatureActionColumn(const TAttribute<bool>& InLiveMode)
	: LiveMode(InLiveMode)
{
	RefreshActionTypes();
}

FName FRCSignatureActionColumn::GetColumnId() const
{
	return TEXT("FRCSignatureActionColumn");
}

bool FRCSignatureActionColumn::ShouldShowColumnByDefault() const
{
	return true;
}

SHeaderRow::FColumn::FArguments FRCSignatureActionColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.FillWidth(0.5f)
		.DefaultLabel(LOCTEXT("DisplayName", "Actions"));
}

TSharedRef<SWidget> FRCSignatureActionColumn::ConstructRowWidget(TSharedPtr<FRCSignatureTreeItemBase> InItem, const TSharedRef<SRCSignatureTree>& InList, const TSharedRef<SRCSignatureRow>& InRow)
{
	return SNew(SRCSignatureActionBox, InItem.ToSharedRef(), InRow)
		.LiveMode(LiveMode)
		.ActionTypesSource(&ActionTypes)
		.OnActionTypesComboBoxOpening(this, &FRCSignatureActionColumn::RefreshActionTypes);
}

void FRCSignatureActionColumn::RefreshActionTypes()
{
	ActionTypes.Reset();

	if (!UObjectInitialized())
	{
		return;
	}

	const FName HiddenMetaData(TEXT("Hidden"));

	for (UScriptStruct* ScriptStruct : TObjectRange<UScriptStruct>())
	{
		if (ScriptStruct->HasMetaData(HiddenMetaData))
		{
			continue;				
		}

		if (ScriptStruct->IsChildOf(TBaseStructure<FRCSignatureAction>::Get()))
		{
			TSharedRef<FRCSignatureActionType> ActionType = MakeShared<FRCSignatureActionType>();
			ActionType->Type = ScriptStruct;
			ActionType->Title = ScriptStruct->GetDisplayNameText();

			// Initialize a Temp Instance to get the Icon to use
			{
				TInstancedStruct<FRCSignatureAction> Instance;
				Instance.InitializeAsScriptStruct(ScriptStruct, /*StructMemory*/nullptr);
				ActionType->Icon = Instance.Get().GetIcon();
			}

			ActionTypes.Add(MoveTemp(ActionType));
		}
	}
}

#undef LOCTEXT_NAMESPACE
