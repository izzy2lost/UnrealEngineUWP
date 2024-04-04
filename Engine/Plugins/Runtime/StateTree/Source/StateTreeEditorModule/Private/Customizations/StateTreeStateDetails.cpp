// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "StateTree.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreeSchema.h"
#include "Debugger/StateTreeDebuggerUIExtensions.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"


TSharedRef<IDetailCustomization> FStateTreeStateDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateDetails);
}

void FStateTreeStateDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Find StateTreeEditorData associated with this panel.
	UStateTreeEditorData* EditorData = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (const UObject* Object = WeakObject.Get())
		{
			if (UStateTreeEditorData* OuterEditorData = Object->GetTypedOuter<UStateTreeEditorData>())
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;
	const FString SchemaPath = Schema ? Schema->GetClass()->GetPathName() : FString();
	
	const TSharedPtr<IPropertyHandle> IDProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, ID));
	const TSharedPtr<IPropertyHandle> NameProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Name));
	const TSharedPtr<IPropertyHandle> TagProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Tag));
	const TSharedPtr<IPropertyHandle> ColorRefProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, ColorRef));
	const TSharedPtr<IPropertyHandle> EnabledProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, bEnabled));
	const TSharedPtr<IPropertyHandle> TasksProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Tasks));
	const TSharedPtr<IPropertyHandle> SingleTaskProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, SingleTask));
	const TSharedPtr<IPropertyHandle> EnterConditionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, EnterConditions));
	const TSharedPtr<IPropertyHandle> TransitionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Transitions));
	const TSharedPtr<IPropertyHandle> TypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Type));
	const TSharedPtr<IPropertyHandle> LinkedSubtreeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, LinkedSubtree));
	const TSharedPtr<IPropertyHandle> LinkedAssetProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, LinkedAsset));
	const TSharedPtr<IPropertyHandle> ParametersProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Parameters));
	const TSharedPtr<IPropertyHandle> SelectionBehaviorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, SelectionBehavior));
	const TSharedPtr<IPropertyHandle> RequiredEventToEnterProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, RequiredEventToEnter));
	const TSharedPtr<IPropertyHandle> CheckPrerequisitesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, bCheckPrerequisitesWhenActivatingChildDirectly));
	


	// Never show enabled
	EnabledProperty->MarkHiddenByCustomization();

	// Show ID only for debugging
	if (UE::StateTree::Editor::GbDisplayItemIds == false)
	{
		IDProperty->MarkHiddenByCustomization();
	}

	uint8 StateTypeValue = 0;
	TypeProperty->GetValue(StateTypeValue);
	const EStateTreeStateType StateType = (EStateTreeStateType)StateTypeValue;

	IDetailCategoryBuilder& StateCategory = DetailBuilder.EditCategory(TEXT("State"), LOCTEXT("StateDetailsState", "State"));
	StateCategory.SetSortOrder(0);

	StateCategory.HeaderContent(UE::StateTreeEditor::DebuggerExtensions::CreateStateWidget(DetailBuilder, EditorData));

	// Name
	NameProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(NameProperty);

	// Tag
	TagProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(TagProperty);

	// Color
	ColorRefProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(ColorRefProperty);

	// Type
	TypeProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(TypeProperty);

	// Per state type properties
	SelectionBehaviorProperty->MarkHiddenByCustomization();
	LinkedSubtreeProperty->MarkHiddenByCustomization();
	LinkedAssetProperty->MarkHiddenByCustomization();

	if (StateType == EStateTreeStateType::State)
	{
		StateCategory.AddProperty(SelectionBehaviorProperty);
	}
	else if (StateType == EStateTreeStateType::Linked)
	{
		StateCategory.AddProperty(LinkedSubtreeProperty);
	}
	else if (StateType == EStateTreeStateType::LinkedAsset)
	{
		// Custom widget for the linked asset, to add filtering to the assets.
		IDetailPropertyRow& Row = StateCategory.AddProperty(LinkedAssetProperty);
		Row.CustomWidget()
		.NameContent()
		[
			LinkedAssetProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(LinkedAssetProperty)
			.AllowedClass(UStateTree::StaticClass())
			.ThumbnailPool(DetailBuilder.GetPropertyUtilities()->GetThumbnailPool())
			.OnShouldFilterAsset_Lambda([SchemaPath](const FAssetData& InAssetData)
			{
				return !SchemaPath.IsEmpty() && !InAssetData.TagsAndValues.ContainsKeyValue(UE::StateTree::SchemaTag, SchemaPath);
			})
		];
	}

	CheckPrerequisitesProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(CheckPrerequisitesProperty);

	// Parameters
	ParametersProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(ParametersProperty);

	// Event
	RequiredEventToEnterProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(RequiredEventToEnterProperty);

	// Enter conditions
	const FName EnterConditionsCategoryName(TEXT("Enter Conditions"));
	if (Schema && Schema->AllowEnterConditions())
	{
		MakeArrayCategory(DetailBuilder, EnterConditionsCategoryName, LOCTEXT("StateDetailsEnterConditions", "Enter Conditions"), 2, EnterConditionsProperty);
	}
	else
	{
		DetailBuilder.EditCategory(EnterConditionsCategoryName).SetCategoryVisibility(false);
	}

	// Tasks
	if ((StateType == EStateTreeStateType::State || StateType == EStateTreeStateType::Subtree))
	{
		if (Schema && Schema->AllowMultipleTasks())
		{
			const FName TasksCategoryName(TEXT("Tasks"));
			MakeArrayCategory(DetailBuilder, TasksCategoryName, LOCTEXT("StateDetailsTasks", "Tasks"), 3, TasksProperty);
			SingleTaskProperty->MarkHiddenByCustomization();
		}
		else
		{
			const FName TaskCategoryName(TEXT("Task"));
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TaskCategoryName);
			Category.SetSortOrder(3);
			
			IDetailPropertyRow& Row = Category.AddProperty(SingleTaskProperty);
			Row.ShouldAutoExpand(true);
			
			TasksProperty->MarkHiddenByCustomization();
		}
	}
	else
	{
		SingleTaskProperty->MarkHiddenByCustomization();
		TasksProperty->MarkHiddenByCustomization();
	}

	// Transitions
	MakeArrayCategory(DetailBuilder, "Transitions", LOCTEXT("StateDetailsTransitions", "Transitions"), 4, TransitionsProperty);

	// Refresh the UI when the type changes.	
	TSharedPtr<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
	TypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropUtils] ()
	{
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}));
}

void FStateTreeStateDetails::MakeArrayCategory(IDetailLayoutBuilder& DetailBuilder, FName CategoryName, const FText& DisplayName, int32 SortOrder, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, DisplayName);
	Category.SetSortOrder(SortOrder);

	const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox)
		.IsEnabled(DetailBuilder.GetPropertyUtilities(), &IPropertyUtilities::IsPropertyEditingEnabled);

	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		PropertyHandle->CreateDefaultPropertyButtonWidgets()
	];
	Category.HeaderContent(HeaderContentWidget);

	// Add items inline
	const TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(PropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		ChildrenBuilder.AddProperty(PropertyHandle);
	}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}

#undef LOCTEXT_NAMESPACE
