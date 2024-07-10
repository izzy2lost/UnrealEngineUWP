// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMComponentEdit.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageInput.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"
#include "CustomDetailsViewSequencer.h"
#include "DetailLayoutBuilder.h"
#include "DMEDefs.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ICustomDetailsView.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailTreeNode.h"
#include "ISinglePropertyView.h"
#include "Items/CustomDetailsViewItemId.h"
#include "Items/ICustomDetailsViewCustomCategoryItem.h"
#include "Items/ICustomDetailsViewCustomItem.h"
#include "Items/ICustomDetailsViewItem.h"
#include "Menus/DMMaterialStageSourceMenus.h"
#include "Misc/CoreDelegates.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Slate/Properties/Editors/SDMPropertyEditBool.h"
#include "Slate/Properties/Editors/SDMPropertyEditColor.h"
#include "Slate/Properties/Editors/SDMPropertyEditEnum.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat.h"
#include "Slate/Properties/Editors/SDMPropertyEditObject.h"
#include "Slate/Properties/Editors/SDMPropertyEditVector.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

static const float RowPadding = 2.0f;

#define LOCTEXT_NAMESPACE "SDMComponentEdit"

ECheckBoxState SDMComponentEdit::IsInputPerChannelMapped(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx)
{
	// TODO Implement per channel mapping
	return ECheckBoxState::Unchecked;
}

FText SDMComponentEdit::GetInputChannelMapDescription(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx, int32 InChannelIdx)
{
	if (!IsValid(InThroughput) || !InThroughput->IsComponentValid())
	{
		return FText::GetEmpty();
	}
	
	UDMMaterialStage* Stage = InThroughput->GetStage();
	
	if (!IsValid(Stage) || !Stage->IsComponentValid())
	{
		return FText::GetEmpty();
	}

	const TArray<FDMMaterialStageConnection>& InputConnectionMap = Stage->GetInputConnectionMap();

	if (!InputConnectionMap.IsValidIndex(InInputIdx))
	{
		return FText::GetEmpty();
	}

	const FDMMaterialStageConnection& Connection = InputConnectionMap[InInputIdx];

	if (!Connection.Channels.IsValidIndex(InChannelIdx))
	{
		return FText::GetEmpty();
	}

	FText SourceName = LOCTEXT("?", "?");
	EDMValueType OutputType = EDMValueType::VT_None;

	const FDMMaterialStageConnectorChannel& Channel = Connection.Channels[InChannelIdx];

	if (Channel.SourceIndex == FDMMaterialStageConnectorChannel::PREVIOUS_STAGE)
	{
		if (Stage)
		{
			UDMMaterialLayerObject* Layer = Stage->GetLayer();

			if (!IsValid(Layer))
			{
				return FText::GetEmpty();
			}

			UDMMaterialSlot* Slot = Layer->GetSlot();

			if (!IsValid(Slot) || !Slot->IsComponentValid())
			{
				return FText::GetEmpty();
			}

			UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

			if (!IsValid(ModelEditorOnlyData))
			{
				return FText::GetEmpty();
			}

			UDMMaterialProperty* PropertyObj = ModelEditorOnlyData->GetMaterialProperty(Channel.MaterialProperty);

			if (!IsValid(PropertyObj) || !PropertyObj->IsComponentValid())
			{
				return FText::GetEmpty();
			}

			SourceName = FText::Format(
				LOCTEXT("PrevStageFormat", "Prev Stage {0}"),
				PropertyObj->GetDescription()
			);

			if (const UDMMaterialLayerObject* PreviousStage = Layer->GetPreviousLayer(Channel.MaterialProperty, EDMMaterialLayerStage::Base))
			{
				if (UDMMaterialStage* PreviousMask = PreviousStage->GetStage(EDMMaterialLayerStage::Mask))
				{
					if (UDMMaterialStageSource* PreviousStageSource = PreviousMask->GetSource())
					{
						const TArray<FDMMaterialStageConnector>& PreviousStageOutputs = PreviousStageSource->GetOutputConnectors();

						if (PreviousStageOutputs.IsValidIndex(Channel.OutputIndex))
						{
							static const FText StageOutputFormat = LOCTEXT("StageOutputFormat", "{0}: {1}");

							SourceName = FText::Format(
								StageOutputFormat,
								SourceName,
								PreviousStageOutputs[Channel.OutputIndex].Name
							);

							OutputType = PreviousStageOutputs[Channel.OutputIndex].Type;
						}
					}
				}
			}
		}
	}
	else
	{
		const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
		const int32 StageInputIdx = Channel.SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

		if (StageInputs.IsValidIndex(StageInputIdx))
		{
			SourceName = StageInputs[StageInputIdx]->GetChannelDescription(Channel);

			const TArray<FDMMaterialStageConnector>& StageInputOutputConnectors = StageInputs[StageInputIdx]->GetOutputConnectors();

			if (StageInputOutputConnectors.IsValidIndex(Channel.OutputIndex))
			{
				if (StageInputOutputConnectors.Num() > 1)
				{
					static const FText InputOutputFormat = LOCTEXT("InputOutputFormat", "{0}: {1}");

					SourceName = FText::Format(
						InputOutputFormat,
						SourceName,
						StageInputOutputConnectors[Channel.OutputIndex].Name
					);
				}

				OutputType = StageInputOutputConnectors[Channel.OutputIndex].Type;
			}
		}
	}

	if (Channel.OutputChannel == FDMMaterialStageConnectorChannel::WHOLE_CHANNEL || OutputType == EDMValueType::VT_None)
	{
		static const FText WholeChannelFormat = LOCTEXT("WholeChannelFormat", "{0}");
		return FText::Format(WholeChannelFormat, SourceName);
	}

	// Assume RGBA.
	TArray<FText> Channels;

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(1));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(2));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(3));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(4));
	}

	static const FText Separator = LOCTEXT("ChannelSeparator", ",");
	static const FText MaskedChannelFormat = LOCTEXT("MaskedChannelFormat", "{0}: {1}");
	const FText ChannelName = FText::Join(Separator, Channels);

	return FText::Format(MaskedChannelFormat, SourceName, ChannelName);
}

SDMComponentEdit::~SDMComponentEdit()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	SDMEditor::ClearPropertyHandles(this);

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		Stage->SetBeingEdited(false);
	}
}

void SDMComponentEdit::Construct(const FArguments& InArgs, UDMMaterialComponent* InComponent, const TWeakPtr<SDMEditor>& InEditorWidget)
{
	ComponentWeak = InComponent;
	EditorWidgetWeak = InEditorWidget;

	TGuardValue<bool> Constructing(bConstructing, true);

	KeyframeHandler = nullptr;

	UObject* WorldContext = InComponent;

	if (!WorldContext)
	{
		if (TSharedPtr<SDMEditor> EditorWidget = InEditorWidget.Pin())
		{
			WorldContext = EditorWidget->GetMaterialModelBase();
		}
	}

	if (WorldContext)
	{
		if (const UWorld* const World = WorldContext->GetWorld())
		{
			if (const UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
			{
				KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
			}
		}
	}

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent))
	{
		Stage->SetBeingEdited(true);
	}

	ChildSlot
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(Container, SBox)
			[
				CreateEditWidget()
			]
		];
}

void SDMComponentEdit::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{
}

TSharedRef<SWidget> SDMComponentEdit::CreateEditWidget()
{
	SDMEditor::ClearPropertyHandles(this);

	constexpr bool bDefaultCategoryExpansionState = true;

	UDMMaterialComponent* Component = ComponentWeak.Get();

	bool bIsDynamic = false;

	if (TSharedPtr<SDMEditor> EditorWidget = GetEditorWidget())
	{
		if (UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase())
		{
			bIsDynamic = !MaterialModelBase->IsA<UDynamicMaterialModel>();
		}
	}

	FCustomDetailsViewArgs Args;
	Args.KeyframeHandler = KeyframeHandler;
	Args.bAllowGlobalExtensions = true;
	Args.bAllowResetToDefault = true;
	Args.bShowCategories = false;
	Args.OnExpansionStateChanged.AddSP(this, &SDMComponentEdit::OnExpansionStateChanged);

	TSharedRef<ICustomDetailsView> DetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(Args);
	FCustomDetailsViewItemId RootId = DetailsView->GetRootItem()->GetItemId();

	TSharedPtr<ICustomDetailsViewItem> DefaultCategoryItem;

	auto GetDefaultCategory = [this, Component, &DefaultCategoryItem, &DetailsView, &RootId]()
		{
			if (!DefaultCategoryItem.IsValid())
			{
				constexpr const TCHAR* DefaultCategoryName = TEXT("General");

				DefaultCategoryItem = DetailsView->CreateCustomCategoryItem(DefaultCategoryName, LOCTEXT("General", "General"))->AsItem();
				DefaultCategoryItem->RefreshItemId();
				DetailsView->ExtendTree(RootId, ECustomDetailsTreeInsertPosition::Child, DefaultCategoryItem.ToSharedRef());

				bool bExpansionState = true;
				SDMEditor::GetExpansionState(Component, DefaultCategoryName, bExpansionState);

				DetailsView->SetItemExpansionState(DefaultCategoryItem->GetItemId(), bExpansionState);
				Categories.Add(DefaultCategoryName);
			}

			return DefaultCategoryItem;
		};

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(Component))
	{
		TSharedPtr<ICustomDetailsViewCustomItem> TypeSelectorItem = DetailsView->CreateCustomItem(
			FName(TEXT("SamplerType")),
			LOCTEXT("SamplerType", "Type"),
			LOCTEXT("SamplerTypeTooltip", "Source Function")
		);

		if (TypeSelectorItem.IsValid())
		{
			TypeSelectorItem->SetValueWidget(CreateSourceTypeEditWidget());

			if (bIsDynamic)
			{
				TypeSelectorItem->AsItem()->SetEnabledOverride(false);
			}

			DetailsView->ExtendTree(
				GetDefaultCategory()->GetItemId(),
				ECustomDetailsTreeInsertPosition::FirstChild,
				TypeSelectorItem->AsItem()
			);
		}
	}

	TArray<FDMPropertyHandle> EditRows = GetEditRows();

	for (FDMPropertyHandle& EditRow : EditRows)
	{
		const bool bHasValidCustomWidget = EditRow.ValueWidget.IsValid() && !EditRow.ValueName.IsNone() && EditRow.NameOverride.IsSet();

		if (!EditRow.DetailTreeNode && !bHasValidCustomWidget)
		{
			continue;
		}

		ECustomDetailsTreeInsertPosition Position = ECustomDetailsTreeInsertPosition::Child;

		if (EditRow.PropertyHandle.IsValid())
		{
			if (EditRow.PropertyHandle->HasMetaData("HighPriority"))
			{
				Position = ECustomDetailsTreeInsertPosition::FirstChild;
			}
			else if (EditRow.PropertyHandle->HasMetaData("LowPriority"))
			{
				Position = ECustomDetailsTreeInsertPosition::LastChild;
			}
		}

		FName CategoryName = EditRow.CategoryOverrideName;

		if (CategoryName.IsNone() && EditRow.PropertyHandle.IsValid())
		{
			// Sub category (possibly)
			if (TSharedPtr<IPropertyHandle> SubCategoryProperty = EditRow.PropertyHandle->GetParentHandle())
			{
				if (SubCategoryProperty->IsCategoryHandle())
				{
					// "Material Designer" (possibly)
					if (TSharedPtr<IPropertyHandle> MaterialDesignerCategoryProperty = SubCategoryProperty->GetParentHandle())
					{
						if (MaterialDesignerCategoryProperty->IsCategoryHandle())
						{
							CategoryName = *SubCategoryProperty->GetPropertyDisplayName().ToString();
						}
					}
				}
			}
		}

		TSharedPtr<ICustomDetailsViewItem> CategoryItem;

		if (CategoryName.IsNone())
		{
			CategoryItem = GetDefaultCategory();
		}
		else
		{
			CategoryItem = DetailsView->FindCustomItem(CategoryName);

			if (!CategoryItem.IsValid())
			{
				CategoryItem = DetailsView->CreateCustomCategoryItem(CategoryName, FText::FromName(CategoryName))->AsItem();
				CategoryItem->RefreshItemId();
				DetailsView->ExtendTree(RootId, ECustomDetailsTreeInsertPosition::Child, CategoryItem.ToSharedRef());

				bool bExpansionState = true;
				SDMEditor::GetExpansionState(Component, CategoryName, bExpansionState);

				DetailsView->SetItemExpansionState(CategoryItem->GetItemId(), bExpansionState);

				Categories.Add(CategoryName);
			}
		}

		if (bHasValidCustomWidget)
		{
			TSharedPtr<ICustomDetailsViewCustomItem> Item = DetailsView->CreateCustomItem(
				EditRow.ValueName,
				EditRow.NameOverride.GetValue(),
				EditRow.NameToolTipOverride.Get(FText::GetEmpty())
			);

			if (!Item.IsValid())
			{
				continue;
			}

			Item->SetValueWidget(EditRow.ValueWidget.ToSharedRef());

			if (!EditRow.bEnabled)
			{
				Item->AsItem()->SetEnabledOverride(false);

				// Disable the expansion widgets (SNullWidget is treated as removing the override).
				Item->SetExpansionWidget(SNew(SBox));
			}

			if (EditRow.MaxWidth.IsSet())
			{
				Item->AsItem()->SetValueWidgetWidthOverride(EditRow.MaxWidth);
			}

			if (CategoryItem.IsValid())
			{
				DetailsView->ExtendTree(CategoryItem->GetItemId(), Position, Item->AsItem());
			}
			else
			{
				DetailsView->ExtendTree(RootId, Position, Item->AsItem());
			}

			continue;
		}

		if (!EditRow.DetailTreeNode)
		{
			continue;
		}

		TSharedRef<ICustomDetailsViewItem> Item = DetailsView->CreateDetailTreeItem(EditRow.DetailTreeNode.ToSharedRef());

		if (EditRow.NameOverride.IsSet())
		{
			Item->SetOverrideWidget(
				ECustomDetailsViewWidgetType::Name,
				SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(EditRow.NameOverride.GetValue())
					.ToolTipText(EditRow.NameToolTipOverride.Get(FText::GetEmpty()))
			);
		}

		if (!EditRow.bEnabled)
		{
			Item->SetEnabledOverride(false);

			// Disable the expansion widgets (SNullWidget is treated as removing the override).
			Item->SetOverrideWidget(ECustomDetailsViewWidgetType::Extensions, SNew(SBox));
		}

		if (EditRow.PropertyHandle.IsValid() && EditRow.PropertyHandle->HasMetaData("NotKeyframeable"))
		{
			Item->SetKeyframeEnabled(false);
		}

		if (EditRow.ResetToDefaultOverride.IsSet())
		{
			Item->SetResetToDefaultOverride(EditRow.ResetToDefaultOverride.GetValue());
		}

		if (EditRow.MaxWidth.IsSet())
		{
			Item->SetValueWidgetWidthOverride(EditRow.MaxWidth);
		}

		if (CategoryItem.IsValid())
		{
			DetailsView->ExtendTree(CategoryItem->GetItemId(), Position, Item);
		}
		else
		{
			DetailsView->ExtendTree(RootId, Position, Item);
		}
	}

	DetailsView->RebuildTree(ECustomDetailsViewBuildType::InstantBuild);

	return DetailsView;
}

TArray<FDMPropertyHandle> SDMComponentEdit::GetEditRows()
{
	TArray<FDMPropertyHandle> PropertyRows;
	TSet<UDMMaterialComponent*> ProcessedObjects;

	if (UDMMaterialComponent* Component = ComponentWeak.Get())
	{
		FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(SharedThis(this), Component, PropertyRows, ProcessedObjects);
	}
	else
	{
		if (TSharedPtr<SDMEditor> EditorWidget = GetEditorWidget())
		{
			if (UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase())
			{
				bool bIsDynamic = !MaterialModelBase->IsA<UDynamicMaterialModel>();

				GenerateMaterialModelPropertyRows(EditorWidget.ToSharedRef(), MaterialModelBase, bIsDynamic, PropertyRows, ProcessedObjects);
			}
		}
	}

	return PropertyRows;
}

TSharedRef<SWidget> SDMComponentEdit::CreateExtensionButtons(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget,
	UDMMaterialComponent* InComponent, const FName& InPropertyName, bool bInAllowKeyframe)
{
	return CreateExtensionButtons(
		InComponentEditWidget, 
		InComponent, 
		InPropertyName, 
		bInAllowKeyframe,
		FSimpleDelegate::CreateLambda([ComponentEditWidgetWeak = InComponentEditWidget.ToWeakPtr()]()
			{
				if (TSharedPtr<SDMComponentEdit> ComponentEditWidget = ComponentEditWidgetWeak.Pin())
				{
					if (TSharedPtr<SDMEditor> EditorWidget = ComponentEditWidget->GetEditorWidget())
					{
						EditorWidget->InvalidateComponentEditWidget();
					}
				}
			})
	);
}

TSharedRef<SWidget> SDMComponentEdit::CreateExtensionButtons(const TSharedPtr<SWidget>& InPropertyOwner, UDMMaterialComponent* InComponent,
	const FName& InPropertyName, bool bInAllowKeyframe, FSimpleDelegate InOnResetDelegate)
{
	if (!IsValid(InComponent))
	{
		return SNew(SBox)
			.WidthOverride(42.f)
			.HeightOverride(18.f);
	}

	FProperty* Property = InComponent->GetClass()->FindPropertyByName(InPropertyName);

	if (!Property)
	{
		return SNew(SBox)
			.WidthOverride(42.f)
			.HeightOverride(18.f);
	}

	FDMPropertyHandle DMPropertyHandle = SDMEditor::GetPropertyHandle(InPropertyOwner.Get(), InComponent, InPropertyName);
	TSharedPtr<IPropertyHandle> PropertyHandle = DMPropertyHandle.PropertyHandle;
	TSharedPtr<IDetailTreeNode> DetailTreeNode = DMPropertyHandle.DetailTreeNode;

	if (PropertyHandle.IsValid())
	{
		ensure(PropertyHandle->GetProperty() == Property);
	}

	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InComponent;
	ExtensionRowArgs.Property = Property;
	ExtensionRowArgs.OwnerTreeNode = DetailTreeNode;
	ExtensionRowArgs.PropertyHandle = PropertyHandle;

	FSlimHorizontalToolBarBuilder ToolBarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TArray<FPropertyRowExtensionButton> ExtensionButtons;
	PropertyEditorModule.GetGlobalRowExtensionDelegate().Broadcast(ExtensionRowArgs, ExtensionButtons);

	TSharedRef<SWidget> ResetButtonWidget = PropertyCustomizationHelpers::MakeResetButton(
		FSimpleDelegate::CreateLambda([PropertyHandle, InOnResetDelegate]()
			{
				if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
				{
					TArray<UObject*> Outers;
					PropertyHandle->GetOuterObjects(Outers);

					if (Outers.IsEmpty() == false && IsValid(Outers[0]))
					{
						FDMScopedUITransaction Transaction(LOCTEXT("ResetValue", "Reset Value to default."));

						if (FProperty* Property = PropertyHandle->GetProperty())
						{
							Outers[0]->Modify();

							if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Outers[0]))
							{
								Value->ApplyDefaultValue();
							}
							else
							{
								void* DefaultValue = Property->ContainerPtrToValuePtr<void>(Outers[0]->GetClass()->GetDefaultObject(true));

								if (DefaultValue != nullptr)
								{
									if (Property->HasSetter())
									{
										Property->CallSetter(Outers[0], DefaultValue);
									}
									else
									{
										Outers[0]->PreEditChange(Property);

										Property->SetValue_InContainer(Outers[0], DefaultValue);

										FPropertyChangedEvent PCE = FPropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
										Outers[0]->PostEditChangeProperty(PCE);
									}
								}
							}
						}
						else
						{
							PropertyHandle->ResetToDefault();
						}

						InOnResetDelegate.ExecuteIfBound();
					}
				}
			})
	);
	ResetButtonWidget->SetVisibility(
		TAttribute<EVisibility>::CreateLambda([PropertyHandle]()->EVisibility
			{
				if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
				{
					if (FProperty* Property = PropertyHandle->GetProperty())
					{
						TArray<UObject*> Outers;
						PropertyHandle->GetOuterObjects(Outers);

						if (Outers.IsEmpty() == false && IsValid(Outers[0]))
						{
							if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Outers[0]))
							{
								return Value->IsDefaultValue()
									? EVisibility::Hidden
									: EVisibility::Visible;
							}

							return Property->Identical_InContainer(Outers[0], Outers[0]->GetClass()->GetDefaultObject(true))
								? EVisibility::Hidden
								: EVisibility::Visible;
						}
					}

					return PropertyHandle->CanResetToDefault() ? EVisibility::Visible : EVisibility::Hidden;
				}

				return EVisibility::Hidden;
			})
	);

	ToolBarBuilder.AddWidget(ResetButtonWidget);

	bool bAddedSequencerButtons = false;

	// Sequencer relies on getting the Keyframe Handler via the Details View of the IDetailTreeNode, but it's null since
	// there's no Details View here. Instead add it manually.
	if (bInAllowKeyframe && PropertyHandle.IsValid())
	{
		if (UWorld* World = InComponent->GetWorld())
		{
			if (UDMWorldSubsystem* DMWorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
			{
				if (const TSharedPtr<IDetailKeyframeHandler>& KeyframeHandler = DMWorldSubsystem->GetKeyframeHandler())
				{
					TArray<FPropertyRowExtensionButton> SequencerButtons;

					FCustomDetailsViewSequencerUtils::CreateSequencerExtensionButton(
						KeyframeHandler,
						PropertyHandle,
						SequencerButtons
					);

					for (const FPropertyRowExtensionButton& SequencerButton : SequencerButtons)
					{
						ToolBarBuilder.AddToolBarButton(
							SequencerButton.UIAction,
							NAME_None,
							TAttribute<FText>(),
							SequencerButton.ToolTip,
							SequencerButton.Icon
						);

						bAddedSequencerButtons = true;
					}
				}
			}
		}
	}

	if (!bAddedSequencerButtons)
	{
		// Maintain space
		ToolBarBuilder.AddWidget(
			SNew(SBox)
			.WidthOverride(20.f)
			.HeightOverride(18.f)
		);
	}

	if (!ExtensionButtons.IsEmpty())
	{
		// Build extension toolbar 
		ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		ToolBarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");

		for (const FPropertyRowExtensionButton& Extension : ExtensionButtons)
		{
			ToolBarBuilder.AddToolBarButton(Extension.UIAction, NAME_None, Extension.Label, Extension.ToolTip, Extension.Icon);
		}
	}

	return ToolBarBuilder.MakeWidget();
}

TSharedPtr<SWidget> SDMComponentEdit::CreateSinglePropertyEditWidget(UDMMaterialComponent* InComponent, const FName& InPropertyName)
{
	if (!IsValid(InComponent))
	{
		return nullptr;
	}

	FProperty* ClassProperty = InComponent->GetClass()->FindPropertyByName(InPropertyName);

	if (!ClassProperty)
	{
		return nullptr;
	}

	FDMPropertyHandle PropertyHandle = SDMEditor::GetPropertyHandle(this, InComponent, InPropertyName);

	if (PropertyHandle.PropertyHandle.IsValid())
	{
		if (ClassProperty->IsA<FBoolProperty>())
		{
			return SNew(SDMPropertyEditBool, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (ClassProperty->IsA<FEnumProperty>())
		{
			return SNew(SDMPropertyEditEnum, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (ClassProperty->IsA<FFloatProperty>())
		{
			return SNew(SDMPropertyEditFloat, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(ClassProperty))
		{
			return SNew(SDMPropertyEditObject, PropertyHandle.PropertyHandle, ObjectProperty->PropertyClass)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(ClassProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				return SNew(SDMPropertyEditVector, PropertyHandle.PropertyHandle, 2)
					.ComponentEditWidget(SharedThis(this));
			}
			else if (StructProperty->Struct == TBaseStructure<FVector>::Get()
				|| StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				return SNew(SDMPropertyEditVector, PropertyHandle.PropertyHandle, 3)
					.ComponentEditWidget(SharedThis(this));
			}
			else if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return SNew(SDMPropertyEditColor, PropertyHandle.PropertyHandle)
					.ComponentEditWidget(SharedThis(this));
			}
		}

		return nullptr;
	}

	// This is a complete fallback that does not layout correctly.
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FSinglePropertyParams InitParams;
	InitParams.NamePlacement = EPropertyNamePlacement::Hidden;
	InitParams.NotifyHook = InComponent;

	return PropertyEditor.CreateSingleProperty(InComponent, InPropertyName, InitParams);
}

TSharedRef<SWidget> SDMComponentEdit::MakeSourceTypeEditWidgetMenuContent()
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (UDMMaterialSlot* Slot = Layer->GetSlot())
			{
				if (TSharedPtr<SDMEditor> EditorWidget = GetEditorWidget())
				{
					if (TSharedPtr<SDMSlot> SlotWidget = EditorWidget->GetActiveSlotWidget())
					{
						if (SlotWidget->GetSlot() == Slot)
						{
							if (TSharedPtr<SDMStage> StageWidget = SlotWidget->FindStageWidget(Stage))
							{
								return FDMMaterialStageSourceMenus::MakeChangeSourceMenu(SlotWidget, StageWidget);
							}
						}
					}
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

FText SDMComponentEdit::GetSourceTypeEditWidgetText() const
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (UDMMaterialStageSource* Source = Stage->GetSource())
		{
			return Source->GetStageDescription();
		}
	}

	return FText::GetEmpty();
}

void SDMComponentEdit::OnUndo()
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (Layer->GetStageType(Stage) == EDMMaterialLayerStage::Mask)
			{
				if (bCreatedWithLinkedUVs != Layer->IsTextureUVLinkEnabled())
				{
					if (TSharedPtr<SDMEditor> EditorWidget = GetEditorWidget())
					{
						EditorWidget->InvalidateComponentEditWidget();
					}
				}
			}
		}
	}
}

void SDMComponentEdit::OnExpansionStateChanged(const TSharedRef<ICustomDetailsViewItem>& InItem, bool bInExpansionState)
{
	if (bConstructing)
	{
		return;
	}

	const FCustomDetailsViewItemId& ItemId = InItem->GetItemId();

	if (ItemId.GetItemType() != static_cast<uint32>(EDetailNodeType::Category))
	{
		return;
	}

	TSharedPtr<SDMEditor> EditorWidget = GetEditorWidget();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	EditorWidget->SetExpansionState(ComponentWeak.Get(), *ItemId.GetItemName(), bInExpansionState);
}

void SDMComponentEdit::GenerateMaterialModelPropertyRows(const TSharedRef<SDMEditor> InEditorWidget, UDynamicMaterialModelBase* InMaterialModelBase,
	bool bInDynamic, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	UDynamicMaterialModel* MaterialModel = InMaterialModelBase->ResolveMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	const FName MaterialSettingsCategory = FName("Material Settings");

	auto AddGlobalValue = [InEditorWidget, &InOutPropertyRows, &InOutProcessedObjects, &MaterialSettingsCategory, InMaterialModelBase]
		(UDMMaterialComponent* InComponent, const FText& InNameOverride)
		{
			if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(InMaterialModelBase))
			{
				InComponent = MaterialModelDynamic->GetComponentDynamic(InComponent->GetFName());

				if (!InComponent)
				{
					return;
				}
			}

			FDMPropertyHandle& ComponentHandle = InOutPropertyRows.Add_GetRef(InEditorWidget->GetPropertyHandle(&*InEditorWidget,
				InComponent, UDMMaterialValue::ValueName));

			ComponentHandle.CategoryOverrideName = MaterialSettingsCategory;
			ComponentHandle.NameOverride = InNameOverride;

			if (UDMMaterialValue* MaterialValue = Cast<UDMMaterialValue>(InComponent))
			{
				ComponentHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateUObject(MaterialValue, &UDMMaterialValue::CanResetToDefault),
					FResetToDefaultHandler::CreateUObject(MaterialValue, &UDMMaterialValue::ResetToDefault),
					/* Propagate to children */ false
				);
			}
			else if (UDMMaterialValueDynamic* MaterialValueDynamic = Cast<UDMMaterialValueDynamic>(InComponent))
			{
				ComponentHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateUObject(MaterialValueDynamic, &UDMMaterialValueDynamic::CanResetToDefault),
					FResetToDefaultHandler::CreateUObject(MaterialValueDynamic, &UDMMaterialValueDynamic::ResetToDefault),
					/* Propagate to children */ false
				);
			}

			InOutProcessedObjects.Add(InComponent);
		};

	AddGlobalValue(MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOffsetValueName), LOCTEXT("GlobalOffset", "Global Offset"));
	AddGlobalValue(MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalTilingValueName), LOCTEXT("GlobalTiling", "Global Tiling"));
	AddGlobalValue(MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalRotationValueName), LOCTEXT("GlobalRotation", "Global Rotation"));

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModelBase))
	{
		if (EditorOnlyData->GetBlendMode() != BLEND_Opaque)
		{
			AddGlobalValue(MaterialModel->GetGlobalParameterValue(UDynamicMaterialModel::GlobalOpacityValueName), LOCTEXT("GlobalOpacity", "Global Opacity"));
		}

		const FText PropertyFormat = LOCTEXT("PropertyFormat", "Global {0}");
		UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

		EditorOnlyData->ForEachMaterialPropertyType(
			[EditorOnlyData , &AddGlobalValue, &PropertyFormat, MaterialPropertyEnum]
			(EDMMaterialPropertyType InProperty)
			{
				if (EditorOnlyData->GetSlotForMaterialProperty(InProperty))
				{
					if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InProperty))
					{
						if (MaterialProperty->IsValidForModel(*EditorOnlyData))
						{
							if (UDMMaterialValueFloat1* AlphaValue = Cast<UDMMaterialValueFloat1>(MaterialProperty->GetComponent(UDynamicMaterialModelEditorOnlyData::AlphaValueName)))
							{
								AddGlobalValue(
									AlphaValue,
									FText::Format(
										PropertyFormat,
										MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(InProperty))
									)
								);
							}
						}
					}
				}

				return EDMIterationResult::Continue;
			}, 
			/* Start from */ EDMMaterialPropertyType::Roughness
		);
	}

	const FName MaterialTypeCategory = FName("Material Type");
	auto AddVariable = [InEditorWidget, &InOutPropertyRows, &MaterialTypeCategory, bInDynamic](UObject* InObject, FName InPropertyName)
		{
			FDMPropertyHandle& ValueHandle = InOutPropertyRows.Add_GetRef(InEditorWidget->GetPropertyHandle(&*InEditorWidget,
				InObject, InPropertyName));

			ValueHandle.CategoryOverrideName = MaterialTypeCategory;
			ValueHandle.bEnabled = !bInDynamic;
		};

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModelBase))
	{
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, ChannelListPreset));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, Domain));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, BlendMode));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, ShadingModel));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bHasPixelAnimation));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bTwoSided));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bResponsiveAAEnabled));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bOutputTranslucentVelocityEnabled));
		AddVariable(EditorOnlyData, GET_MEMBER_NAME_CHECKED(UDynamicMaterialModelEditorOnlyData, bNaniteTessellationEnabled));
	}
}

TSharedRef<SWidget> SDMComponentEdit::CreateSourceTypeEditWidget()
{
	TWeakPtr<SDMComponentEdit> ThisWeak = SharedThis(this);

	return SNew(SBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 0.f, 0.2f)
		[
			SNew(SComboButton)
				.HasDownArrow(false)
				.IsFocusable(true)
				.ContentPadding(4.0f)
				.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
				.ToolTipText(LOCTEXT("ChangeLayer", "Change Stage Type"))
				.OnGetMenuContent(this, &SDMComponentEdit::MakeSourceTypeEditWidgetMenuContent)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &SDMComponentEdit::GetSourceTypeEditWidgetText)
				]
		];
}

void SDMComponentEdit::CreateKeyFrame(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	TArray<UObject*> Outers;
	InPropertyHandle->GetOuterObjects(Outers);

	if (Outers.IsEmpty() || !IsValid(Outers[0]))
	{
		return;
	}

	UDMMaterialComponent* Component = Cast<UDMMaterialComponent>(Outers[0]);

	if (!IsValid(Component))
	{
		return;
	}

	if (!InPropertyHandle.IsValid())
	{
		return;
	}

	const UWorld* const World = Component->GetWorld();
	if (!World)
	{
		return;
	}

	const UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();
	if (!WorldSubsystem)
	{
		return;
	}

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
	if (!KeyframeHandler.IsValid())
	{
		return;
	}

	if (!KeyframeHandler->IsPropertyKeyable(Component->GetClass(), *InPropertyHandle))
	{
		return;
	}

	KeyframeHandler->OnKeyPropertyClicked(*InPropertyHandle);
}

#undef LOCTEXT_NAMESPACE
