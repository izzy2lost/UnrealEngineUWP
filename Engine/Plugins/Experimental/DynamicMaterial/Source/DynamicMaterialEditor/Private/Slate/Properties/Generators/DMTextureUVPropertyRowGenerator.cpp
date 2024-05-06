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
	void AddPropertyRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, FName InProperty, 
		TArray<FDMPropertyHandle>& InOutPropertyRows);

	void AddVisualizerRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, 
		TArray<FDMPropertyHandle>& InOutPropertyRows);

	bool CanResetTextureUVPropertyToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle);

	void ResetTextureUVPropertyToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle);
}

void FDMTextureUVPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, 
	UDMMaterialComponent* InComponent, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
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
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Tiling, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_Pivot, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnX, InOutPropertyRows);
	AddPropertyRow(InComponentEditWidget, TextureUV, UDMTextureUV::NAME_bMirrorOnY, InOutPropertyRows);
	AddVisualizerRow(InComponentEditWidget, TextureUV, InOutPropertyRows);
}

void FDMTextureUVPropertyRowGenerator::AddPopoutComponentProperties(const TSharedRef<SWidget>& InParentWidget, UDMMaterialComponent* InComponent, 
	TArray<FDMPropertyHandle>& InOutPropertyRows)
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
	AddPropertyRow(InParentWidget, TextureUV, UDMTextureUV::NAME_Tiling, InOutPropertyRows);
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

void UE::DynamicMaterialEditor::Private::AddPropertyRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, 
	FName InProperty, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	FDMPropertyHandle& NewHandle = InOutPropertyRows.Add_GetRef(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, InProperty));

	NewHandle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateStatic(&UE::DynamicMaterialEditor::Private::CanResetTextureUVPropertyToDefault),
		FResetToDefaultHandler::CreateStatic(&UE::DynamicMaterialEditor::Private::ResetTextureUVPropertyToDefault)
	);
}

void UE::DynamicMaterialEditor::Private::AddVisualizerRow(const TSharedRef<SWidget>& InComponentEditWidget, UDMTextureUV* InTextureUV, 
	TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	// Make sure we don't get a substage
	UDMMaterialStage* Stage = InTextureUV->GetTypedParent<UDMMaterialStage>(/* Allow Subclasses */ false);

	if (!Stage)
	{
		return;
	}

	FDMPropertyHandle VisualizerHandle;
	VisualizerHandle.NameOverride = LOCTEXT("Visualizer", "UV Visualizer");
	VisualizerHandle.NameToolTipOverride = LOCTEXT("VisualizerToolTip", "A graphical Texture UV editor.\n\n- Offset Mode: Change the Texture UV offset.\n- Pivot Mode: Change the Texture UV pivot, rotation and tiling.\n\nControl+click to reset values to default.");
	VisualizerHandle.ValueName = FName(*InTextureUV->GetComponentPath());
	VisualizerHandle.ValueWidget = SNew(SDMTextureUVVisualizerProperty, Stage, InTextureUV);
	VisualizerHandle.CategoryOverrideName = TEXT("Texture UV");
	InOutPropertyRows.Add(VisualizerHandle);
}


bool UE::DynamicMaterialEditor::Private::CanResetTextureUVPropertyToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	FProperty* Property = InPropertyHandle->GetProperty();

	if (!Property)
	{
		return false;
	}

	FName PropertyName = Property->GetFName();

	if (PropertyName == NAME_None)
	{
		return false;
	}

	TArray<UObject*> Outers;
	InPropertyHandle->GetOuterObjects(Outers);

	if (Outers.IsEmpty())
	{
		return false;
	}

	const UDMTextureUV* PropertyObject = Cast<UDMTextureUV>(Outers[0]);

	if (!PropertyObject)
	{
		return false;
	}

	const UDMTextureUV* DefaultObject = GetDefault<UDMTextureUV>();

	if (!DefaultObject)
	{
		return false;
	}

	if (PropertyName == UDMTextureUV::NAME_UVSource)
	{
		return DefaultObject->GetUVSource() != PropertyObject->GetUVSource();
	}

	if (PropertyName == UDMTextureUV::NAME_bMirrorOnX)
	{
		return DefaultObject->GetMirrorOnX() != PropertyObject->GetMirrorOnX();
	}

	if (PropertyName == UDMTextureUV::NAME_bMirrorOnY)
	{
		return DefaultObject->GetMirrorOnY() != PropertyObject->GetMirrorOnY();
	}

	if (PropertyName == UDMTextureUV::NAME_Offset)
	{
		return !DefaultObject->GetOffset().Equals(PropertyObject->GetOffset());
	}

	if (PropertyName == UDMTextureUV::NAME_Pivot)
	{
		return !DefaultObject->GetPivot().Equals(PropertyObject->GetPivot());
	}

	if (PropertyName == NAME_Rotation)
	{
		return DefaultObject->GetRotation() != PropertyObject->GetRotation();
	}

	if (PropertyName == UDMTextureUV::NAME_Tiling)
	{
		return !DefaultObject->GetTiling().Equals(PropertyObject->GetTiling());
	}

	return false;
}

void UE::DynamicMaterialEditor::Private::ResetTextureUVPropertyToDefault(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	InPropertyHandle->ResetToDefault();
}

#undef LOCTEXT_NAMESPACE
