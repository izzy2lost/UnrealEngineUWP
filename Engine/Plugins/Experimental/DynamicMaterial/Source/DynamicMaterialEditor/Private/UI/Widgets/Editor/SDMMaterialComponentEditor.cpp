// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"

#include "DynamicMaterialEditorModule.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialLayer.h"
#include "DynamicMaterialModule.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SDMMaterialComponentEditor"

void SDMMaterialComponentEditor::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

SDMMaterialComponentEditor::~SDMMaterialComponentEditor()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialComponent* Component = GetComponent())
	{
		Component->GetOnUpdate().RemoveAll(this);
	}
}

void SDMMaterialComponentEditor::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget, 
	UDMMaterialComponent* InMaterialComponent)
{
	SetCanTick(false);

	SDMObjectEditorWidgetBase::Construct(
		SDMObjectEditorWidgetBase::FArguments(), 
		InEditorWidget, 
		InMaterialComponent
	);

	if (InMaterialComponent)
	{
		InMaterialComponent->GetOnUpdate().AddSP(this, &SDMMaterialComponentEditor::OnComponentUpdated);
	}
}

UDMMaterialComponent* SDMMaterialComponentEditor::GetComponent() const
{
	return Cast<UDMMaterialComponent>(ObjectWeak.Get());
}

void SDMMaterialComponentEditor::OnComponentUpdated(UDMMaterialComponent* InComponent, UDMMaterialComponent* InSource, EDMUpdateType InUpdateType)
{
	if (EnumHasAnyFlags(InUpdateType, EDMUpdateType::Structure))
	{
		if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
		{
			EditorWidget->EditComponent(GetComponent(), /* Force refresh */ true);
		}
	}
}

TArray<FDMPropertyHandle> SDMMaterialComponentEditor::GetPropertyRows()
{
	TArray<FDMPropertyHandle> PropertyRows;
	TSet<UDMMaterialComponent*> ProcessedObjects;

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
		SharedThis(this),
		GetComponent(),
		PropertyRows,
		ProcessedObjects
	);

	return PropertyRows;
}

void SDMMaterialComponentEditor::OnUndo()
{
	if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
	{
		EditorWidget->EditComponent(GetComponent(), /* Force refresh */ true);
	}
}

#undef LOCTEXT_NAMESPACE
