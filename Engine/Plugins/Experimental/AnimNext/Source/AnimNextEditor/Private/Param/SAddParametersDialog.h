// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Views/SListView.h"
#include "Param/ParameterPickerArgs.h"

class SWrapBox;
class UAnimNextParameterLibrary;
class UAnimNextModule_EditorData;

namespace UE::AnimNext::Editor
{
	struct FParameterToAdd;


class SAddParametersDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SAddParametersDialog)
		: _AllowMultiple(true)
	{}

	/** Whether we allow multiple parameters to be added or just one at a time */
	SLATE_ARGUMENT(bool, AllowMultiple)

	/** Delegate called to filter parameters by type for display to the user */
	SLATE_EVENT(FOnFilterParameterType, OnFilterParameterType)

	/** Initial parameter type to use */
	SLATE_ARGUMENT(FAnimNextParamType, InitialParamType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetData& InAsset);

	bool ShowModal(TArray<FParameterToAdd>& OutParameters);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void AddEntry(const FAnimNextParamType& InParamType = FAnimNextParamType());

	void RefreshEntries();

	struct FParameterToAddEntry : FParameterToAdd
	{
		FParameterToAddEntry() = default;

		FParameterToAddEntry(const FAnimNextParamType& InType, FName InName)
			: FParameterToAdd(InType, InName)
		{}

		bool bIsNew = true;
	};

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterToAddEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	TSharedRef<SWidget> HandleGetAddParameterMenuContent(TSharedPtr<FParameterToAddEntry> InEntry);
	
private:
	friend class SParameterToAdd;

	TSharedPtr<SListView<TSharedRef<FParameterToAddEntry>>> EntriesList;

	TArray<TSharedRef<FParameterToAddEntry>> Entries;

	FOnFilterParameterType OnFilterParameterType;

	FAssetData Asset;

	bool bOKPressed = false;
};

}
