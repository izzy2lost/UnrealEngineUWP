// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"

#include "DetailLayoutBuilder.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"


void FChaosVDDetailsCustomizationUtils::HideAllCategories(IDetailLayoutBuilder& DetailBuilder, const TSet<FName>& AllowedCategories)
{
	// Hide everything as the only thing we want to show in these actors is the Recorded debug data
	TArray<FName> CurrentCategoryNames;
	DetailBuilder.GetCategoryNames(CurrentCategoryNames);
	for (const FName& CategoryToHide : CurrentCategoryNames)
	{
		if (!AllowedCategories.Contains(CategoryToHide))
		{
			DetailBuilder.HideCategory(CategoryToHide);
		}
	}
}

void FChaosVDDetailsCustomizationUtils::HideInvalidParticleDataProperties(TConstArrayView<TSharedPtr<IPropertyHandle>> InPropertyHandles)
{
	if (InPropertyHandles.Num() == 0)
	{
		return;
	}

	for (const TSharedPtr<IPropertyHandle>& Handle : InPropertyHandles)
	{
		if (FProperty* Property = Handle ? Handle->GetProperty() : nullptr)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty && StructProperty->Struct && StructProperty->Struct->IsChildOf(FChaosVDParticleDataBase::StaticStruct()))
			{
				void* Data = nullptr;
				Handle->GetValueData(Data);
				if (Data)
				{
					const FChaosVDParticleDataBase* DataViewer = static_cast<const FChaosVDParticleDataBase*>(Data);

					// The Particle Data viewer struct has several fields that will have default values if there was no recorded data for them in the trace file
					// As these do not represent any real value, we should hide them in the details panel
					if (!DataViewer->HasValidData())
					{
						Handle->MarkHiddenByCustomization();
					}
				}
			}
		}
	}
}
