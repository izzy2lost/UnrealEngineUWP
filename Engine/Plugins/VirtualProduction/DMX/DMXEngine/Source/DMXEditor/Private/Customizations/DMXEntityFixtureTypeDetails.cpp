// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXEntityFixtureTypeDetails.h"

#include "DetailLayoutBuilder.h"
#include "DMXEditorLog.h"
#include "Factories/DMXGDTFToFixtureTypeConverter.h"
#include "IPropertyUtilities.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXImportGDTF.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "DMXEntityFixtureTypeDetails"

TSharedRef<IDetailCustomization> FDMXEntityFixtureTypeDetails::MakeInstance()
{
	return MakeShared<FDMXEntityFixtureTypeDetails>();
}

void FDMXEntityFixtureTypeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXEntityFixtureType, Modes));

	GDTFSourceHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXEntityFixtureType, GDTFSource));
	GDTFSourceHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXEntityFixtureTypeDetails::OnGDTFSourceChanged));
}

void FDMXEntityFixtureTypeDetails::OnGDTFSourceChanged()
{
	using namespace UE::DMX::GDTF;

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtilities->GetSelectedObjects();

	for (TWeakObjectPtr<UObject> WeakFixtureTypeObject : SelectedObjects)
	{
		if (UDMXEntityFixtureType* FixtureType = Cast<UDMXEntityFixtureType>(WeakFixtureTypeObject.Get()))
		{
			FixtureType->PreEditChange(nullptr);
			FixtureType->Modes.Reset();
			FixtureType->PostEditChange();

			if (FixtureType->GDTFSource.IsNull())
			{
				continue;
			}
			UDMXImportGDTF* GDTF = FixtureType->GDTFSource.LoadSynchronous();

			FixtureType->PreEditChange(nullptr);
			constexpr bool bUpdateFixtureTypeName = true;
			FDMXGDTFToFixtureTypeConverter::ConvertGDTF(*FixtureType, *GDTF, bUpdateFixtureTypeName);
			FixtureType->PostEditChange();
		}
	}
}

#undef LOCTEXT_NAMESPACE
