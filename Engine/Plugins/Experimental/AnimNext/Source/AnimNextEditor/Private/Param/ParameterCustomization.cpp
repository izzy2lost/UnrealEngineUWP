// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "PropertyHandle.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_Parameter.h"
#include "Module/AnimNextModule_EditorData.h"

#define LOCTEXT_NAMESPACE "ParamTypePropertyCustomization"

namespace UE::AnimNext::Editor
{

void FParameterCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.IsEmpty() || Objects.Num() > 1)
	{
		return;
	}

	if (UAnimNextModule_Parameter* Parameter = Cast<UAnimNextModule_Parameter>(Objects[0].Get()))
	{
		IDetailCategoryBuilder& ParameterCategory = DetailBuilder.EditCategory(TEXT("Parameter"), FText::GetEmpty(), ECategoryPriority::Important);

		IDetailCategoryBuilder& DefaultValueCategory = DetailBuilder.EditCategory(TEXT("DefaultValue"), FText::GetEmpty(), ECategoryPriority::Default);

		TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

		if (UAnimNextModule_EditorData* EditorData = Cast<UAnimNextModule_EditorData>(Parameter->GetOuter()))
		{
			const FName EntryName = Parameter->GetEntryName();
			if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(EntryName)) 
			{
				if (UAnimNextModule* ReferencedModule = UE::AnimNext::UncookedOnly::FUtils::GetGraph(EditorData))
				{
					FAddPropertyParams AddPropertyParams;
					TArray<IDetailPropertyRow*> DetailPropertyRows;

					if (const FPropertyBagPropertyDesc* PropertyDesc = ReferencedModule->DefaultState.State.FindPropertyDescByName(Parameter->GetParamName()))
					{
						IDetailPropertyRow* DetailPropertyRow = DefaultValueCategory.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(ReferencedModule->DefaultState.State), Parameter->GetParamName(), EPropertyLocation::Default, AddPropertyParams);
						if (TSharedPtr<IPropertyHandle> Handle = DetailPropertyRow->GetPropertyHandle(); Handle.IsValid())
						{
							Handle->SetPropertyDisplayName(FText::FromName(EntryName));
							
							const TWeakObjectPtr<UAnimNextModule> ReferencedModuleWeak = ReferencedModule;

							const auto OnPropertyValuePreChange = [ReferencedModuleWeak]()
								{
									if (ReferencedModuleWeak.IsValid())
									{
										ReferencedModuleWeak->Modify(); // needed to enable the transaction when we modify the PropertyBag
									}
								};
							const auto OnPropertyValueChange = [ReferencedModuleWeak](const FPropertyChangedEvent& InEvent)
								{
									if (ReferencedModuleWeak.IsValid())
									{
										if (UAnimNextModule_EditorData* EditorData = Cast<UAnimNextModule_EditorData>(ReferencedModuleWeak->EditorData))
										{
											if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(UncookedOnly::FUtils::GetParameterNameFromQualifiedName(InEvent.GetPropertyName())))
											{
												AssetEntry->MarkPackageDirty();
												AssetEntry->BroadcastModified();
											}
										}
									}
								};

							Handle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateLambda(OnPropertyValuePreChange));
							Handle->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda(OnPropertyValueChange));

							Handle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda(OnPropertyValuePreChange));
							Handle->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda(OnPropertyValueChange));
						}
					}
				}
			}
		}
	}
}

void FParameterCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) 
{
	CustomizeDetails(*DetailBuilder);
}

FText FParameterCustomization::GetName() const
{
	return FText();
}

void FParameterCustomization::SetName(const FText& InNewText, ETextCommit::Type InCommitType)
{
}

bool FParameterCustomization::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	return true;
}


}

#undef LOCTEXT_NAMESPACE
