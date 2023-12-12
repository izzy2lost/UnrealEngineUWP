// Copyright Epic Games, Inc. All Rights Reserved.

#include "SChooserTableRow.h"
#include "Chooser.h"
#include "ChooserEditorStyle.h"
#include "ChooserTableEditor.h"
#include "IObjectChooser.h"
#include "ObjectChooserWidgetFactories.h"
#include "SChooserRowHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SSeparator.h"



#define LOCTEXT_NAMESPACE "ChooserTableRow"

namespace UE::ChooserEditor
{
	void SChooserTableRow::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowIndex = Args._Entry;
		Chooser = Args._Chooser;
		Editor = Args._Editor;

		SMultiColumnTableRow<TSharedPtr<FChooserTableRow>>::Construct(
			FSuperRowType::FArguments(),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	TSharedRef<SWidget> SChooserTableRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		static FName Result = "Result";
		static FName Handles = "Handles";
	
		if (Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
		{
			if (ColumnName == Handles)
			{
				// row drag handle
				return SNew(SChooserRowHandle).ChooserEditor(Editor).RowIndex(RowIndex->RowIndex);
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex->RowIndex].GetMutableMemory(), Chooser->ResultsStructs[RowIndex->RowIndex].GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this, RowIndex=RowIndex->RowIndex](const UScriptStruct* ChosenStruct)
				{
					UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->ResultsStructs[RowIndex].InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex].GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder);
				}),
				&CacheBorder
				);
			
				return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ResultWidget.ToSharedRef()
						]
						+ SOverlay::Slot().VAlign(VAlign_Bottom)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive && !bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						]
						+ SOverlay::Slot().VAlign(VAlign_Top)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive && bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						];
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), RowIndex->RowIndex);
				
					if (ColumnWidget.IsValid())
					{
						return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ColumnWidget.ToSharedRef()
						]
						+ SOverlay::Slot()
						[
							SNew(SColorBlock).Visibility(EVisibility::HitTestInvisible).Color_Lambda(
									[this,Column]()
									{
										if (Chooser->bDebugTestValuesValid && Column->HasFilters())
										{
											if (Column->EditorTestFilter(RowIndex->RowIndex))
											{
												return FLinearColor(0.0,1.0,0.0,0.30);
											}
											else
											{
												return FLinearColor(1.0,0.0,0.0,0.20);
											}
										}
										return FLinearColor::Transparent;
									})
						];
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_Fallback)
		{
			if (ColumnName == Handles)
			{
				return SNew(SBox).Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
					[
						SNew(SImage)
						.Image(FChooserEditorStyle::Get().GetBrush("ChooserEditor.FallbackIcon"))
						.ToolTipText(LOCTEXT("FallbackTooltip","Fallback result:  Returned if all rows failed."))
					];
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false,ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), Chooser->FallbackResult.GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
				{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->FallbackResult.InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder
						,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)"));
				}),
				&CacheBorder
				,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)")
				);
				
				return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ResultWidget.ToSharedRef()
						]
						+ SOverlay::Slot().VAlign(VAlign_Top)
						[
							SNew(SSeparator).SeparatorImage(FCoreStyle::Get().GetBrush("FocusRectangle"))
							.Visibility_Lambda([this]() { return bDragActive  ? EVisibility::Visible : EVisibility::Hidden; })
						];
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), -2);
				
					if (ColumnWidget.IsValid())
					{
						return ColumnWidget.ToSharedRef();
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_AddRow)
		{
			// on the row past the end, show an Add button in the result column
			if (ColumnName == Result)
			{
				return Editor->GetCreateRowComboButton().ToSharedRef();
			}
		}

		return SNullWidget::NullWidget;
	}

	void SChooserTableRow::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			bDragActive = true;
			float Center = MyGeometry.Position.Y + MyGeometry.Size.Y;
			bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
		}
	}
	void SChooserTableRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
	{
		bDragActive = false;
	}

	FReply SChooserTableRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			float Center = MyGeometry.AbsolutePosition.Y + MyGeometry.Size.Y/2;
			bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	FReply SChooserTableRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			if (Chooser == Operation->ChooserEditor->GetChooser())
			{
				int NewRowIndex;
				if (!Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
				{
					// for special (negative) indices, move to the end
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, Chooser->ResultsStructs.Num());
				}
				else if (bDropAbove)
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex);
				}
				else
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex+1);
				}
				Editor->SelectRow(NewRowIndex);
				
				return FReply::Handled();		
			}
		}
		return FReply::Unhandled();
	}

}

#undef LOCTEXT_NAMESPACE
