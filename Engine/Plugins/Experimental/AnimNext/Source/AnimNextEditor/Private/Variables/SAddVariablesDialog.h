// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Views/SListView.h"

class SWrapBox;
class UAnimNextModule_EditorData;

namespace UE::AnimNext::Editor
{

struct FVariableToAdd
{
	FVariableToAdd() = default;

	FVariableToAdd(const FAnimNextParamType& InType, FName InName)
		: Type(InType)
		, Name(InName)
	{}

	bool IsValid() const
	{
		return Name != NAME_None && Type.IsValid(); 
	}

	bool IsValid(FText& OutReason) const;

	// Type
	FAnimNextParamType Type;

	// Name for variable
	FName Name;
};

// Result of a filter operation via FOnFilterVariableType
enum class EFilterVariableResult : int32
{
	Include,
	Exclude
};

// Delegate called to filter variables by type for display to the user
using FOnFilterVariableType = TDelegate<EFilterVariableResult(const FAnimNextParamType& /*InType*/)>;

class SAddVariablesDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SAddVariablesDialog)
		: _AllowMultiple(true)
	{}

	/** Whether we allow multiple variables to be added or just one at a time */
	SLATE_ARGUMENT(bool, AllowMultiple)

	/** Delegate called to filter variables by type for display to the user */
	SLATE_EVENT(FOnFilterVariableType, OnFilterVariableType)

	/** Initial variable type to use */
	SLATE_ARGUMENT(FAnimNextParamType, InitialParamType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetData& InAsset);

	bool ShowModal(TArray<FVariableToAdd>& OutVariables);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void AddEntry(const FAnimNextParamType& InParamType = FAnimNextParamType());

	void RefreshEntries();

	struct FVariableToAddEntry : FVariableToAdd
	{
		FVariableToAddEntry() = default;

		FVariableToAddEntry(const FAnimNextParamType& InType, FName InName)
			: FVariableToAdd(InType, InName)
		{}

		bool bIsNew = true;
	};

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FVariableToAddEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	TSharedRef<SWidget> HandleGetAddVariableMenuContent(TSharedPtr<FVariableToAddEntry> InEntry);
	
private:
	friend class SVariableToAdd;

	TSharedPtr<SListView<TSharedRef<FVariableToAddEntry>>> EntriesList;

	TArray<TSharedRef<FVariableToAddEntry>> Entries;

	FOnFilterVariableType OnFilterVariableType;

	FAssetData Asset;

	bool bOKPressed = false;
};

}
