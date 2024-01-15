// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableInstanceDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableInstancePrivateData.h"
#include "MuCOE/SCustomizableInstanceProperties.h"

class UObject;

#define LOCTEXT_NAMESPACE "CustomizableInstanceDetails"


TSharedRef<IDetailCustomization> FCustomizableInstanceDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableInstanceDetails);
}


void FCustomizableInstanceDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	const IDetailsView* DetailsView = DetailBuilder->GetDetailsView();
	check(DetailsView->GetSelectedObjects().Num());

	UCustomizableObjectInstance* CustomInstance = Cast<UCustomizableObjectInstance>(DetailsView->GetSelectedObjects()[0].Get());
	check(CustomInstance);
	
	LayoutBuilder = DetailBuilder;

	IDetailCategoryBuilder& ResourcesCategory = DetailBuilder->EditCategory("Generated Resources");
	
	TArray<UObject*> Private;
	Private.Add(CustomInstance->GetPrivate(	));
	
	FAddPropertyParams PrivatePropertyParams;
	PrivatePropertyParams.HideRootObjectNode(true);
	
	IDetailPropertyRow* PrivateDataRow = ResourcesCategory.AddExternalObjects(Private, EPropertyLocation::Default, PrivatePropertyParams);

	IDetailCategoryBuilder& MainCategory = DetailBuilder->EditCategory( "InstanceParameters" );

	MainCategory.AddCustomRow( LOCTEXT("CustomizableInstanceDetails", "Instance Parameters") )
	[
		SNew(SCustomizableInstanceProperties)
			.CustomInstance(CustomInstance)
			.InstanceDetails(SharedThis(this))
	];

	DetailBuilder->EditCategory("TextureParameter");
}


void FCustomizableInstanceDetails::Refresh() const
{
	if (IDetailLayoutBuilder* Layout = LayoutBuilder.Pin().Get()) // Raw because we don't want to keep alive the details builder when calling the force refresh details
	{
		Layout->ForceRefreshDetails();
	}
}


#undef LOCTEXT_NAMESPACE
