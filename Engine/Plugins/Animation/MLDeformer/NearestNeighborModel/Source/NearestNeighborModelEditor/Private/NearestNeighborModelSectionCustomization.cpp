// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelSectionCustomization.h"

#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "MLDeformerEditorModule.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborEditorModel.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "NearestNeighborModelSectionCustomization"

namespace UE::NearestNeighborModel
{
	namespace Private
	{
		FNearestNeighborEditorModel* GetEditorModel(const UNearestNeighborModel* Model)
		{
			if (!Model)
			{
				return nullptr;
			}
			using ::UE::MLDeformer::FMLDeformerEditorModule;
			FMLDeformerEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FMLDeformerEditorModule>("MLDeformerFrameworkEditor");
			return static_cast<FNearestNeighborEditorModel*>(EditorModule.GetModelRegistry().GetEditorModel(const_cast<UNearestNeighborModel*>(Model)));
		}
	};

	void SNearestNeighborModelSectionWidget::Construct(const FArguments& InArgs)
	{
		Section = InArgs._Section;
	
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
		FDetailsViewArgs Args;
		Args.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
		Args.bAllowSearch = false;
		Args.bShowObjectLabel = false;
		Args.bShowScrollBar = false;
		DetailsView = PropertyModule.CreateDetailView(Args);
		DetailsView->SetObject(Section);
	
		this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailsView.ToSharedRef()
			]
		];
	}

	void FNearestNeighborModelSectionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
	{
		DetailBuilder.HideCategory("Section Private");
		IDetailCategoryBuilder& SectionBuilder = DetailBuilder.EditCategory("Section", LOCTEXT("SectionCategory", "Section"));
		SectionBuilder.AddProperty(UNearestNeighborModelSection::GetNumPCACoeffsPropertyName());

		// Get the selected objects
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetSelectedObjects();
		if (SelectedObjects.IsEmpty())
		{
			return;
		}
		TWeakObjectPtr<UObject> SelectedObject = SelectedObjects[0];
		if (!SelectedObject.IsValid())
		{
			return;
		}
		UNearestNeighborModelSection* Section = Cast<UNearestNeighborModelSection>(SelectedObject.Get());
		if (!Section)
		{
			return;
		}

		const FNearestNeighborEditorModel* EditorModel = Private::GetEditorModel(Section->GetModel());
		if (!EditorModel)
		{
			return;
		}

		TSharedPtr<IPropertyHandle> VertexMapHandle = DetailBuilder.GetProperty(UNearestNeighborModelSection::GetVertexMapStringPropertyName());
		if (VertexMapHandle.IsValid())
		{
			SectionBuilder.AddProperty(VertexMapHandle).CustomWidget()
				.OverrideResetToDefault(FResetToDefaultOverride::Hide())
				.NameContent()
				[
					VertexMapHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(2, 2)
					.AutoHeight()
					[
						SNew(STextComboBox)
						.OptionsSource(EditorModel->GetVertexMapSelector()->GetOptions())
						.OnSelectionChanged_Lambda([Section, EditorModel](TSharedPtr<FString> InString, ESelectInfo::Type InSelectInfo)
							{
								EditorModel->GetVertexMapSelector()->OnSelectionChanged(*Section, InString, InSelectInfo);
							})
						.InitiallySelectedItem(EditorModel->GetVertexMapSelector()->GetSelectedItem(*Section))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([Section]()
						{
							return Section->GetMeshIndex() == INDEX_NONE;
						})))
						[
							VertexMapHandle->CreatePropertyValueWidget()
						]
					]
				];
		}

		const UNearestNeighborModel* const Model = Section->GetModel();
		if (!Model)
		{
			return;
		}
		const FName PosesName = UNearestNeighborModelSection::GetNeighborPosesPropertyName();
		TSharedPtr<IPropertyHandle> NeighborPosesHandle = DetailBuilder.GetProperty(PosesName);
		SectionBuilder.AddProperty(PosesName)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([Model]()
				{
					return Model->IsReadyForTraining();
				})));
		const FName MeshesName = UNearestNeighborModelSection::GetNeighborMeshesPropertyName();
		TSharedPtr<IPropertyHandle> NeighborMeshesHandle = DetailBuilder.GetProperty(MeshesName);
		SectionBuilder.AddProperty(MeshesName)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([Model]()
				{
					return Model->IsReadyForTraining();
				})));
		const FName FramesName = UNearestNeighborModelSection::GetExcludedFramesPropertyName();
		TSharedPtr<IPropertyHandle> ExcludedFramesHandle = DetailBuilder.GetProperty(FramesName);
		SectionBuilder.AddProperty(FramesName)
			.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([Model]()
				{
					return Model->IsReadyForTraining();
				})));
	}
}	// namespace UE::NearestNeighborModel

#undef LOCTEXT_NAMESPACE