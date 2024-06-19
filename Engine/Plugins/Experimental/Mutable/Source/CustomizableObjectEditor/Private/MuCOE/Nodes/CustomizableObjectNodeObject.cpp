// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"

#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "Misc/UObjectToken.h"
#include "Logging/MessageLog.h"
#include "Containers/Queue.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

const FName UCustomizableObjectNodeObject::ChildrenPinName(TEXT("Children"));
const FName UCustomizableObjectNodeObject::ComponentsPinName(TEXT("Components"));
const FName UCustomizableObjectNodeObject::OutputPinName(TEXT("Object"));
const TCHAR* UCustomizableObjectNodeObject::LODPinNamePrefix = TEXT("LOD ");

UCustomizableObjectNodeObject::UCustomizableObjectNodeObject()
	: Super()
{
	bIsBase = true;
	ObjectName = "Unnamed Object";
	NumLODs = 1;
	Identifier = FGuid::NewGuid();
}


void UCustomizableObjectNodeObject::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::StateTextureCompressionStrategyEnum)
	{
		for (FCustomizableObjectState& State : States)
		{
			if (State.TextureCompressionStrategy== ETextureCompressionStrategy::None
				&&
				State.bDontCompressRuntimeTextures_DEPRECATED)
			{
				State.bDontCompressRuntimeTextures_DEPRECATED = false;
				State.TextureCompressionStrategy = ETextureCompressionStrategy::DontCompressRuntime;
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::RegenerateNodeObjectsIds)
	{
		// This will regenerate all the Node Object Guids to finally remove the duplicated Guids warning.
		// It is safe to do this here as Node Object do not use its node guid to link themeselves to other nodes.
		CreateNewGuid();

		// This change may make cooks to become undeterministic, if the object GUID is finally used (it is a "toggle group" option).
		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(GetCustomizableObjectGraph()->GetOuter());
		if (CustomizableObject)
		{
			FMessageLog("Mutable").Message(EMessageSeverity::Info)
				->AddToken(FTextToken::Create(LOCTEXT("Indeterministic Warning", "The object was saved with an old version and it may generate indeterministic packages. Resave it to fix the problem.")))
				->AddToken(FUObjectToken::Create(CustomizableObject));
		}
	}

	// Update state never-stream flag from deprecated enum
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::CustomizableObjectStateHasSeparateNeverStreamFlag)
	{
		for (FCustomizableObjectState& s : States)
		{
			s.bDisableTextureStreaming = s.TextureCompressionStrategy != ETextureCompressionStrategy::None;
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::StateUIMetadata)
	{
		for (FCustomizableObjectState& State : States)
		{
			State.UIMetadata.ObjectFriendlyName = State.StateUIMetadata_DEPRECATED.ObjectFriendlyName;
			State.UIMetadata.UISectionName = State.StateUIMetadata_DEPRECATED.UISectionName;
			State.UIMetadata.UIOrder = State.StateUIMetadata_DEPRECATED.UIOrder;
			State.UIMetadata.UIThumbnail = State.StateUIMetadata_DEPRECATED.UIThumbnail;
			State.UIMetadata.ExtraInformation = State.StateUIMetadata_DEPRECATED.ExtraInformation;
			State.UIMetadata.ExtraAssets = State.StateUIMetadata_DEPRECATED.ExtraAssets;
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NewComponentOptions)
	{
		// Like we did in the CO components, we use the index of the component as the name of the component
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentSettings.Num(); ++ComponentIndex)
		{
			ComponentSettings[ComponentIndex].ComponentName = FString::FromInt(ComponentIndex);
		}
	}
}


void UCustomizableObjectNodeObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!Identifier.IsValid())
	{
		Identifier = FGuid::NewGuid();
	}

	// Update the cached flag in the main object
	UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>( GetCustomizableObjectGraph()->GetOuter() );
	if (CustomizableObject)
	{
		CustomizableObject->GetPrivate()->SetIsChildObject(ParentObject != nullptr);
	}

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs"))
	{
		NumLODs = FMath::Clamp(NumLODs, 1, 64);

		for (int32 CompSetIndex = 0; CompSetIndex < ComponentSettings.Num(); ++CompSetIndex)
		{
			ComponentSettings[CompSetIndex].LODReductionSettings.SetNum(NumLODs);
		}

		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeObject::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// NOTE: Ensure all built-in pins are handled in UCustomizableObjectNodeObject::IsBuiltInPin

	for (int32 i = 0; i < NumLODs; ++i)
	{
		FString LODName = FString::Printf(TEXT("%s%d "), LODPinNamePrefix, i);

		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(*LODName), true);
		Pin->bDefaultValueIsIgnored = true;
	}

	UEdGraphPin* ComponentsPin = CustomCreatePin(EGPD_Input, Schema->PC_Component, ComponentsPinName, true);
	ComponentsPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ChildrenPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, ChildrenPinName, true);
	ChildrenPin->bDefaultValueIsIgnored = true;

	for (const FRegisteredObjectNodeInputPin& Pin : ICustomizableObjectModule::Get().GetAdditionalObjectNodePins())
	{
		// Use the global pin name here to prevent extensions using the same pin names from
		// interfering with each other.
		//
		// This also prevents extension pins from clashing with the built-in pins from this node,
		// such as "Object".
		UEdGraphPin* GraphPin = CustomCreatePin(EGPD_Input, Pin.InputPin.PinType, Pin.GlobalPinName, Pin.InputPin.bIsArray);

		GraphPin->PinFriendlyName = Pin.InputPin.DisplayName;
	}

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Object, OutputPinName);

	if (bIsBase)
	{
		OutputPin->bHidden = true;
	}
}


FText UCustomizableObjectNodeObject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView ||
		ObjectName.IsEmpty())
	{
		if (bIsBase)
		{
			return LOCTEXT("Base_Object", "Base Object");
		}
		else
		{
			return LOCTEXT("Base_Object_Deprecated", "Base Object (Deprecated)");
		}
	}
	else
	{
		FFormatNamedArguments Args;	
		Args.Add(TEXT("ObjectName"), FText::FromString(ObjectName) );

		if (bIsBase)
		{
			return FText::Format(LOCTEXT("Base_Object_Title", "{ObjectName}\nBase Object"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("Child_Object_Title_Deprecated", "{ObjectName}\nChild Object (Deprecated)"), Args);
		}
	}
}


FLinearColor UCustomizableObjectNodeObject::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Object);
}


void UCustomizableObjectNodeObject::PrepareForCopying()
{
	FText Msg(LOCTEXT("Cannot copy object node","There can only be one Customizable Object Node Object element per graph") );
	FMessageLog MessageLog("Mutable");
	MessageLog.Notify(Msg, EMessageSeverity::Info, true);
}


int32 UCustomizableObjectNodeObject::GetLOD(UEdGraphPin* Pin) const
{
	for (int32 LOD = 0; LOD < GetNumLODPins(); ++LOD)
	{
		if (Pin == LODPin(LOD))
		{
			return LOD;
		}
	}

	return -1;
}


bool UCustomizableObjectNodeObject::CanUserDeleteNode() const
{
	return !bIsBase;
}


bool UCustomizableObjectNodeObject::CanDuplicateNode() const
{
	return !bIsBase;
}


TArray<UCustomizableObjectNodeMaterialBase*> UCustomizableObjectNodeObject::GetMaterialNodes(const int LOD) const
{
	TArray<UCustomizableObjectNodeMaterialBase*> Result;

	TQueue<UEdGraphNode*> PotentialCustomizableNodeObjects;

	for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*LODPin(LOD)))
	{
		PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
	}

	UEdGraphNode* CurrentElement;
	while (PotentialCustomizableNodeObjects.Dequeue(CurrentElement))
	{
		if (UCustomizableObjectNodeMaterialBase* CurrentMaterialNode = Cast<UCustomizableObjectNodeMaterialBase>(CurrentElement))
		{
			Result.Add(CurrentMaterialNode);
		}
		else if (UCustomizableObjectNodeMaterialVariation* CurrentMaterialVariationNode = Cast<UCustomizableObjectNodeMaterialVariation>(CurrentElement))
		{
			// Case of material variation. It's not a material, but a node that further references any material, add all its inputs that could be a material
			for (int numMaterialPin = 0; numMaterialPin < CurrentMaterialVariationNode->GetNumVariations(); ++numMaterialPin)
			{
				const UEdGraphPin* VariationPin = CurrentMaterialVariationNode->VariationPin(numMaterialPin);
				for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*VariationPin))
				{
					PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
				}
			}

			const UEdGraphPin* DefaultPin = CurrentMaterialVariationNode->DefaultPin();
			for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*DefaultPin))
			{
				PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
			}
		} 
	}

	return Result;
}


void UCustomizableObjectNodeObject::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	// Fix up ComponentSettings. Only root nodes
	if (ComponentSettings.IsEmpty() && bIsBase && !ParentObject)
	{
		FComponentSettings ComponentSettingsTemplate;
		ComponentSettingsTemplate.LODReductionSettings.SetNum(NumLODs);

		if (UCustomizableObject* CurrentObject = Cast<UCustomizableObject>(GetOutermostObject()))
		{
			ComponentSettings.Init(ComponentSettingsTemplate, CurrentObject->GetPrivate()->MutableMeshComponents.Num());
		}
	}

	// Reconstruct in case any extension pins have changed
	ReconstructNode();
}

void UCustomizableObjectNodeObject::PostPasteNode()
{
	Super::PostPasteNode();

	Identifier = FGuid::NewGuid();
}

void UCustomizableObjectNodeObject::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	Identifier = FGuid::NewGuid();
}


void UCustomizableObjectNodeObject::SetParentObject(UCustomizableObject* CustomizableParentObject)
{
	if (CustomizableParentObject != GetGraphEditor()->GetCustomizableObject())
	{
		ParentObject = CustomizableParentObject;

		// Update the cached flag in the main object
		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(GetCustomizableObjectGraph()->GetOuter());
		if (CustomizableObject)
		{
			CustomizableObject->GetPrivate()->SetIsChildObject(ParentObject != nullptr);

			TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

			if (Editor.IsValid())
			{
				Editor->UpdateObjectProperties();
			}
		}
	}
}


FText UCustomizableObjectNodeObject::GetTooltipText() const
{
	return LOCTEXT("Base_Object_Tooltip",
	"As root object: Defines a customizable object root, its basic properties and its relationship with descendant Customizable Objects.\n\nAs a child object: Defines a Customizable Object children outside of the parent asset, to ease organization of medium and large\nCustomizable Objects. (Functionally equivalent to the Child Object Node.)");
}


bool UCustomizableObjectNodeObject::IsSingleOutputNode() const
{
	return true;
}

bool UCustomizableObjectNodeObject::IsBuiltInPin(FName PinName)
{
	return PinName == ChildrenPinName
		|| PinName == ComponentsPinName
		|| PinName == OutputPinName
		|| PinName.ToString().StartsWith(LODPinNamePrefix);
}

#undef LOCTEXT_NAMESPACE
