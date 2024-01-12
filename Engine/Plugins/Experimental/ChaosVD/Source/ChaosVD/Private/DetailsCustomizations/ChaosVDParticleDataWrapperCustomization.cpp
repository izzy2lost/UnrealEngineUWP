// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsCustomizations/ChaosVDParticleDataWrapperCustomization.h"

#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"


TSharedRef<IPropertyTypeCustomization> FChaosVDParticleDataWrapperCustomization::MakeInstance()
{
	return MakeShareable(new FChaosVDParticleDataWrapperCustomization());
}

void FChaosVDParticleDataWrapperCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	if (NumChildren == 0)
	{
		return;
	}

	TArray<TSharedPtr<IPropertyHandle>> Handles;
	Handles.Reserve(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		Handles.Add(StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
	}

	FChaosVDDetailsCustomizationUtils::HideInvalidParticleDataProperties(Handles);
}
