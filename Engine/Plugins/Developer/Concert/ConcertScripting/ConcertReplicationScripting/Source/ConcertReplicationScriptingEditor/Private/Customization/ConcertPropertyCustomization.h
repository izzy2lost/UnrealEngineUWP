// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Widgets/SConcertPropertyChainCombo.h"

struct FConcertPropertyChain;
class IDetailChildrenBuilder;

namespace UE::ConcertReplicationScriptingEditor
{
	/** Shows a combo button that allows selecting a property from a class. */
	class FConcertPropertyCustomization : public IPropertyTypeCustomization
	{
	public:
	
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IPropertyTypeCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
		//~ End IPropertyTypeCustomization Interface

	private:

		/** Handle to the property containing the FConcertPropertyChainWrapper struct. */
		TSharedPtr<IPropertyHandle> PropertyHandle;

		/** The UI displayed for the property. */
		TSharedPtr<SConcertPropertyChainCombo> PropertyComboBox;

		/** Dummy passed in to the SConcertPropertyChainCombo. Holds all the values from the different property sources. */
		TSet<FConcertPropertyChain> Properties;
		bool bHasMultipleValues = false;

		void RefreshProperties();
		void RefreshPropertiesAndUpdateUI();
		
		void OnPropertySelectionChanged(const FConcertPropertyChain& Property, bool bIsSelected);
	};
}


