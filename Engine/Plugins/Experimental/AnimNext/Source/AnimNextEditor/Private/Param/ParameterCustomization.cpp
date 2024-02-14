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

					if (ReferencedGraph->PropertyBag.FindPropertyDescByName(ParameterName))
					{
						IDetailPropertyRow* DetailPropertyRow = DefaultValueCategory.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(ReferencedGraph->PropertyBag), ParameterName, EPropertyLocation::Default, AddPropertyParams);
						if (TSharedPtr<IPropertyHandle> Handle = DetailPropertyRow->GetPropertyHandle(); Handle.IsValid())
						{
							Handle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda([this, ReferencedGraph]()
							{
								ReferencedGraph->Modify(); // needed to enable the transaction when we modify the PropertyBag
							}));
							Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, ReferencedGraph, ParameterName]()
							{
								if (UAnimNextGraph_EditorData* EditorData = Cast<UAnimNextGraph_EditorData>(ReferencedGraph->EditorData))
								{
									if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(ParameterName))
									{
										AssetEntry->MarkPackageDirty();
									}
								}
							}));
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
