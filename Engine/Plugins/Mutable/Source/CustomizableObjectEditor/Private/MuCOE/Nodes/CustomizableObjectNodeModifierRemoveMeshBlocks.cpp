// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMeshBlocks.h"

#include "MuCOE/Nodes/CustomizableObjectNodeModifierExtendMeshSection.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeModifierRemoveMeshBlocks::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);
	
	// Convert deprecated node index list to the node id list.
	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::PostLoadToCustomVersion
		&& BlockIds_DEPRECATED.Num() < Blocks_DEPRECATED.Num())
	{
		UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetCustomizableObjectExternalNode<UCustomizableObjectNodeMaterialBase>(ParentMaterialObject_DEPRECATED.Get(), ParentMaterialNodeId_DEPRECATED);
		if (ParentMaterialNode)
		{
			TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

			if (!Layouts.IsValidIndex(ParentLayoutIndex))
			{
				UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
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
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks refers to an invalid layout block index %d. Parent node has %d blocks."),
							*GetOutermost()->GetName(), BlockIndex, ParentLayout->Blocks.Num());

						continue;
					}
					
					const FGuid Id = ParentLayout->Blocks[BlockIndex].Id;
					if (!Id.IsValid())
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks refers to an valid layout block %d but that block doesn't have an id."),
							*GetOutermost()->GetName(), BlockIndex);

						continue;
					}

					BlockIds_DEPRECATED.Add(Id);
				}
			}
		}
		else
		{
			UE_LOG(LogMutable, Log, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks has no parent. It will not be upgraded."), *GetOutermost()->GetName());
		}
	}

	// Convert deprecated node id list to absolute rect list.
	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::UseUVRects)
	{
		// If we are here, it means this node was loaded from a version that didn't have it's own layout.
		check(Layout->Blocks.IsEmpty());

		UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetCustomizableObjectExternalNode<UCustomizableObjectNodeMaterialBase>(ParentMaterialObject_DEPRECATED.Get(), ParentMaterialNodeId_DEPRECATED);

		TArray<UCustomizableObjectLayout*> ParentLayouts;

		if (!ParentMaterialNode)
		{
			// Conversion failed?
			ensure(false);
			UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks version upgrade failed."), *GetOutermost()->GetName());
		}
		else
		{
			ParentLayouts = ParentMaterialNode->GetLayouts();
		}

		if (!ParentLayouts.IsValidIndex(ParentLayoutIndex))
		{
			UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
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
					bool bSkipBlock = false;
					for (const FCustomizableObjectLayoutBlock& ExistingBlock : Layout->Blocks)
					{
						if (ExistingBlock.Id == BlockId)
						{
							bSkipBlock = true;
							UE_LOG(LogMutable, Log, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks has a duplicated layout block id. One has been ignored during version upgrade."), *GetOutermost()->GetName());
							break;
						}
					}

					if (bSkipBlock)
					{
						continue;
					}

					bool bFoundInParent = false;
					for (const FCustomizableObjectLayoutBlock& ParentBlock : ParentLayout->Blocks)
					{
						if (ParentBlock.Id == BlockId)
						{
							bFoundInParent = true;

							FCustomizableObjectLayoutBlock NewBlock;
							NewBlock = ParentBlock;

							// Clear some unnecessary data.
							NewBlock.bReduceBothAxes = false;
							NewBlock.bReduceByTwo = false;
							NewBlock.Priority = 0;

							Layout->Blocks.Add(NewBlock);
							break;
						}
					}

					if (!bFoundInParent)
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeModifierRemoveMeshBlocks refers to and invalid layout block. It has been ignored during version upgrade."), *GetOutermost()->GetName());
					}
				}
			}
		}
	}
}


void UCustomizableObjectNodeModifierRemoveMeshBlocks::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName("Modifier"));
}


FText UCustomizableObjectNodeModifierRemoveMeshBlocks::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Remove_Mesh_Blocks", "Remove Mesh Blocks");
}


FText UCustomizableObjectNodeModifierRemoveMeshBlocks::GetTooltipText() const
{
	return LOCTEXT("Remove_Mesh_Blocks_Tooltip", "Remove all the geometry in the chosen layout blocks from a material.");
}


bool UCustomizableObjectNodeModifierRemoveMeshBlocks::IsSingleOutputNode() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
