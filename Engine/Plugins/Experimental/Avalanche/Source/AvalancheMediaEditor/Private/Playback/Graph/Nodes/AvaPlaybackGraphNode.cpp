// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode.h"
#include "AvalancheMediaEditorSettings.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphEditorActions.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/AvaPlaybackCommands.h"
#include "Playback/Graph/AvaPlaybackGraph.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_Root.h"
#include "Playback/Graph/Nodes/Slate/SAvaPlaybackGraphNode.h"
#include "Playback/Nodes/AvaPlaybackNode.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackGraphNode"

UAvalanchePlayback* UAvaPlaybackGraphNode::GetPlayback() const
{
	return CastChecked<UAvaPlaybackGraph>(GetGraph())->GetPlayback();
}

void UAvaPlaybackGraphNode::SetPlaybackNode(UAvaPlaybackNode* InPlaybackNode)
{
	PlaybackNode = InPlaybackNode;
	PlaybackNode->SetGraphNode(this);
}

double UAvaPlaybackGraphNode::GetLastTimeTicked()
{
	return PlaybackNode ? PlaybackNode->GetLastTimeTicked() : -1.0;
}

double UAvaPlaybackGraphNode::GetChildLastTimeTicked(int32 ChildIndex) const
{
	return PlaybackNode ? PlaybackNode->GetChildLastTimeTicked(ChildIndex) : -1.0;
}

bool UAvaPlaybackGraphNode::CanAddInputPin() const
{
	if (PlaybackNode)
	{
		// Check if adding another input would exceed max child nodes.
		return PlaybackNode->GetChildNodes().Num() < PlaybackNode->GetMaxChildNodes();
	}
	else
	{
		return false;
	}
}

void UAvaPlaybackGraphNode::AddInputPin()
{
	const FScopedTransaction Transaction(LOCTEXT("PlaybackGraphNodeAddInput", "Add Playback Input"));
	Modify();
	CreateInputPin();

	UAvalanchePlayback* const Playback = GetPlayback();
	Playback->CompilePlaybackNodesFromGraphNodes();
	Playback->MarkPackageDirty();

	// Refresh the current graph, so the pins can be updated
	GetGraph()->NotifyGraphChanged();
}

void UAvaPlaybackGraphNode::CreateInputPin()
{
	const int32 InputIndex = GetInputPinCount();
	
	UEdGraphPin* const Pin = CreatePin(EGPD_Input
		, GetInputPinCategory(InputIndex)
		, GetInputPinSubCategory(InputIndex)
		, GetInputPinSubCategoryObject(InputIndex)
		, GetInputPinName(InputIndex));

	check(Pin);
	
	if (Pin->PinName.IsNone())
	{
		// Makes sure pin has a name for lookup purposes but user will never see it
		Pin->PinName = CreateUniquePinName(TEXT("Input"));
		Pin->PinFriendlyName = FText::FromString(TEXT(" "));
	}
}

void UAvaPlaybackGraphNode::RemoveInputPin(UEdGraphPin* InGraphPin)
{
	const FScopedTransaction Transaction(LOCTEXT("PlaybackGraphNodeRemoveInput", "Remove Playback Input") );
	Modify();
	
	const TArray<UEdGraphPin*> InputPins = GetInputPins();
	
	for (int32 InputIndex = 0; InputIndex < InputPins.Num(); InputIndex++)
	{
		if (InGraphPin == InputPins[InputIndex])
		{
			InGraphPin->MarkAsGarbage();
			Pins.Remove(InGraphPin);
			
			// also remove the PlaybackNode child node so ordering matches
			PlaybackNode->Modify();
			PlaybackNode->RemoveChildNode(InputIndex);
			break;
		}
	}

	UAvalanchePlayback* const Playback = GetPlayback();
	Playback->CompilePlaybackNodesFromGraphNodes();
	Playback->MarkPackageDirty();

	// Refresh the current graph, so the pins can be updated
	GetGraph()->NotifyGraphChanged();
}

void UAvaPlaybackGraphNode::PrepareForCopying()
{
	if (PlaybackNode)
	{
		// Temporarily take ownership of the PlaybackNode, so that it is not deleted when cutting
		PlaybackNode->Rename(nullptr, this, REN_DontCreateRedirectors);
	}
}

void UAvaPlaybackGraphNode::PostPasteNode()
{
	if (PlaybackNode)
	{
		PlaybackNode->PostAllocateNode();
	}
}

void UAvaPlaybackGraphNode::PostCopyNode()
{
	ResetPlaybackNodeOwner();
}

void UAvaPlaybackGraphNode::ResetPlaybackNodeOwner()
{
	if (PlaybackNode)
	{
		UAvalanchePlayback* const Playback = GetPlayback();
		if (PlaybackNode->GetOuter() != Playback)
		{
			// Ensures PlaybackNode is owned by the Playback Object
			PlaybackNode->Rename(nullptr, Playback, REN_DontCreateRedirectors);
		}

		// Set up the back pointer for newly created sound nodes
		PlaybackNode->SetGraphNode(this);
	}
}

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNode::StaticClass();
}

void UAvaPlaybackGraphNode::CreateInputPins()
{
	if (PlaybackNode)
	{
		for (int32 ChildIndex = 0; ChildIndex < PlaybackNode->GetChildNodes().Num(); ++ChildIndex)
		{
			CreateInputPin();
		}
	}
}

void UAvaPlaybackGraphNode::CreateOutputPin()
{
	if (!IsRootNode())
	{
		UEdGraphPin* const Pin = CreatePin(EGPD_Output
			, GetOutputPinCategory()
			, NAME_None);
		
		if (Pin->PinName.IsNone())
		{
			// Makes sure pin has a name for lookup purposes but user will never see it
			Pin->PinName = CreateUniquePinName(TEXT("Output"));
			Pin->PinFriendlyName = FText::FromString(TEXT(" "));;
		}
	}
}

bool UAvaPlaybackGraphNode::IsRootNode() const
{
	return !!Cast<UAvaPlaybackGraphNode_Root>(this);
}

UEdGraphPin* UAvaPlaybackGraphNode::GetOutputPin() const
{
	for (UEdGraphPin* const Pin : Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			return Pin;
		}
	}
	return nullptr;
}

TArray<UEdGraphPin*> UAvaPlaybackGraphNode::GetInputPins() const
{
	TArray<UEdGraphPin*> InputPins;
	
	//Reserve with the Expected Number of Inputs (since we assume only 1 output pin, reserve PinNum - 1)
	InputPins.Reserve(FMath::Max(0, Pins.Num() - 1));
	
	for (UEdGraphPin* const Pin : Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			InputPins.Add(Pin);
		}
	}

	return InputPins;
}

UEdGraphPin* UAvaPlaybackGraphNode::GetInputPin(int32 InputIndex)  const
{
	TArray<UEdGraphPin*> InputPins = GetInputPins();
	
	if (InputPins.IsValidIndex(InputIndex))
	{
		return InputPins[InputIndex];
	}
	
	checkf(0, TEXT("PlaybackGraphNode::GetInputPin. Index out of Bounds"));
	return nullptr;
}

int32 UAvaPlaybackGraphNode::GetInputPinIndex(UEdGraphPin* InInputPin) const
{
	if (InInputPin && InInputPin->Direction == EGPD_Input)
	{
		int32 Index = 0;
		for (const UEdGraphPin* const Pin : Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				if (Pin == InInputPin)
				{
					return Index;
				}
				++Index;
			}
		}
	}
	return INDEX_NONE;
}

int32 UAvaPlaybackGraphNode::GetInputPinCount() const
{
	return GetInputPins().Num();
}

void UAvaPlaybackGraphNode::InsertNewNode(UEdGraphPin* FromPin, UEdGraphPin* NewLinkPin, TSet<UEdGraphNode*>& OutNodeList)
{
	const UAvaPlaybackGraphSchema* const Schema = CastChecked<UAvaPlaybackGraphSchema>(GetSchema());

	// The pin we are creating from already has a connection that needs to be broken. We want to "insert" the new node in between, so that the output of the new node is hooked up too
	UEdGraphPin* OldLinkedPin = FromPin->LinkedTo[0];
	check(OldLinkedPin);

	FromPin->BreakAllPinLinks();

	// Hook up the old linked pin to the first valid output pin on the new node
	for (int32 OutputPinIndex=0; OutputPinIndex < Pins.Num(); OutputPinIndex++)
	{
		UEdGraphPin* OutputPin = Pins[OutputPinIndex];
		check(OutputPin);
		
		if (Schema->CanCreateConnection(OldLinkedPin, OutputPin).Response == ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE)
		{
			if (Schema->TryCreateConnection(OldLinkedPin, OutputPin))
			{
				OutNodeList.Add(OldLinkedPin->GetOwningNode());
				OutNodeList.Add(this);
			}
			break;
		}
	}

	if (Schema->TryCreateConnection(FromPin, NewLinkPin))
	{
		OutNodeList.Add(FromPin->GetOwningNode());
		OutNodeList.Add(this);
	}
}

void UAvaPlaybackGraphNode::PostLoad()
{
	Super::PostLoad();

	// Fixup any Playback Node back pointers that may be out of date
	if (PlaybackNode)
	{
		PlaybackNode->SetGraphNode(this);
	}

	for (UEdGraphPin* const Pin : Pins)
	{
		if (Pin && Pin->PinName.IsNone())
		{
			// Makes sure pin has a name for lookup purposes but user will never see it
			if (Pin->Direction == EGPD_Input)
			{
				Pin->PinName = CreateUniquePinName(TEXT("Input"));
			}
			else
			{
				Pin->PinName = CreateUniquePinName(TEXT("Output"));
			}
			Pin->PinFriendlyName = FText::FromString(TEXT(" "));
		}
	}
}

void UAvaPlaybackGraphNode::PostEditImport()
{
	Super::PostEditImport();
	ResetPlaybackNodeOwner();
}

void UAvaPlaybackGraphNode::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE)
	{
		CreateNewGuid();
	}
}

FText UAvaPlaybackGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (PlaybackNode)
	{
		return PlaybackNode->GetNodeDisplayNameText();
	}
	return Super::GetNodeTitle(TitleType);
}

FText UAvaPlaybackGraphNode::GetTooltipText() const
{
	if (PlaybackNode)
	{
		return PlaybackNode->GetNodeTooltipText();
	}
	return Super::GetTooltipText();
}

void UAvaPlaybackGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context->Pin)
	{
		// If on an input that can be deleted, show option
		if (Context->Pin->Direction == EGPD_Input && PlaybackNode->GetChildNodes().Num() > PlaybackNode->GetMinChildNodes())
		{
			FToolMenuSection& Section = Menu->AddSection("PlaybackGraphRemovePin"
				, LOCTEXT("PlaybackGraphRemovePin", "Input Pin Actions"));
			Section.AddMenuEntry(FAvaPlaybackCommands::Get().AddInputPin);
			Section.AddMenuEntry(FAvaPlaybackCommands::Get().RemoveInputPin);
		}
	}
	else if (Context->Node)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("PlaybackGraphNodeEdit");
			Section.AddMenuEntry(FGenericCommands::Get().Delete);
			Section.AddMenuEntry(FGenericCommands::Get().Cut);
			Section.AddMenuEntry(FGenericCommands::Get().Copy);
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
			Section.AddSeparator(NAME_None);
		}
		
		{
			FToolMenuSection& Section = Menu->AddSection("PlaybackGraphNodeOrganization", LOCTEXT("PlaybackGraphNodeOrganization", "Organization"));
			
			Section.AddSubMenu("Alignment"
				, LOCTEXT("AlignmentHeader", "Alignment")
				, FText()
				, FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
				{
					{
						FToolMenuSection& SubMenuSection = SubMenu->AddSection("PlaybackGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
					}
	
					{
						FToolMenuSection& SubMenuSection = SubMenu->AddSection("PlaybackGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
						SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
					}
				}));
			
			Section.AddSeparator(NAME_None);
		}
	}
}

void UAvaPlaybackGraphNode::AllocateDefaultPins()
{
	check(Pins.Num() == 0);
	CreateInputPins();
	CreateOutputPin();
}

void UAvaPlaybackGraphNode::ReconstructNode()
{
	// Break any links to 'orphan' pins
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		TArray<class UEdGraphPin*>& LinkedToRef = Pin->LinkedTo;
		for (int32 LinkIdx=0; LinkIdx < LinkedToRef.Num(); LinkIdx++)
		{
			UEdGraphPin* OtherPin = LinkedToRef[LinkIdx];
			// If we are linked to a pin that its owner doesn't know about, break that link
			if (!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
			{
				Pin->LinkedTo.Remove(OtherPin);
			}
		}
	}

	// Store the old Input and Output pins
	TArray<UEdGraphPin*> OldInputPins = GetInputPins();
	UEdGraphPin* OldOutputPin = GetOutputPin();
	
	// Move the existing pins to a saved array
	TArray OldPins(Pins);
	Pins.Reset();

	// Recreate the new pins
	AllocateDefaultPins();

	// Get new Input and Output pins
	TArray<UEdGraphPin*> NewInputPins = GetInputPins();
	UEdGraphPin* NewOutputPin = GetOutputPin();

	for (int32 PinIndex = 0; PinIndex < OldInputPins.Num(); ++PinIndex)
	{
		if (PinIndex < NewInputPins.Num())
		{
			NewInputPins[PinIndex]->MovePersistentDataFromOldPin(*OldInputPins[PinIndex]);
		}
	}
	
	if (OldOutputPin)
	{
		NewOutputPin->MovePersistentDataFromOldPin(*OldOutputPin);
	}

	// Throw away the original pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPin->Modify();
		UEdGraphNode::DestroyPin(OldPin);
	}
}

void UAvaPlaybackGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin)
	{
		const UAvaPlaybackGraphSchema* const Schema = CastChecked<UAvaPlaybackGraphSchema>(GetSchema());
		TSet<UEdGraphNode*> NodeList;

		// auto-connect from dragged pin to first compatible pin on the new node
		for (int32 Index = 0; Index < Pins.Num(); ++Index)
		{
			UEdGraphPin* Pin = Pins[Index];
			check(Pin);
			
			FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, Pin);
			
			if (Response.Response == ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE)
			{
				if (Schema->TryCreateConnection(FromPin, Pin))
				{
					NodeList.Add(FromPin->GetOwningNode());
					NodeList.Add(this);
				}
				break;
			}
			
			if (Response.Response == ECanCreateConnectionResponse::CONNECT_RESPONSE_BREAK_OTHERS_A)
			{
				InsertNewNode(FromPin, Pin, NodeList);
				break;
			}
		}

		// Send all nodes that received a new pin connection a notification
		for (TSet<UEdGraphNode*>::TConstIterator It = NodeList.CreateConstIterator(); It; ++It)
		{
			UEdGraphNode* Node = *It;
			Node->NodeConnectionListChanged();
		}
	}
}

bool UAvaPlaybackGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UAvaPlaybackGraphSchema::StaticClass());
}

FLinearColor UAvaPlaybackGraphNode::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackDefaultNodeColor;
}

TSharedPtr<SGraphNode> UAvaPlaybackGraphNode::CreateVisualWidget()
{
	return SNew(SAvaPlaybackGraphNode, this);
}

FName UAvaPlaybackGraphNode::GetInputPinName(int32 InputPinIndex) const
{
	if (PlaybackNode)
	{
		return PlaybackNode->GetInputPinName(InputPinIndex);
	}
	return NAME_None;
}

FName UAvaPlaybackGraphNode::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackGraphSchema::PC_ChannelFeed;
}

FName UAvaPlaybackGraphNode::GetInputPinSubCategory(int32 InputPinIndex) const
{
	return NAME_None;
}

UObject* UAvaPlaybackGraphNode::GetInputPinSubCategoryObject(int32 InputPinIndex) const
{
	return nullptr;
}

FName UAvaPlaybackGraphNode::GetOutputPinCategory() const
{
	return UAvaPlaybackGraphSchema::PC_ChannelFeed;
}

#undef LOCTEXT_NAMESPACE
