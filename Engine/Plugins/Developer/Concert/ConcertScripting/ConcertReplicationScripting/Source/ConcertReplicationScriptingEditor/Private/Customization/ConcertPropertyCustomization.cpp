// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertPropertyCustomization.h"

#include "ConcertPropertyChainWrapper.h"
#include "Widgets/SConcertPropertyChainCombo.h"

#include "DetailWidgetRow.h"

namespace UE::ConcertReplicationScriptingEditor
{
	/** Keeps track of the last class that was used for searching properties in SConcertPropertyChainCombo. */
	static TWeakObjectPtr<const UClass> LastSelectedClass;
	/**
	 * Maps every property path to the last used class. This why when a user works with multiple properties, each one will remember the last used class for convenience.
	 * This leaks memory but a user must edit a lot of properties to really be noticeable... should be ok.
	 */
	static TMap<FString, TWeakObjectPtr<const UClass>> PropertyToLastUsedClass;
	
	TSharedRef<IPropertyTypeCustomization> FConcertPropertyCustomization::MakeInstance()
	{
		return MakeShared<FConcertPropertyCustomization>();
	}

	void FConcertPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		PropertyHandle = StructPropertyHandle;
		PropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FConcertPropertyCustomization::RefreshPropertiesAndUpdateUI));
		RefreshProperties();
		
		const FString PropertyPath(PropertyHandle->GetPropertyPath());
		const TWeakObjectPtr<const UClass>* LastBoundClass = PropertyToLastUsedClass.Find(PropertyPath);
		const TWeakObjectPtr<const UClass> ClassToUse = LastBoundClass ? *LastBoundClass : LastSelectedClass;
		
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SBox)
			.Padding(FMargin(0,2,0,1))
			[
				SAssignNew(PropertyComboBox, SConcertPropertyChainCombo)
				.InitialClassSelection(ClassToUse.IsValid() ? ClassToUse.Get() : nullptr)
				.IsEditable(!PropertyHandle->IsEditConst())
				.ContainedProperties(&Properties)
				.OnClassChanged_Lambda([this](const UClass* Class)
				{
					LastSelectedClass = Class;
					
					const FString PropertyPath(PropertyHandle->GetPropertyPath());
					PropertyToLastUsedClass.Add(PropertyPath, Class);
				})
				.OnPropertySelectionChanged(this, &FConcertPropertyCustomization::OnPropertySelectionChanged)
			]
		];
	}

	void FConcertPropertyCustomization::RefreshProperties()
	{
		TArray<const void*> RawStructData;
		PropertyHandle->AccessRawData(RawStructData);

		Properties.Reset();
		bHasMultipleValues = false;

		TOptional<FConcertPropertyChain> SharedValue;
		for (int32 Idx = 0; Idx < RawStructData.Num(); ++Idx)
		{
			if (!RawStructData[Idx])
			{
				continue;
			}

			const FConcertPropertyChainWrapper& CurrentValue = *(FConcertPropertyChainWrapper*)RawStructData[Idx];
			Properties.Add(CurrentValue.PropertyChain);
			
			if (!SharedValue)
			{
				SharedValue = CurrentValue.PropertyChain;
			}

			const bool bValueIsDifferent = *SharedValue != CurrentValue.PropertyChain;
			bHasMultipleValues |= bValueIsDifferent;
		}
	}

	void FConcertPropertyCustomization::RefreshPropertiesAndUpdateUI()
	{
		RefreshProperties();
		PropertyComboBox->RefreshPropertyContent();
	}

	void FConcertPropertyCustomization::OnPropertySelectionChanged(const FConcertPropertyChain& Property, bool bIsSelected)
	{
		// And this updates the underlying property data
		TArray<void*> RawStructData;
		PropertyHandle->AccessRawData(RawStructData);
		FConcertPropertyChainWrapper NewValue = bIsSelected ? FConcertPropertyChainWrapper{ Property } : FConcertPropertyChainWrapper{};
		for (int32 Idx = 0; Idx < RawStructData.Num(); ++Idx)
		{
			FConcertPropertyChainWrapper& Value = *(FConcertPropertyChainWrapper*)RawStructData[Idx];
			Value = NewValue;
		}
		
		// This makes sure the checkboxes display the correct state ...
		Properties = { NewValue.PropertyChain };
		// ... and this updates the combo button's content. 
		PropertyComboBox->RefreshPropertyContent();
	}
}
