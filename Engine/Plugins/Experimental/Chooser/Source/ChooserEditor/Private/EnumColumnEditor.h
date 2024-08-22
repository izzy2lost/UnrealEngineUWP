// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEnumCombo.h"
#include "IChooserParameterEnum.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EnumColumnEditor"

namespace UE::ChooserEditor
{
	
	// Wrapper widget for EnumComboBox which will reconstruct the combo box when the Enum has changed
	template <typename ColumnType>
	class SEnumCell : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_OneParam(FOnValueSet, int);
		
		SLATE_BEGIN_ARGS(SEnumCell)
		{}

		SLATE_ARGUMENT(UObject*, TransactionObject)
		SLATE_ARGUMENT(ColumnType*, EnumColumn)
		SLATE_ATTRIBUTE(int32, EnumValue);
		SLATE_EVENT(FOnValueSet, OnValueSet)
				
		SLATE_END_ARGS()

		TSharedRef<SWidget> CreateEnumComboBox()
		{
			if (const ColumnType* EnumColumnPointer = EnumColumn)
			{
				if (EnumColumnPointer->InputValue.IsValid())
				{
					if (const UEnum* Enum = EnumColumnPointer->InputValue.template Get<FChooserParameterEnumBase>().GetEnum())
					{
						return SNew(SEnumComboBox, Enum)
							.IsEnabled_Lambda([this](){ return IsEnabled(); } )
							.CurrentValue(EnumValue)
							.OnEnumSelectionChanged_Lambda([this](int32 InEnumValue, ESelectInfo::Type)
							{
								const FScopedTransaction Transaction(LOCTEXT("Edit RHS", "Edit Enum Value"));
								TransactionObject->Modify(true);
								OnValueSet.ExecuteIfBound(InEnumValue);
							});
					}
				}
			}
			
			return SNullWidget::NullWidget;
		}

		void UpdateEnumComboBox()
		{
			ChildSlot[ CreateEnumComboBox()	];
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			const UEnum* CurrentEnumSource = nullptr;
			if (EnumColumn->InputValue.IsValid())
			{
				CurrentEnumSource = EnumColumn->InputValue.template Get<FChooserParameterEnumBase>().GetEnum(); 
			}
			if (EnumSource != CurrentEnumSource)
			{
				EnumComboBorder->SetContent(CreateEnumComboBox());
				EnumSource = CurrentEnumSource;
			}
		}
							
		void Construct( const FArguments& InArgs)
		{
			SetEnabled(InArgs._IsEnabled);
			
			SetCanTick(true);
			EnumColumn = InArgs._EnumColumn;
			TransactionObject = InArgs._TransactionObject;
			EnumValue = InArgs._EnumValue;
			OnValueSet = InArgs._OnValueSet;

			if (EnumColumn)
			{
				if (EnumColumn->InputValue.IsValid())
				{
					EnumSource = EnumColumn->InputValue.template Get<FChooserParameterEnumBase>().GetEnum();
				}
			}

			UpdateEnumComboBox();

			int Row = RowIndex.Get();

			ChildSlot
			[
				SAssignNew(EnumComboBorder, SBorder).Padding(0).BorderBackgroundColor(FLinearColor(0,0,0,0))
				[
					CreateEnumComboBox()
				]
			];
			
		}

		~SEnumCell()
		{
		}

	private:
		UObject* TransactionObject = nullptr;
		ColumnType* EnumColumn = nullptr;
		const UEnum* EnumSource = nullptr;
		TSharedPtr<SBorder> EnumComboBorder;
		TAttribute<int> RowIndex;
		FDelegateHandle EnumChangedHandle;
		
		FOnValueSet OnValueSet;
		TAttribute<int32> EnumValue;
	};

	
	void RegisterEnumWidgets();
}

#undef LOCTEXT_NAMESPACE