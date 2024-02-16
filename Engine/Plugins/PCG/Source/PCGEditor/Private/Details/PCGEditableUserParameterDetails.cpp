// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/PCGEditableUserParameterDetails.h"

#include "PCGCommon.h"
#include "PCGGraph.h"
#include "Elements/PCGUserParameterGet.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UObject/StructOnScope.h"

#define LOCTEXT_NAMESPACE "PCGEditableUserParameterDetails"

namespace PCGEditableUserParameterDetailsConstants
{
	FName UserParametersCategory = TEXT("Instance");
}

TSharedRef<IDetailCustomization> FPCGEditableUserParameterDetails::MakeInstance()
{
	return MakeShareable(new FPCGEditableUserParameterDetails);
}

void FPCGEditableUserParameterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedPtr<FStructOnScope>> CustomizedStructs;
	DetailBuilder.GetStructsBeingCustomized(CustomizedStructs);

	TArray<TWeakObjectPtr<UObject>> SelectedObjects = DetailBuilder.GetSelectedObjects();
	if (!ensure(!SelectedObjects.IsEmpty()))
	{
		return;
	}

	TWeakObjectPtr<UObject> SettingsObject = SelectedObjects[0];
	if (!ensure(SettingsObject.IsValid()))
	{
		return;
	}

	if (UPCGUserParameterGetSettings* Settings = Cast<UPCGUserParameterGetSettings>(SettingsObject.Get()))
	{
		if (UObject* NodeObject = Settings->GetOuter())
		{
			if (UPCGGraphInterface* GraphInterface = Cast<UPCGGraphInterface>(NodeObject->GetOuter()))
			{
				// It is safe, because we hook pre/post edit changes that will trigger the callbacks
				if (FInstancedPropertyBag* UserParameters = GraphInterface->GetMutableUserParametersStruct_Unsafe())
				{
					IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(PCGEditableUserParameterDetailsConstants::UserParametersCategory);
					IDetailPropertyRow* DetailPropertyRow = CategoryBuilder.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(*UserParameters), Settings->PropertyName);
					TSharedPtr<IPropertyHandle> RowPropertyHandle = DetailPropertyRow->GetPropertyHandle();

					if (RowPropertyHandle.IsValid())
					{
						RowPropertyHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateLambda([this, GraphInterface = MakeWeakObjectPtr(GraphInterface), PropertyName = Settings->PropertyName]()
						{
							check(GEditor);
							if (GEditor->CanTransact())
							{
								GEditor->BeginTransaction(LOCTEXT("EditGraphParameter", "Edit Graph Parameter"));
							}

							if (GraphInterface.IsValid())
							{
								GraphInterface->Modify();
							}
						}));

						RowPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, GraphInterface = MakeWeakObjectPtr(GraphInterface), PropertyName = Settings->PropertyName]()
						{
							if (GraphInterface.IsValid())
							{
								GraphInterface->OnGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, PropertyName);
							}

							check(GEditor);
							if (GEditor->IsTransactionActive())
							{
								GEditor->EndTransaction();
							}
						}));
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE