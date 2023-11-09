// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelSectionCustomization.h"

#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "MLDeformerEditorModule.h"
#include "MLDeformerEditorStyle.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborEditorModel.h"
#include "Rendering/SkeletalMeshLODImporterData.h" 
#include "Rendering/SkeletalMeshModel.h"
#include "SMLDeformerBonePickerDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Text/STextBlock.h"

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

		TArray<float> GetVertexAttributeValues(const TArray<int32>& VertexMap, const TArray<float>& VertexWeights, int32 NumVertices)
		{
			check(VertexMap.Num() == VertexWeights.Num());
			check(VertexMap.Num() <= NumVertices);
			TArray<float> VertexAttribute;
			VertexAttribute.SetNumZeroed(NumVertices);
			for (int32 VertexIndex = 0; VertexIndex < VertexMap.Num(); ++VertexIndex)
			{
				VertexAttribute[VertexMap[VertexIndex]] = VertexWeights[VertexIndex];
			}
			return VertexAttribute;
		}

		void CreateVertexAttributes(USkeletalMesh& SkeletalMesh, const FString& AttributeName, const TArray<int32>& VertexMap, const TArray<float>& VertexWeights)
		{
			FSkeletalMeshImportData ImportData; 
			constexpr int32 LODIndex = 0;
			SkeletalMesh.LoadLODImportedData(LODIndex, ImportData);
			using SkeletalMeshImportData::FVertexAttribute;
			constexpr int32 NumComponents = 1;
			FVertexAttribute VertexAttribute(GetVertexAttributeValues(VertexMap, VertexWeights, SkeletalMesh.GetNumImportedVertices()), NumComponents);
			ImportData.VertexAttributeNames.Add(AttributeName);
			ImportData.VertexAttributes.Add(VertexAttribute);
			SkeletalMesh.SaveLODImportedData(LODIndex, ImportData);
		}


		class SNewAttributesDialog
			: public SCustomDialog
		{
		public:
			SLATE_BEGIN_ARGS(SNewAttributesDialog) {}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs);
			const FString& GetAttributesName() const { return AttributesName; }
		private:
			FString AttributesName;
		};

		void SNewAttributesDialog::Construct(const FArguments& InArgs)
		{
			FText DialogTitle = LOCTEXT("EnterAttributesNameDialogTitle", "New Vertex Attributes");
			SCustomDialog::Construct
			(
				SCustomDialog::FArguments()
				.AutoCloseOnButtonPress(true)
				.Title(DialogTitle)
				.UseScrollBox(false)
				.Buttons(
				{
					SCustomDialog::FButton(LOCTEXT("OKText", "OK"))
					.SetPrimary(true)
					.SetFocus(),
					SCustomDialog::FButton(LOCTEXT("CancelText", "Cancel"))
				})
				.Content()
				[
					SNew(SBox)
					.Padding(FMargin(10.0f, 10.0f))
					.MinDesiredWidth(400.0f)
					.HAlign(EHorizontalAlignment::HAlign_Fill)
					.VAlign(EVerticalAlignment::VAlign_Fill)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(EVerticalAlignment::VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AttributesName", "Attributes Name:"))
						]
						+ SHorizontalBox::Slot()
						.VAlign(EVerticalAlignment::VAlign_Center)
						[
							SNew(SBox)
							.MinDesiredWidth(200.0f)
							.Padding(FMargin(4.0f, 0.0f))
							[
								SNew(SEditableTextBox)
								.OnTextChanged_Lambda([&AttributesName = AttributesName](const FText& InText)
								{
									AttributesName = InText.ToString();
								})
							]
						]
					]
				]
			);
		}

		void CreateWeightMapWidgetSelectedBones(FDetailWidgetRow& Row, IDetailLayoutBuilder& LayoutBuilder, UNearestNeighborModelSection& Section, const FNearestNeighborEditorModel& EditorModel)
		{
			Row.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectedBones", "Selected Bones"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(4, 2)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([&Section]()
					{
						return FText::FromString(Section.GetBoneNamesString());
					})
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ToolTipText_Lambda([&Section]()
					{
						return FText::FromString(Section.GetBoneNamesString());
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SelectBones", "Select Bones"))
						.OnClicked_Lambda([&Section, &EditorModel]()
						{
							UNearestNeighborModel* const Model = Cast<UNearestNeighborModel>(EditorModel.GetModel());
							if (!Model)
							{
								return FReply::Handled();
							}
							USkeletalMesh* const SkelMesh = Model->GetSkeletalMesh();
							if (!SkelMesh)
							{
								return FReply::Handled();
							}
							const FLinearColor HighlightColor = UE::MLDeformer::FMLDeformerEditorStyle::Get().GetColor("MLDeformer.InputsWidget.HighlightColor");
							TSharedPtr<UE::MLDeformer::SMLDeformerBonePickerDialog> Dialog = 
								SNew(UE::MLDeformer::SMLDeformerBonePickerDialog)
								.RefSkeleton(&SkelMesh->GetRefSkeleton())
								.AllowMultiSelect(true)
								.HighlightBoneNamesColor(FSlateColor(HighlightColor))
								.HighlightBoneNames(Section.GetBoneNames());
							
							Dialog->ShowModal();
							const TArray<FName>& BoneNames = Dialog->GetPickedBoneNames();
							if (!BoneNames.IsEmpty())
							{
								Section.SetBoneNames(BoneNames);
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SButton)
						.Text(LOCTEXT("CreateAttributes", "Create Attributes"))
						.OnClicked_Lambda([&Section, &EditorModel]()
						{
							TSharedPtr<SNewAttributesDialog> Dialog = SNew(SNewAttributesDialog);
							Dialog->ShowModal();
							const FString& AttributesName = Dialog->GetAttributesName();
							UE_LOG(LogNearestNeighborModel, Log, TEXT("Create Attributes: %s"), *AttributesName);

							UNearestNeighborModel* const Model = Cast<UNearestNeighborModel>(EditorModel.GetModel());
							if (!Model || !Model->GetSkeletalMesh())
							{
								return FReply::Handled();
							}

							CreateVertexAttributes(*Model->GetSkeletalMesh(), AttributesName, Section.GetVertexMap(), Section.GetVertexWeights());
							return FReply::Handled();
						})
					]
				]
			];
		}

		void CreateWeightMapWidgetFromText(FDetailWidgetRow& Row, IDetailLayoutBuilder& LayoutBuilder,UNearestNeighborModelSection& Section, const FNearestNeighborEditorModel& EditorModel)
		{
			TSharedPtr<IPropertyHandle> VertexMapHandle = LayoutBuilder.GetProperty(UNearestNeighborModelSection::GetVertexMapStringPropertyName());
			if (VertexMapHandle.IsValid())
			{
				Row.NameContent()
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
						.OptionsSource(EditorModel.GetVertexMapSelector()->GetOptions())
						.OnSelectionChanged_Lambda([&Section, &EditorModel](TSharedPtr<FString> InString, ESelectInfo::Type InSelectInfo)
							{
								EditorModel.GetVertexMapSelector()->OnSelectionChanged(Section, InString, InSelectInfo);
							})
						.InitiallySelectedItem(EditorModel.GetVertexMapSelector()->GetSelectedItem(Section))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([&Section]()
						{
							return Section.GetMeshIndex() == INDEX_NONE;
						})))
						[
							VertexMapHandle->CreatePropertyValueWidget()
						]
					]
				];
				LayoutBuilder.HideProperty(VertexMapHandle);
			}
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

		SectionBuilder.AddProperty(UNearestNeighborModelSection::GetWeightMapCreationMethodPropertyName());

		FDetailWidgetRow& WeightMapFromTextRow = SectionBuilder.AddCustomRow(LOCTEXT("WeightMapFromText", "WeightMapFromText"))
			.OverrideResetToDefault(FResetToDefaultOverride::Hide())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([Section]()
			{
				return Section->GetWeightMapCreationMethod() == 
				ENearestNeighborModelSectionWeightMapCreationMethod::FromText 
				? EVisibility::Visible : EVisibility::Collapsed;
			})));
		Private::CreateWeightMapWidgetFromText(WeightMapFromTextRow, DetailBuilder, *Section, *EditorModel);

		FDetailWidgetRow& WeightMapSelectedBonesRow = SectionBuilder.AddCustomRow(LOCTEXT("WeightMapSelectedBones", "WeightMapSelectedBones"))
			.OverrideResetToDefault(FResetToDefaultOverride::Hide())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([Section]()
			{
				return Section->GetWeightMapCreationMethod() == 
				ENearestNeighborModelSectionWeightMapCreationMethod::SelectedBones 
				? EVisibility::Visible : EVisibility::Collapsed;
			})));
		Private::CreateWeightMapWidgetSelectedBones(WeightMapSelectedBonesRow, DetailBuilder, *Section, *EditorModel);

		SectionBuilder.AddProperty(UNearestNeighborModelSection::GetAttributeNamePropertyName())
			.OverrideResetToDefault(FResetToDefaultOverride::Hide())
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([Section]()
			{
				return Section->GetWeightMapCreationMethod() == 
				ENearestNeighborModelSectionWeightMapCreationMethod::VertexAttributes 
				? EVisibility::Visible : EVisibility::Collapsed;
			})));

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