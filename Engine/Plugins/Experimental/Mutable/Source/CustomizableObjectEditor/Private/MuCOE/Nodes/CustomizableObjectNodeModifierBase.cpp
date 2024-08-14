// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"


FLinearColor UCustomizableObjectNodeModifierBase::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Modifier);
}


void UCustomizableObjectNodeModifierBase::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// Remove "Material" pin and add "Modifier"
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddModifierPin)
	{
		// Replace the output pin
		UEdGraphPin* OldPin = FindPin(TEXT("Material"));
		UEdGraphPin* NewPin = CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName("Modifier"));
		if (OldPin && NewPin)
		{
			FGuid PinId = NewPin->PinId;
			NewPin->CopyPersistentDataFromOldPin(*OldPin);
			NewPin->PinId = PinId;
			NewPin->bHidden = OldPin->bHidden;

			CustomRemovePin(*OldPin);

			// Reconnect it to the correct output of the target node
			TArray<UEdGraphPin*> LinksToRemove;
			for (int32 LinkIndex = 0; LinkIndex < NewPin->LinkedTo.Num(); ++LinkIndex)
			{
				UEdGraphPin* LinkedToPin = NewPin->LinkedTo[LinkIndex];

				if (!LinkedToPin)
				{
					continue;
				}

				UEdGraphNode* ToNode = LinkedToPin->GetOwningNode();
				if (!ToNode)
				{
					continue;
				}

				if (UCustomizableObjectNodeObject* ToObjectNode = Cast<UCustomizableObjectNodeObject>(ToNode))
				{
					UEdGraphPin* ToModifierPin = ToObjectNode->ModifiersPin();
					if (ToModifierPin == LinkedToPin)
					{
						// It is already correctly connected
						continue;
					}

					if (ToModifierPin)
					{
						LinksToRemove.Add(LinkedToPin);
						NewPin->MakeLinkTo(ToModifierPin);
					}
				}
				else
				{
					// The modifier is connected to a node for which automatic upgrade support is not implemented. This will be warned below
				}
			}

			// Remove reconnected links
			for (UEdGraphPin* LinkToRemove : LinksToRemove)
			{
				NewPin->BreakLinkTo(LinkToRemove);
			}
		}
	}

	// Check for old legacy connections that need manual update
	{
		UEdGraphPin* NewPin = FindPin(TEXT("Modifier"));
		if (NewPin)
		{
			for (int32 LinkIndex = 0; LinkIndex < NewPin->LinkedTo.Num(); ++LinkIndex)
			{
				UEdGraphPin* LinkedToPin = NewPin->LinkedTo[LinkIndex];

				if (!LinkedToPin)
				{
					continue;
				}

				UEdGraphNode* ToNode = LinkedToPin->GetOwningNode();
				if (!ToNode)
				{
					continue;
				}

				if (LinkedToPin->PinType.PinCategory != Schema->PC_Modifier)
				{
					// The modifier is connected to a node for which automatic upgrade support is not implemented
					FString Msg = FString::Printf(TEXT("A modifier node has a legacy connection to a node [%s] without automatic upgrade support. Manual update is probably needed."), *ToNode->GetName());
					FCustomizableObjectEditorLogger::CreateLog(FText::FromString(Msg))
						.Severity(EMessageSeverity::Warning)
						.Context(*this)
						.BaseObject(true)
						.Log();

				}
			}
		}
	}

}

