// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/LabelWidget.h"

#include "ActorEditorUtils.h"
#include "Columns/SlateDelegateColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiEditableCapability.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiTextCapability.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiTooltipCapability.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiStyleOverrideCapability.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "TedsUI_LabelWidget"

//
// ULabelWidgetFactory
//

namespace UE::Editor::DataStorage
{
	namespace UI::Private
	{
		static void UpdateTextWidget(const TWeakPtr<SWidget>& Widget, const FTypedElementLabelColumn& Label, const uint64* HashValue)
		{
			TSharedPtr<SWidget> WidgetPointer = Widget.Pin();
			if (ensureMsgf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
				"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
				"references.")))
			{
				if (TSharedPtr<ITypedElementUiTextCapability> Text = WidgetPointer->GetMetaData<ITypedElementUiTextCapability>())
				{
					Text->SetText(FText::FromString(Label.Label));
				}

				if (TSharedPtr<ITypedElementUiTooltipCapability> ToolTip = WidgetPointer->GetMetaData<ITypedElementUiTooltipCapability>())
				{
					if (!HashValue)
					{
						ToolTip->SetToolTipText(FText::FromString(Label.Label));
					}
					else
					{
						ToolTip->SetToolTipText(FText::FromString(FString::Format(TEXT("{0}\nHash: {1}"), { Label.Label, *HashValue })));
					}
				}
			}
		}

		static void SyncColumnsToWidget(
			ITypedElementDataStorageInterface* DataStorage,
			RowHandle TargetRow,
			FTypedElementU64IntValueCacheColumn& TextHash,
			const TWeakPtr<SWidget>& Widget,
			bool bShowHashInToolTip)
		{
			if (const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
			{
				if (const FTypedElementLabelHashColumn* LabelHashColumn = DataStorage->GetColumn<FTypedElementLabelHashColumn>(TargetRow))
				{
					if (LabelHashColumn->LabelHash != TextHash.Value)
					{
						UpdateTextWidget(Widget, *LabelColumn, bShowHashInToolTip ? &LabelHashColumn->LabelHash : nullptr);
						TextHash.Value = LabelHashColumn->LabelHash;
					}
				}
				else
				{
					UpdateTextWidget(Widget, *LabelColumn, nullptr);
				}
			}
		}
	} // namespace UI::Private
} // namespace UE::Editor::DataStorage

void ULabelWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;
	using namespace UE::Editor;
	
	DataStorage::QueryHandle UpdateLabelWidget = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementLabelColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.None<FTypedElementLabelHashColumn>()
		.Compile());

	DataStorage::QueryHandle UpdateLabelAndHashWidget = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementLabelColumn, FTypedElementLabelHashColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync label to widget"),
			FProcessor(EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncWidgets))
				.SetExecutionMode(EExecutionMode::GameThread),
			[](
				IQueryContext& Context, 
				FTypedElementSlateWidgetReferenceColumn& Widget,
				FTypedElementU64IntValueCacheColumn& TextHash,
				const FLabelWidgetColumn& Config,
				const FTypedElementRowReferenceColumn& Target)
			{
				Context.RunSubquery(0, Target.Row, CreateSubqueryCallbackBinding(
					[&Widget](const FTypedElementLabelColumn& Label)
					{
						DataStorage::UI::Private::UpdateTextWidget(Widget.Widget, Label, nullptr);
					}));
				Context.RunSubquery(1, Target.Row, CreateSubqueryCallbackBinding(
					[&Widget, &TextHash, &Config](const FTypedElementLabelColumn& Label, const FTypedElementLabelHashColumn& Hash)
					{
						if (Hash.LabelHash != TextHash.Value)
						{
							DataStorage::UI::Private::UpdateTextWidget(Widget.Widget, Label, Config.bShowHashInTooltip ? &Hash.LabelHash : nullptr);
							TextHash.Value = Hash.LabelHash;
						}
					}));
			})
		.DependsOn()
			.SubQuery({ UpdateLabelWidget, UpdateLabelAndHashWidget })
		.Compile()
	);
}

void ULabelWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory<FLabelWidgetConstructor>(FName(TEXT("General.Cell")), 
		TypedElementDataStorage::FColumn<FTypedElementLabelColumn>() || (TypedElementDataStorage::FColumn<FTypedElementLabelColumn>() && TypedElementDataStorage::FColumn<FTypedElementLabelHashColumn>()));
}



//
// FLabelWidgetConstructor
//

FLabelWidgetConstructor::FLabelWidgetConstructor()
	: Super(FLabelWidgetConstructor::StaticStruct())
{
}

FLabelWidgetConstructor::FLabelWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(InTypeInfo)
{
}

TConstArrayView<const UScriptStruct*> FLabelWidgetConstructor::GetAdditionalColumnsList() const
{
	static const TTypedElementColumnTypeList<
		FTypedElementRowReferenceColumn,
		FTypedElementU64IntValueCacheColumn,
		FLabelWidgetColumn,
		FExternalWidgetSelectionColumn> Columns;
	return Columns;
}

TSharedPtr<SWidget> FLabelWidgetConstructor::Construct(
	UE::Editor::DataStorage::RowHandle Row,
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	TSharedPtr<SWidget> Result;
	const bool* IsEditable = Arguments.FindForColumn<FTypedElementLabelColumn>(TypedElementDataStorage::IsEditableName).TryGetExact<bool>();
	if (IsEditable && *IsEditable)
	{
		if (FTypedElementRowReferenceColumn* TargetRowColumn = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row))
		{
			TSharedPtr<SInlineEditableTextBlock> TextBlock = SNew(SInlineEditableTextBlock)
				.OnTextCommitted_Lambda(
					[DataStorage, TargetRow = TargetRowColumn->Row](const FText& NewText, ETextCommit::Type CommitInfo)
					{
						// This callback happens on the game thread so it's safe to directly call into the data storage.
						FString NewLabelText = NewText.ToString();
						if (FTypedElementLabelHashColumn* LabelHashColumn = DataStorage->GetColumn<FTypedElementLabelHashColumn>(TargetRow))
						{
							LabelHashColumn->LabelHash = CityHash64(reinterpret_cast<const char*>(*NewLabelText), NewLabelText.Len() * sizeof(**NewLabelText));
						}
						if (FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
						{
							LabelColumn->Label = MoveTemp(NewLabelText);
						}
						DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(TargetRow);
					})
				.OnVerifyTextChanged_Lambda([](const FText& Label, FText& ErrorMessage)
					{
						// Note: The use of actor specific functionality should be minimized, but this function acts generic enough that the 
						// use of actor is just in names.
						return FActorEditorUtils::ValidateActorName(Label, ErrorMessage);
					})
				.IsSelected_Static(&FLabelWidgetConstructor::IsWidgetSelected, DataStorage, Row);;
			TextBlock->AddMetadata(MakeShared<TTypedElementUiEditableCapability<SInlineEditableTextBlock>>(*TextBlock));
			TextBlock->AddMetadata(MakeShared<TTypedElementUiTextCapability<SInlineEditableTextBlock>>(*TextBlock));
			TextBlock->AddMetadata(MakeShared<TTypedElementUiTooltipCapability<SInlineEditableTextBlock>>(*TextBlock));
			TextBlock->AddMetadata(MakeShared<TTypedElementUiStyleOverrideCapability<SInlineEditableTextBlock>>(*TextBlock));
			Result = TextBlock;
		}
	}
	else
	{
		TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
			.IsEnabled(false);
		TextBlock->AddMetadata(MakeShared<TTypedElementUiTextCapability<STextBlock>>(*TextBlock));
		TextBlock->AddMetadata(MakeShared<TTypedElementUiTooltipCapability<STextBlock>>(*TextBlock));
		Result = TextBlock;
	}
	
	if (Result)
	{
		DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row)->Widget = Result;
		if (SetColumns(DataStorage, Row))
		{
			if (FinalizeWidget(DataStorage, DataStorageUi, Row, Result))
			{
				AddDefaultWidgetColumns(Row, DataStorage);
				return Result;
			}
		}
	}

	return nullptr;
}

bool FLabelWidgetConstructor::SetColumns(ITypedElementDataStorageInterface* DataStorage, UE::Editor::DataStorage::RowHandle Row)
{
	DataStorage->AddColumn(Row, FLabelWidgetColumn{ .bShowHashInTooltip = (MatchedColumnTypes.Num() == 2) });
	return true;
}

bool FLabelWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	UE::Editor::DataStorage::RowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	UE::Editor::DataStorage::UI::Private::SyncColumnsToWidget(
		DataStorage, 
		DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row,
		*DataStorage->GetColumn<FTypedElementU64IntValueCacheColumn>(Row),
		Widget, MatchedColumnTypes.Num() == 2);
	return true;
}

bool FLabelWidgetConstructor::IsWidgetSelected(ITypedElementDataStorageInterface* DataStorage, UE::Editor::DataStorage::RowHandle UiRow)
{
	if (const FExternalWidgetSelectionColumn* ExternalWidgetSelectionColumn = DataStorage->GetColumn<FExternalWidgetSelectionColumn>(UiRow))
	{
		if (ExternalWidgetSelectionColumn->IsSelected.IsBound())
		{
			return ExternalWidgetSelectionColumn->IsSelected.Execute();
		}
	}
	return false;
}
#undef LOCTEXT_NAMESPACE
