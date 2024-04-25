// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMTextureUVPropertyRowGenerator.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMTextureUV.h"
#include "DMEDefs.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Slate/Properties/Editors/SDMPropertyEditVector.h"
#include "Slate/Properties/SDMTextureUVVisualizerProperty.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMEditor.h"
#include "Styling/SlateIconFinder.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "DMTextureUVPropertyRowGenerator"

const TSharedRef<FDMTextureUVPropertyRowGenerator>& FDMTextureUVPropertyRowGenerator::Get()
{
	static TSharedRef<FDMTextureUVPropertyRowGenerator> Generator = MakeShared<FDMTextureUVPropertyRowGenerator>();
	return Generator;
}

namespace UE::DynamicMaterialEditor::Private
{
	void AddPropertyRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, FName InProperty, TArray<FDMPropertyHandle>& InOutPropertyRows);
	void AddVisualizerRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows);
}

void FDMTextureUVPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMTextureUV* TextureUV = Cast<UDMTextureUV>(InComponent);

	if (!TextureUV)
	{
		return;
	}

	InOutProcessedObjects.Add(InComponent);

	using namespace UE::DynamicMaterialEditor::Private;

	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Offset, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Rotation, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Scale, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Pivot, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnX, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnY, InOutPropertyRows);
	AddVisualizerRow(InComponentEditWidget, TextureUV, InOutPropertyRows);
}

void FDMTextureUVPropertyRowGenerator::AddPopoutComponentProperties(const TSharedRef<SWidget>& InParentWidget, UDMMaterialComponent* InComponent, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	UDMTextureUV* TextureUV = Cast<UDMTextureUV>(InComponent);

	if (!TextureUV)
	{
		return;
	}

	using namespace UE::DynamicMaterialEditor::Private;

	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_Offset, InOutPropertyRows);
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_Rotation, InOutPropertyRows);
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_Scale, InOutPropertyRows);
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_Pivot, InOutPropertyRows);
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnX, InOutPropertyRows);
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnY, InOutPropertyRows);
}

bool FDMTextureUVPropertyRowGenerator::AllowKeyframeButton(UDMMaterialComponent* InComponent, FProperty* InProperty)
{
	if (InProperty)
	{
		const bool* AddKeyframeButtonPtr = UDMTextureUV::TextureProperties.Find(InProperty->GetFName());

		if (AddKeyframeButtonPtr)
		{
			return *AddKeyframeButtonPtr;
		}
	}

	return FDMComponentPropertyRowGenerator::AllowKeyframeButton(InComponent, InProperty);
}

void UE::DynamicMaterialEditor::Private::AddPropertyRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, FName InProperty, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	FDMPropertyHandle& NewHandle = InOutPropertyRows.Add_GetRef(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, InProperty));

	NewHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateUObject(InTextureUV, &UDMTextureUV::CanResetToDefault),
		FResetToDefaultHandler::CreateUObject(InTextureUV, &UDMTextureUV::ResetToDefault)
	);
}

void UE::DynamicMaterialEditor::Private::AddVisualizerRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	// Make sure we don't get a substage
	UDMMaterialStage* Stage = InTextureUV->GetTypedParent<UDMMaterialStage>(/* Allow Subclasses */ false);

	if (!Stage)
	{
		return;
	}

	FDMPropertyHandle VisualizerHandle;
	VisualizerHandle.NameOverride = LOCTEXT("Visualizer", "UV Visualizer");
	VisualizerHandle.NameToolTipOverride = LOCTEXT("VisualizerToolTip", "A graphical Texture UV editor.\n\n- Offset Mode: Change the Texture UV offset.\n- Pivot Mode: Change the Texture UV pivot, rotation and scale.\n\nControl+click to reset values to default.");
	VisualizerHandle.ValueName = FName(*InTextureUV->GetComponentPath());
	VisualizerHandle.ValueWidget = SNew(SDMTextureUVVisualizerProperty, Stage, InTextureUV);
	InOutPropertyRows.Add(VisualizerHandle);
}

#undef LOCTEXT_NAMESPACE
