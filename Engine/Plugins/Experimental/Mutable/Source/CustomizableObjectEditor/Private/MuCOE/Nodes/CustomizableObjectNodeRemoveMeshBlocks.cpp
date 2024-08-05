// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocks.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeRemoveMeshBlocks::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();
	
	int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	// Convert deprecated node index list to the node id list.
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::PostLoadToCustomVersion
		&& BlockIds_DEPRECATED.Num() < Blocks_DEPRECATED.Num())
	{
		if (UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode())
		{
			TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

			if (!Layouts.IsValidIndex(ParentLayoutIndex))
			{
				UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
					*GetOutermost()->GetName(), ParentLayoutIndex, Layouts.Num());
			}
			else if (UCustomizableObjectNodeMaterial* ParentMaterial = Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
			{
				UCustomizableObjectLayout* ParentLayout = Layouts[ParentLayoutIndex];

				for (int IndexIndex = BlockIds_DEPRECATED.Num(); IndexIndex < Blocks_DEPRECATED.Num(); ++IndexIndex)
				{
					const int BlockIndex = Blocks_DEPRECATED[IndexIndex];
					if (!ParentLayout->Blocks.IsValidIndex(BlockIndex))
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid layout block index %d. Parent node has %d blocks."),
							*GetOutermost()->GetName(), BlockIndex, ParentLayout->Blocks.Num());

						continue;
					}
					
					const FGuid Id = ParentLayout->Blocks[BlockIndex].Id;
					if (!Id.IsValid())
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an valid layout block %d but that block doesn't have an id."),
							*GetOutermost()->GetName(), BlockIndex);

						continue;
					}

					BlockIds_DEPRECATED.Add(Id);
				}
			}
		}
	}

	// Convert deprecated node id list to absolute rect list.
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::UseUVRects)
	{
		// If we are here, it means this node was loaded from a version that didn't have it's own layout.
		check(Layout->Blocks.IsEmpty());

		UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode();

		TArray<UCustomizableObjectLayout*> ParentLayouts = ParentMaterialNode->GetLayouts();

		if (!ParentLayouts.IsValidIndex(ParentLayoutIndex))
		{
			UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
				*GetOutermost()->GetName(), ParentLayoutIndex, ParentLayouts.Num());
		}
		else
		{
			UCustomizableObjectLayout* ParentLayout = ParentLayouts[ParentLayoutIndex];
			FIntPoint GridSize = ParentLayout->GetGridSize();

			Layout->SetGridSize(GridSize);

			if (Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
			{
				for (const FGuid& BlockId : BlockIds_DEPRECATED)
				{
					for (const FCustomizableObjectLayoutBlock& ParentBlock : ParentLayout->Blocks)
					{
						if (ParentBlock.Id == BlockId)
						{
							FCustomizableObjectLayoutBlock NewBlock;
							NewBlock = ParentBlock;

							// Clear some unnecessary data.
							NewBlock.bReduceBothAxes = false;
							NewBlock.bReduceByTwo = false;
							NewBlock.Priority = 0;

							Layout->Blocks.Add(NewBlock);
						}
					}
				}

				if (Layout->Blocks.Num() != BlockIds_DEPRECATED.Num())
				{
					UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to %d invalid layout block. It has been ignored during version upgrade."),
						*GetOutermost()->GetName(), int32(BlockIds_DEPRECATED.Num() - Layout->Blocks.Num()));
				}
			}
		}
	}
}


void UCustomizableObjectNodeRemoveMeshBlocks::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNodeRemoveMeshBlocks::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged 
		&& (PropertyThatChanged->GetName() == TEXT("ParentMaterialObject") || PropertyThatChanged->GetName() == TEXT("ParentLayoutIndex")))
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCustomizableObjectNodeRemoveMeshBlocks::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Material"));
}


FText UCustomizableObjectNodeRemoveMeshBlocks::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Remove_Mesh_Blocks", "Remove Mesh Blocks");
}


FLinearColor UCustomizableObjectNodeRemoveMeshBlocks::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


void UCustomizableObjectNodeRemoveMeshBlocks::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin == OutputPin())
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}
}

bool UCustomizableObjectNodeRemoveMeshBlocks::IsNodeOutDatedAndNeedsRefresh()
{
	bool Result = false;

	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode();
	if (ParentMaterialNode)
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (!Layouts.IsValidIndex(ParentLayoutIndex))
		{
			Result = true;
		}
	}

	// Remove previous compilation warnings
	if (!Result && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

	return Result;
}

FString UCustomizableObjectNodeRemoveMeshBlocks::GetRefreshMessage() const
{
	return "Source Layout has changed, layout blocks might have changed. Please Refresh Node to reflect those changes.";
}


FText UCustomizableObjectNodeRemoveMeshBlocks::GetTooltipText() const
{
	return LOCTEXT("Remove_Mesh_Blocks_Tooltip", "Remove all the geometry in the chosen layout blocks from a material.");
}


bool UCustomizableObjectNodeRemoveMeshBlocks::IsSingleOutputNode() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
