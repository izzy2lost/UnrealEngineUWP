// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEEditorClonerComponentDetailCustomization.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/Extensions/CEClonerExtensionBase.h"
#include "Cloner/Extensions/CEClonerLifetimeExtension.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Input/Reply.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceCurve.h"
#include "PropertyEditorModule.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "CEEditorClonerComponentDetailCustomization"

void FCEEditorClonerComponentDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const FName ClassName = UCEClonerComponent::StaticClass()->GetFName();

	// Remove sections
	PropertyModule.RemoveSection(ClassName, TEXT("Rendering"));
	PropertyModule.RemoveSection(ClassName, TEXT("Effects"));

	// Remove exposed user parameters
	InDetailBuilder.HideCategory(TEXT("NiagaraComponent_Parameters"));

	// Remove niagara utilities
	InDetailBuilder.HideCategory(TEXT("NiagaraComponent_Utilities"));

	// Remove extension array property
	TSharedRef<IPropertyHandle> ActiveExtensionsProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEClonerComponent, ActiveExtensions), UCEClonerComponent::StaticClass());

	if (!ActiveExtensionsProperty->IsValidHandle())
	{
		return;
	}

	InDetailBuilder.HideProperty(ActiveExtensionsProperty);

	uint32 NumElements = 0;
	TSharedPtr<IPropertyHandleArray> ActiveExtensionArrayProperty = ActiveExtensionsProperty->AsArray();
	ActiveExtensionArrayProperty->GetNumElements(NumElements);

	// Everything needs to be below Cloner category
	const int32 StartOrder = InDetailBuilder.EditCategory(TEXT("Cloner")).GetSortOrder() + 1;

	TSharedRef<IPropertyHandle> ActiveLayoutProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEClonerComponent, ActiveLayout), UCEClonerComponent::StaticClass());

	if (!ActiveLayoutProperty->IsValidHandle())
	{
		return;
	}

	InDetailBuilder.EditCategory(ActiveLayoutProperty->GetDefaultCategoryName()).SetSortOrder(StartOrder);

	const TArray<TWeakObjectPtr<UCEClonerComponent>> ClonerComponentsWeak = InDetailBuilder.GetObjectsOfTypeBeingCustomized<UCEClonerComponent>();

	for (uint32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
	{
		TSharedPtr<IPropertyHandle> ActiveExtensionProperty = ActiveExtensionArrayProperty->GetElement(ElementIndex);

		UObject* ObjectValue;
		ActiveExtensionProperty->GetValue(ObjectValue);

		UCEClonerExtensionBase* ActiveExtension = Cast<UCEClonerExtensionBase>(ObjectValue);

		if (!IsValid(ActiveExtension))
		{
			continue;
		}

		IDetailCategoryBuilder& ExtensionCategoryBuilder = InDetailBuilder.EditCategory(ActiveExtension->GetExtensionName());
		ExtensionCategoryBuilder.SetSortOrder(StartOrder + ActiveExtension->GetExtensionCategoryOrder());

		const FName ExtensionSectionName = ActiveExtension->GetExtensionCategory();
		const TSharedRef<FPropertySection> ExtensionSection = PropertyModule.FindOrCreateSection(ClassName, ExtensionSectionName, FText::FromName(ExtensionSectionName));
		ExtensionSection->AddCategory(ActiveExtension->GetExtensionName());

		TFunction<void(const TSharedPtr<IPropertyHandle>&)> AddObjectProperty = [&ExtensionCategoryBuilder, &AddObjectProperty](const TSharedPtr<IPropertyHandle>& InPropertyHandle)
		{
			uint32 NumChildren = 0;
			InPropertyHandle->GetNumChildren(NumChildren);

			for (uint32 ChildrenIndex = 0; ChildrenIndex < NumChildren; ChildrenIndex++)
			{
				TSharedPtr<IPropertyHandle> ChildHandle = InPropertyHandle->GetChildHandle(ChildrenIndex);

				if (ChildHandle->IsCategoryHandle() && NumChildren == 1)
				{
					AddObjectProperty(ChildHandle);
				}
				else
				{
					ExtensionCategoryBuilder.AddProperty(ChildHandle);
				}
			}
		};

		AddObjectProperty(ActiveExtensionProperty->GetChildHandle(0));

		if (UCEClonerLifetimeExtension* LifetimeExtension = Cast<UCEClonerLifetimeExtension>(ActiveExtension))
		{
			UNiagaraDataInterfaceCurve* LifetimeScaleCurve = LifetimeExtension->GetLifetimeScaleCurveDI();

			if (!LifetimeScaleCurve || ClonerComponentsWeak.Num() > 1)
			{
				return;
			}

			const TArray<UObject*> ShowObjects {LifetimeScaleCurve};
			ExtensionCategoryBuilder.SetShowAdvanced(true);

			FAddPropertyParams Params;
			Params.HideRootObjectNode(true);
			Params.CreateCategoryNodes(false);

			IDetailPropertyRow* CurveRow = ExtensionCategoryBuilder.AddExternalObjects(ShowObjects, EPropertyLocation::Advanced, Params);

			TWeakObjectPtr<UCEClonerLifetimeExtension> LifetimeExtensionWeak(LifetimeExtension);
			CurveRow->Visibility(MakeAttributeLambda([LifetimeExtensionWeak]()
			{
				if (UCEClonerLifetimeExtension* LifetimeExtension = LifetimeExtensionWeak.Get())
				{
					return LifetimeExtension->GetLifetimeEnabled()
						&& LifetimeExtension->GetLifetimeScaleEnabled()
						? EVisibility::Visible
						: EVisibility::Collapsed;
				}

				return EVisibility::Visible;
			}));
		}
	}

	for (const TWeakObjectPtr<UCEClonerComponent>& ClonerComponentWeak : ClonerComponentsWeak)
	{
		UCEClonerComponent* ClonerComponent = ClonerComponentWeak.Get();

		if (!ClonerComponent)
		{
			continue;
		}

		// Look for ufunction in component
		for (UFunction* Function : TFieldRange<UFunction>(ClonerComponent->GetClass(), EFieldIteratorFlags::ExcludeSuper))
		{
			// Only CallInEditor function with 0 parameters
			if (Function && Function->HasMetaData("CallInEditor") && Function->NumParms == 0)
			{
				FName FunctionName = Function->GetFName();
				TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunctions = LayoutFunctionNames.FindOrAdd(FunctionName);
				ObjectFunctions.Add(ClonerComponent, Function);
			}
		}

		// Look for CallInEditor functions in active layout
		if (UCEClonerLayoutBase* ActiveLayout = ClonerComponent->GetActiveLayout())
		{
			// Iterate through all UFunctions in the class
			for (UFunction* Function : TFieldRange<UFunction>(ActiveLayout->GetClass(), EFieldIteratorFlags::ExcludeSuper))
			{
				// Only CallInEditor function with 0 parameters
				if (Function && Function->HasMetaData("CallInEditor") && Function->NumParms == 0)
				{
					FName FunctionName = Function->GetFName();
					TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunctions = LayoutFunctionNames.FindOrAdd(FunctionName);
					ObjectFunctions.Add(ActiveLayout, Function);
				}
			}
		}

		// Look for CallInEditor functions in active extensions
		for (const TObjectPtr<UCEClonerExtensionBase>& ActiveExtension : ClonerComponent->GetActiveExtensions())
		{
			// Iterate through all UFunctions in the class
			for (UFunction* Function : TFieldRange<UFunction>(ActiveExtension->GetClass(), EFieldIteratorFlags::ExcludeSuper))
			{
				// Only CallInEditor function with 0 parameters
				if (Function && Function->HasMetaData("CallInEditor") && Function->NumParms == 0)
				{
					FName FunctionName = Function->GetFName();
					TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunctions = LayoutFunctionNames.FindOrAdd(FunctionName);
					ObjectFunctions.Add(ActiveExtension, Function);
				}
			}
		}
	}

	if (!LayoutFunctionNames.IsEmpty())
	{
		// Add buttons for selected cloners
		const TSharedPtr<SVerticalBox> FunctionsWidget = SNew(SVerticalBox);

		IDetailCategoryBuilder& UtilitiesCategory = InDetailBuilder.EditCategory(TEXT("ClonerComponent_Utilities"), LOCTEXT("ClonerComponent_Utilities", "Cloner Utilities"), ECategoryPriority::Important);

		UtilitiesCategory.AddCustomRow(FText::GetEmpty())
			.WholeRowContent()
			.HAlign(HAlign_Left)
			[
				FunctionsWidget.ToSharedRef()
			];

		for (const TPair<FName, TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>>& LayoutFunctionNamesPair : LayoutFunctionNames)
		{
			const FText ButtonLabel = FText::FromString(FName::NameToDisplayString(LayoutFunctionNamesPair.Key.ToString(), false));

			FunctionsWidget->AddSlot()
				.Padding(0.f, 3.f)
				.AutoHeight()
				[
					SNew(SButton)
					.Text(ButtonLabel)
					.OnClicked(this, &FCEEditorClonerComponentDetailCustomization::OnFunctionButtonClicked, LayoutFunctionNamesPair.Key)
				];
		}
	}
}

FReply FCEEditorClonerComponentDetailCustomization::OnFunctionButtonClicked(FName InFunctionName)
{
	if (TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>> const* ObjectFunctions = LayoutFunctionNames.Find(InFunctionName))
	{
		for (const TPair<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunction : *ObjectFunctions)
		{
			UObject* Object = ObjectFunction.Key.Get();
			UFunction* Function = ObjectFunction.Value.Get();

			if (!Object || !Function)
			{
				continue;
			}

			Object->ProcessEvent(Function, nullptr);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
