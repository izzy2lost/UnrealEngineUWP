// Copyright Epic Games, Inc. All Rights Reserved.

#include "Memento/TypedElementMementoTranslators.h"

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "PropertyBag.h"


const UScriptStruct* UTypedElementDefaultMementoTranslator::GetMementoType() const
{
	return MementoType;
}

void UTypedElementDefaultMementoTranslator::PostInitProperties()
{
	Super::PostInitProperties();
	
	const UScriptStruct* SourceColumnType = GetColumnType();
	if (SourceColumnType == nullptr)
	{
		return;
	}

	// Create a new runtime generated struct as the memento from the ColumnType based on the exposed UPROPERTIES
	// and generate a mapping between the properties of the ColumnType to the Memento
	// This mapping will be used to populate the memento and columns during translation
	
	TArray<FPropertyBagPropertyDesc> PropertyDescs;
	for (FProperty* Property = SourceColumnType->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		PropertyDescs.Add(FPropertyBagPropertyDesc(Property->GetFName(), Property));
	}

	// Note: The name of the memento will be an unhelpful "PropertyBag_<hash>"
	// May wish to revisit property bag interface to allow name to be specified for debug purposes
	const UPropertyBag* PropertyBag = UPropertyBag::GetOrCreateFromDescs(PropertyDescs);
	MementoType = PropertyBag;

	// Need to change the type to a FTypedElementDataStorageColumn to appease TEDS/Mass
	const_cast<UPropertyBag*>(PropertyBag)->SetSuperStruct(FTypedElementDataStorageColumn::StaticStruct());

	// Create the property mapping
	for (FProperty* SourceProperty = SourceColumnType->PropertyLink; SourceProperty; SourceProperty = SourceProperty->PropertyLinkNext)
	{
		const FProperty* DestinationProperty = PropertyBag->FindPropertyByName(SourceProperty->GetFName());
		if (DestinationProperty && DestinationProperty->SameType(SourceProperty))
		{
			SourceProperties.Add(SourceProperty);
			DestinationProperties.Add(DestinationProperty);
		}
	}
}

void UTypedElementDefaultMementoTranslator::TranslateColumnToMemento(const void* Column, void* Memento) const
{
	const UScriptStruct* ColumnType = GetColumnType();

	const std::byte* BaseAddressColumn = static_cast<const std::byte*>(Column);
	std::byte* BaseAddressMemento = static_cast<std::byte*>(Memento);

	check(SourceProperties.Num() == DestinationProperties.Num());

	for (int32 PropertyIndex = 0, PropertyIndexEnd = SourceProperties.Num(); PropertyIndex < PropertyIndexEnd; ++PropertyIndex)
	{
		const FProperty* SourceProperty = SourceProperties[PropertyIndex];
		const FProperty* DestinationProperty = DestinationProperties[PropertyIndex];
		void* DestinationValueAddress = BaseAddressMemento + DestinationProperty->GetOffset_ForInternal();
		const void* SourceValueAddress = BaseAddressColumn + SourceProperty->GetOffset_ForInternal();
		SourceProperty->CopyCompleteValue(
			DestinationValueAddress,
			SourceValueAddress);
	}	
}
