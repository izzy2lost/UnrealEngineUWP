// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "PropertyHandle.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_Parameter.h"
#include "Graph/AnimNextGraph_EditorData.h"

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

	if (UAnimNextGraph_Parameter* Parameter = Cast<UAnimNextGraph_Parameter>(Objects[0].Get()))
	{
		IDetailCategoryBuilder& ParameterCategory = DetailBuilder.EditCategory(TEXT("Parameter"), FText::GetEmpty(), ECategoryPriority::Important);

		IDetailCategoryBuilder& DefaultValueCategory = DetailBuilder.EditCategory(TEXT("DefaultValue"), FText::GetEmpty(), ECategoryPriority::Default);

		TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

		if (UAnimNextGraph_EditorData* EditorData = Cast<UAnimNextGraph_EditorData>(Parameter->GetOuter()))
		{
			const FName ParameterName = Parameter->GetEntryName();
			if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(ParameterName)) 
			{
				if (UAnimNextGraph* ReferencedGraph = UE::AnimNext::UncookedOnly::FUtils::GetGraph(EditorData))
				{
					FAddPropertyParams AddPropertyParams;
					TArray<IDetailPropertyRow*> DetailPropertyRows;

					if (const FPropertyBagPropertyDesc* PropertyDesc = ReferencedGraph->PropertyBag.FindPropertyDescByName(ParameterName))
					{
						IDetailPropertyRow* DetailPropertyRow = DefaultValueCategory.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(ReferencedGraph->PropertyBag), ParameterName, EPropertyLocation::Default, AddPropertyParams);
						if (TSharedPtr<IPropertyHandle> Handle = DetailPropertyRow->GetPropertyHandle(); Handle.IsValid())
						{
							const TWeakObjectPtr<UAnimNextGraph> ReferencedGraphWeak = ReferencedGraph;

							const auto OnPropertyValuePreChange = [ReferencedGraphWeak]()
								{
									if (ReferencedGraphWeak.IsValid())
									{
										ReferencedGraphWeak->Modify(); // needed to enable the transaction when we modify the PropertyBag
									}
								};
							const auto OnPropertyValueChange = [ReferencedGraphWeak](const FPropertyChangedEvent& InEvent)
								{
									if (ReferencedGraphWeak.IsValid())
									{
										if (UAnimNextGraph_EditorData* EditorData = Cast<UAnimNextGraph_EditorData>(ReferencedGraphWeak->EditorData))
										{
											if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(InEvent.GetPropertyName()))
											{
												AssetEntry->MarkPackageDirty();
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
