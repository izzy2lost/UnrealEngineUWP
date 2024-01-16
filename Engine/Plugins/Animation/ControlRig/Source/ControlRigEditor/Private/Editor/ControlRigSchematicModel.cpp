// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSchematicModel.h"

#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ControlRigEditor.h"
#include "ControlRigEditorStyle.h"
#include "ModularRig.h"
#include "ModularRigRuleManager.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Rigs/RigHierarchyController.h"
#include "Async/TaskGraphInterfaces.h"
#include <SchematicGraphPanel/SchematicGraphStyle.h>

#define LOCTEXT_NAMESPACE "ControlRigSchematicModel"

FString FControlRigSchematicRigElementKeyNode::GetDragDropDecoratorLabel() const
{
	return Key.ToString();
}

bool FControlRigSchematicRigElementKeyNode::IsDragSupported() const
{
	return Key.Type == ERigElementType::Connector;
}

FControlRigSchematicModel::~FControlRigSchematicModel()
{
	if(ControlRigBeingDebuggedPtr.IsValid())
	{
		if(UControlRig* ControlRigBeingDebugged = ControlRigBeingDebuggedPtr.Get())
		{
			if(!ControlRigBeingDebugged->HasAnyFlags(RF_BeginDestroyed))
			{
				ControlRigBeingDebugged->GetHierarchy()->OnModified().RemoveAll(this);
			}
		}
	}

	ControlRigBeingDebuggedPtr.Reset();
	ControlRigEditor.Reset();
	ControlRigBlueprint.Reset();
}

void FControlRigSchematicModel::SetEditor(const TSharedRef<FControlRigEditor>& InEditor)
{
	ControlRigEditor = InEditor;
	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();
	ControlRigBeingDebuggedPtr = ControlRigBlueprint->GetDebuggedControlRig();
}

void FControlRigSchematicModel::Reset()
{
	Super::Reset();
	RigElementKeyToGuid.Reset();
}

FControlRigSchematicRigElementKeyNode* FControlRigSchematicModel::AddElementKeyNode(const FRigElementKey& InKey, bool bNotify)
{
	FControlRigSchematicRigElementKeyNode* Node = AddNode<FControlRigSchematicRigElementKeyNode>(false);
	if(Node)
	{
		Node->Key = InKey;

		Node->SetLabel(FText::FromString(Node->GetKey().ToString())); 
		switch(Node->GetKey().Type)
		{
			case ERigElementType::Socket:
			{
				if (ControlRigBeingDebuggedPtr.IsValid())
				{
					if (const URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
					{
						if(const FRigSocketElement* Socket = Hierarchy->Find<FRigSocketElement>(Node->GetKey()))
						{
							const FString Description = Socket->GetDescription(Hierarchy);
							if(!Description.IsEmpty())
							{
								Node->SetLabel(
									FText::Format(LOCTEXT("NodeLabelFormat", "{0}\n{1}"), 
									Node->GetLabel(), 
									FText::FromString(Description))
								);
							}
						}
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}

		RigElementKeyToGuid.Add(Node->GetKey(), Node->GetGuid());

		FRigElementKey ResolvedKey;
		if(IsConnectorResolved(InKey, &ResolvedKey))
		{
			if(const FControlRigSchematicRigElementKeyNode* ResolvedNode = FindElementKeyNode(ResolvedKey))
			{
				SetParentNode(Node->GetGuid(), ResolvedNode->GetGuid());
			}
		}

		if (bNotify && OnNodeAddedDelegate.IsBound())
		{
			OnNodeAddedDelegate.Broadcast(Node);
		}

		UpdateElementKeyLinks();
	}
	return Node;
}

const FControlRigSchematicRigElementKeyNode* FControlRigSchematicModel::FindElementKeyNode(const FRigElementKey& InKey) const
{
	if(const FGuid* FoundGuid = RigElementKeyToGuid.Find(InKey))
	{
		return FindNode< FControlRigSchematicRigElementKeyNode >(*FoundGuid);
	}
	return nullptr;
}

bool FControlRigSchematicModel::ContainsElementKeyNode(const FRigElementKey& InKey) const
{
	return FindElementKeyNode(InKey) != nullptr;
}

bool FControlRigSchematicModel::RemoveNode(const FGuid& InGuid)
{
	FRigElementKey KeyToRemove;
	if(const FControlRigSchematicRigElementKeyNode* Node = FindNode<FControlRigSchematicRigElementKeyNode>(InGuid))
	{
		KeyToRemove = Node->GetKey();
	}
	if(Super::RemoveNode(InGuid))
	{
		RigElementKeyToGuid.Remove(KeyToRemove);
		return true;
	}
	return false;
}

bool FControlRigSchematicModel::RemoveElementKeyNode(const FRigElementKey& InKey)
{
	if(const FGuid* GuidPtr = RigElementKeyToGuid.Find(InKey))
	{
		const FGuid Guid = *GuidPtr;
		const bool bResult = RemoveNode(Guid);
		if(bResult)
		{
			UpdateElementKeyLinks();
		}
		return bResult;
	}
	return false;
}

FControlRigSchematicRigElementKeyLink* FControlRigSchematicModel::AddElementKeyLink(const FRigElementKey& InSourceKey, const FRigElementKey& InTargetKey, bool bNotify)
{
	check(InSourceKey.IsValid());
	check(InTargetKey.IsValid());
	check(InSourceKey != InTargetKey);
	const FControlRigSchematicRigElementKeyNode* SourceNode = FindElementKeyNode(InSourceKey);
	const FControlRigSchematicRigElementKeyNode* TargetNode = FindElementKeyNode(InTargetKey);
	if(SourceNode && TargetNode)
	{
		FControlRigSchematicRigElementKeyLink* Link = AddLink<FControlRigSchematicRigElementKeyLink>(SourceNode->GetGuid(), TargetNode->GetGuid());
		Link->SourceKey = InSourceKey;
		Link->TargetKey = InTargetKey;
		Link->Thickness = 12.f;
		return Link;
	}
	return nullptr;
}

void FControlRigSchematicModel::UpdateElementKeyLinks()
{
	const TSharedPtr<FControlRigEditor> Editor = ControlRigEditor.Pin();
	if(!Editor.IsValid())
	{
		return;
	}

	typedef TTuple< FRigElementKey, FRigElementKey > TElementKeyPair;
	typedef TTuple< FGuid, TElementKeyPair > TElementKeyLinkPair;

	struct FLinkTraverser
	{
		const FControlRigSchematicRigElementKeyNode* VisitElement(
			const FRigBaseElement* InElement, 
			const FControlRigSchematicRigElementKeyNode* InNode, 
			TArray< TElementKeyPair >& OutExpectedLinks) const
		{
			const FControlRigSchematicRigElementKeyNode* Node = InNode;
			if(const FControlRigSchematicRigElementKeyNode* SelfNode = FindNode(InElement->GetKey()))
			{
				Node = SelfNode;
			}

			const TConstArrayView<FRigBaseElement*> Children = Hierarchy->GetChildren(InElement);
			for(const FRigBaseElement* Child : Children)
			{
				const FControlRigSchematicRigElementKeyNode* ChildNode = VisitElement(Child, Node, OutExpectedLinks);
				if(Node && ChildNode && Node != ChildNode)
				{
					const FRigElementKey ParentA = Hierarchy->GetFirstParent(Node->GetKey());
					const FRigElementKey ParentB = Hierarchy->GetFirstParent(ChildNode->GetKey());
					if(!ParentA || !ParentB.IsValid() || ParentA != ParentB)
					{
						OutExpectedLinks.Emplace(Node->GetKey(), ChildNode->GetKey());
					}
				}
			}

			return Node;
		}

		const FControlRigSchematicRigElementKeyNode* FindNode(const FRigElementKey& InKey) const
		{
			if(const FGuid* ElementGuid = RigElementKeyToGuid->Find(InKey))
			{
				return Cast<FControlRigSchematicRigElementKeyNode>(NodeByGuid->FindChecked(*ElementGuid).Get());
			}
			if(const FRigElementKey* SocketKey = SocketToParent.Find(InKey))
			{
				if(const FGuid* ElementGuid = RigElementKeyToGuid->Find(*SocketKey))
				{
					return Cast<FControlRigSchematicRigElementKeyNode>(NodeByGuid->FindChecked(*ElementGuid).Get());
				}
			}
			if(const FRigElementKey* ParentKey = ParentToSocket.Find(InKey))
			{
				if(const FGuid* ElementGuid = RigElementKeyToGuid->Find(*ParentKey))
				{
					return Cast<FControlRigSchematicRigElementKeyNode>(NodeByGuid->FindChecked(*ElementGuid).Get());
				}
			}
			return nullptr;
		}

		TArray< TElementKeyPair > ComputeExpectedLinks() const
		{
			SocketToParent.Reset();
			ParentToSocket.Reset();

			// create a map to look up sockets
			const TArray<FRigElementKey> SocketKeys = Hierarchy->GetSocketKeys();
			for(const FRigElementKey& SocketKey : SocketKeys)
			{
				const FRigElementKey ParentKey = Hierarchy->GetFirstParent(SocketKey);
				if(ParentKey.IsValid())
				{
					SocketToParent.Add(SocketKey, ParentKey);
					if(!ParentToSocket.Contains(ParentKey))
					{
						ParentToSocket.Add(ParentKey, SocketKey);
					}
				}
			}
			
			TArray< TElementKeyPair > ExpectedLinks;
			const TArray<FRigBaseElement*> RootElements = Hierarchy->GetRootElements();
			for(const FRigBaseElement* RootElement : RootElements)
			{
				VisitElement(RootElement, nullptr, ExpectedLinks);
			}
			return ExpectedLinks;
		}

		const UControlRig* ControlRig; 
		const URigHierarchy* Hierarchy; 
		const TMap<FRigElementKey, FGuid>* RigElementKeyToGuid = nullptr;
		const TMap<FGuid, TSharedPtr<FSchematicGraphNode>>* NodeByGuid = nullptr;
		mutable TMap<FRigElementKey, FRigElementKey> SocketToParent;
		mutable TMap<FRigElementKey, FRigElementKey> ParentToSocket;
	};

	FLinkTraverser Traverser;
	Traverser.ControlRig = Editor->GetControlRig();
	if(Traverser.ControlRig == nullptr)
	{
		return;
	}
	Traverser.Hierarchy = Traverser.ControlRig->GetHierarchy();
	if(Traverser.Hierarchy == nullptr)
	{
		return;
	}
	Traverser.RigElementKeyToGuid = &RigElementKeyToGuid;
	Traverser.NodeByGuid = &NodeByGuid;

	const TArray< TElementKeyPair > ExpectedLinks = Traverser.ComputeExpectedLinks();

	TArray< TElementKeyLinkPair > ExistingLinks;
	for(const TSharedPtr<FSchematicGraphLink>& Link : Links)
	{
		if(const FControlRigSchematicRigElementKeyLink* ElementKeyLink = Cast<FControlRigSchematicRigElementKeyLink>(Link.Get()))
		{
			ExistingLinks.Emplace(ElementKeyLink->GetGuid(), TElementKeyPair(ElementKeyLink->GetSourceKey(), ElementKeyLink->GetTargetKey()));
		}
	}

	// remove the obsolete links
	for(const TTuple< FGuid, TTuple< FRigElementKey, FRigElementKey > >& ExistingLink : ExistingLinks)
	{
		if(!ExpectedLinks.Contains(ExistingLink.Get<1>()))
		{
			(void)RemoveLink(ExistingLink.Get<0>());
		}
	}

	// add missing links
	for(const TElementKeyPair& ExpectedLink : ExpectedLinks)
	{
		if(!ExistingLinks.ContainsByPredicate([ExpectedLink](const TElementKeyLinkPair& ExistingLink) -> bool
		{
			return ExistingLink.Get<1>() == ExpectedLink;
		}))
		{
			(void)AddElementKeyLink(ExpectedLink.Get<0>(), ExpectedLink.Get<1>());
		}
	}

	// todo: also introduce links for module relationships
}

void FControlRigSchematicModel::OnSetObjectBeingDebugged(UObject* InObject)
{
	if(ControlRigBeingDebuggedPtr.Get() == InObject)
	{
		return;
	}

	if(ControlRigBeingDebuggedPtr.IsValid())
	{
		if(UControlRig* ControlRigBeingDebugged = ControlRigBeingDebuggedPtr.Get())
		{
			if(!ControlRigBeingDebugged->HasAnyFlags(RF_BeginDestroyed))
			{
				ControlRigBeingDebugged->GetHierarchy()->OnModified().RemoveAll(this);
			}
		}
	}

	ControlRigBeingDebuggedPtr.Reset();
	
	if(UControlRig* ControlRig = Cast<UControlRig>(InObject))
	{
		ControlRigBeingDebuggedPtr = ControlRig;
		if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->OnModified().RemoveAll(this);
			Hierarchy->OnModified().AddRaw(this, &FControlRigSchematicModel::OnHierarchyModified);
		}

		// todo: react to the rig being constructed - but not when the rig hierarchy is being copied 
		// ControlRig->OnPostConstruction_AnyThread().AddRaw(this, &FControlRigSchematicModel::OnPostConstruction);
	}
}

void FControlRigSchematicModel::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(InHierarchy->IsCopyingHierarchy())
	{
		return;
	}
	
	switch (InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket ||
					InElement->GetType() == ERigElementType::Connector)
				{
					const FRigElementKey& NodeKey = InElement->GetKey();
					if(!ContainsElementKeyNode(NodeKey))
					{
						AddElementKeyNode(NodeKey);
					}
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket ||
					InElement->GetType() == ERigElementType::Connector)
				{
					const FString OldNameStr = InHierarchy->GetPreviousName(InElement->GetKey()).ToString();
					FRigElementKey OldKey(*OldNameStr, InElement->GetType());
					// todooo
					//RenameNode(OldKey.ToString(), InElement->GetKey().ToString());
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRemoved:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket ||
					InElement->GetType() == ERigElementType::Connector)
				{
					RemoveElementKeyNode(InElement->GetKey());
				}
			}
			break;
		}
		case ERigHierarchyNotification::HierarchyReset:
		case ERigHierarchyNotification::HierarchyCopied:
		{
			const TArray<FRigSocketElement*> Sockets = InHierarchy->GetElementsOfType<FRigSocketElement>();
			for (const FRigSocketElement* Socket : Sockets)
			{
				if(!ContainsElementKeyNode(Socket->GetKey()))
				{
					AddElementKeyNode(Socket->GetKey());
				}
			}

			const TArray<FRigConnectorElement*> Connectors = InHierarchy->GetElementsOfType<FRigConnectorElement>();
			for (const FRigConnectorElement* Connector : Connectors)
			{
				if(!ContainsElementKeyNode(Connector->GetKey()))
				{
					AddElementKeyNode(Connector->GetKey());
				}
			}

			// remove obsolete nodes
			TArray<FGuid> GuidsToRemove;
			for(const TSharedPtr<FSchematicGraphNode>& Node : Nodes)
			{
				if(const FControlRigSchematicRigElementKeyNode* ElementKeyNode = Cast<FControlRigSchematicRigElementKeyNode>(Node.Get()))
				{
					if(!InHierarchy->Contains(ElementKeyNode->GetKey()))
					{
						GuidsToRemove.Add(ElementKeyNode->GetGuid());
					}
				}
			}
			for(const FGuid& Guid : GuidsToRemove)
			{
				RemoveNode(Guid);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void FControlRigSchematicModel::HandleModularRigModified(EModularRigNotification InNotification, const FRigModuleReference* InModule)
{
	switch (InNotification)
	{
		case EModularRigNotification::ConnectionChanged:
		{
			if (ControlRigBeingDebuggedPtr.IsValid())
			{
				if (const URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
				{
					const TArray<FRigElementKey> Connectors = Hierarchy->GetConnectorKeys();
					for(const FRigElementKey& ConnectorKey : Connectors)
					{
						if(const FSchematicGraphNode* ConnectorNode = FindElementKeyNode(ConnectorKey))
						{
							const FString NameString = ConnectorKey.Name.ToString();
							if(NameString.StartsWith(InModule->GetNamespace(), ESearchCase::CaseSensitive))
							{
								const FString LocalName = NameString.Mid(InModule->GetNamespace().Len());
								if(!LocalName.Contains(UModularRig::NamespaceSeparator))
								{
									const FRigElementKey LocalKey(*LocalName, ConnectorKey.Type);
									if(const FRigElementKey* ResolvedKey = InModule->Connections.Find(LocalKey))
									{
										if(const FSchematicGraphNode* ResolvedNode = FindElementKeyNode(*ResolvedKey))
										{
											(void)SetParentNode(ConnectorNode, ResolvedNode);
											const_cast<FSchematicGraphNode*>(ConnectorNode)->OnMouseLeave();
											return;
										}
									}
									if(const FSchematicGraphNode* ParentNode = ConnectorNode->GetParentNode())
									{
										(void)RemoveFromParentNode(ConnectorNode);
										const_cast<FSchematicGraphNode*>(ConnectorNode)->OnMouseLeave();
									}
									return;
								}
							}
						}
					}
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

FVector2d FControlRigSchematicModel::GetPositionForNode(const FSchematicGraphNode* InNode) const
{
	if(const FControlRigSchematicRigElementKeyNode* Node = Cast<FControlRigSchematicRigElementKeyNode>(InNode))
	{
		if(!InNode->HasParentNode())
		{
			FRigElementKey Key = Node->GetKey();
			if(Key.Type == ERigElementType::Connector)
			{
				FRigElementKey ResolvedKey;
				if(IsConnectorResolved(Key, &ResolvedKey))
				{
					Key = ResolvedKey;
				}
			}

			if(Key.IsValid())
			{
				if(const TSharedPtr<FControlRigEditor> Editor = ControlRigEditor.Pin())
				{
					if(Editor.IsValid())
					{
						if(const UControlRig* ControlRig = Editor->GetControlRig())
						{
							if(const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
							{
								const FTransform Transform = Hierarchy->GetGlobalTransform(Key);
								return Editor->ComputePersonaProjectedScreenPos(Transform.GetLocation());
							}
						}
					}
				}
			}
		}
	}
	return Super::GetPositionForNode(InNode);
}

bool FControlRigSchematicModel::GetPositionAnimationEnabledForNode(const FSchematicGraphNode* InNode) const
{
	if(InNode->IsA<FControlRigSchematicRigElementKeyNode>())
	{
		return false;
	}
	return Super::GetPositionAnimationEnabledForNode(InNode);
}

int32 FControlRigSchematicModel::GetNumLayersForNode(const FSchematicGraphNode* InNode) const
{
	if(const FControlRigSchematicRigElementKeyNode* Node = Cast<FControlRigSchematicRigElementKeyNode>(InNode))
	{
		return 3;
	}
	return Super::GetNumLayersForNode(InNode);
}

const FSlateBrush* FControlRigSchematicModel::GetBrushForNode(const FSchematicGraphNode* InNode, int32 InLayerIndex) const
{
	if(const FControlRigSchematicRigElementKeyNode* Node = Cast<FControlRigSchematicRigElementKeyNode>(InNode))
	{
		if(InLayerIndex == 0)
		{
			static const FSlateBrush* BackgroundBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Background");
			return BackgroundBrush;
		}
		if(InLayerIndex == 1)
		{
			static const FSlateBrush* OutlineSingleBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Outline.Single");
			static const FSlateBrush* OutlineDoubleBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Outline.Double");

			if(Node->GetKey().Type == ERigElementType::Socket)
			{
				if (ControlRigBlueprint.IsValid())
				{
					int32 Count = 0;
					for(const TPair<FRigElementKey, FRigElementKey>& Pair : ControlRigBlueprint->ConnectionMap)
					{
						if(Pair.Value == Node->GetKey())
						{
							Count++;
							if(Count == 2)
							{
								return OutlineDoubleBrush;
							}
						}
					}
				}
			}
			return OutlineSingleBrush;
		}
		
		switch(Node->GetKey().Type)
		{
			case ERigElementType::Socket:
			{
				static const FSlateBrush* UnresolvedSocketBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Dot.Small");
				static const FSlateBrush* ResolvedSocketBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Dot.Large");
				static const FSlateBrush* ResolvedMultipleSocketBrush = FSchematicGraphStyle::Get().GetBrush( "Schematic.Dot.Medium");

				if (ControlRigBlueprint.IsValid())
				{
					int32 Count = 0;
					for(const TPair<FRigElementKey, FRigElementKey>& Pair : ControlRigBlueprint->ConnectionMap)
					{
						if(Pair.Value == Node->GetKey())
						{
							Count++;
							if(Count == 2)
							{
								break;
							}
						}
					}

					if(Count == 1)
					{
						return ResolvedSocketBrush;
					}
					if(Count == 2)
					{
						return ResolvedMultipleSocketBrush;
					}
				}

				return UnresolvedSocketBrush;
			}
			case ERigElementType::Connector:
			{
				static const FSlateBrush* ConnectorPrimaryBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorPrimary");
				static const FSlateBrush* ConnectorOptionalBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorOptional");
				static const FSlateBrush* ConnectorSecondaryBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorSecondary");

				if (ControlRigBlueprint.IsValid())
				{
					if(const UControlRig* ControlRig = ControlRigBlueprint->GetDebuggedControlRig())
					{
						if(const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
						{
							if(const FRigConnectorElement* Connector = Hierarchy->Find<FRigConnectorElement>(Node->GetKey()))
							{
								if(Connector->Settings.Type == EConnectorType::Secondary)
								{
									return Connector->Settings.bOptional ? 	ConnectorOptionalBrush : ConnectorSecondaryBrush;
								}
							}
						}
					}
				}
				return ConnectorPrimaryBrush;
			}
			case ERigElementType::Bone:
			{
				static const FSlateBrush* BoneBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.Bone");
				return BoneBrush;
			}
			case ERigElementType::Control:
			{
				static const FSlateBrush* ControlBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.Control");
				return ControlBrush;
			}
			case ERigElementType::Null:
			{
				static const FSlateBrush* NullBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.Null");
				return NullBrush;
			}
			default:
			{
				break;
			}
		}
	}
	return Super::GetBrushForNode(InNode, InLayerIndex);
}

FLinearColor FControlRigSchematicModel::GetColorForNode(const FSchematicGraphNode* InNode, int32 InLayerIndex) const
{
	if(const FControlRigSchematicRigElementKeyNode* ElementKeyNode = Cast<FControlRigSchematicRigElementKeyNode>(InNode))
	{
		if(InLayerIndex == 0) // background
		{
			return FLinearColor(0, 0, 0, 0.75);
		}
		if(InLayerIndex == 1) // outline
		{
			return FLinearColor::White;
		}
		
		switch(ElementKeyNode->GetKey().Type)
		{
			case ERigElementType::Bone:
			{
				return FControlRigEditorStyle::Get().BoneUserInterfaceColor;
			}
			case ERigElementType::Null:
			{
				return FControlRigEditorStyle::Get().NullUserInterfaceColor;
			}
			case ERigElementType::Control:
			{
				if (ControlRigBeingDebuggedPtr.IsValid())
				{
					if (const URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
					{
						if(const FRigControlElement* Control = Hierarchy->Find<FRigControlElement>(ElementKeyNode->GetKey()))
						{
							return Control->Settings.ShapeColor;
						}
					}
				}
			}
			case ERigElementType::Socket:
			{
				if (ControlRigBeingDebuggedPtr.IsValid())
				{
					if (const URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
					{
						if(const FRigSocketElement* Socket = Hierarchy->Find<FRigSocketElement>(ElementKeyNode->GetKey()))
						{
							return Socket->GetColor(Hierarchy);
						}
					}
				}
				break;
			}
			case ERigElementType::Connector:
			{
				return FLinearColor::White;
				//return FControlRigEditorStyle::Get().ConnectorUserInterfaceColor;
			}
			default:
			{
				break;
			}
		}
	}
	return Super::GetColorForNode(InNode, InLayerIndex);
}

ESchematicGraphPlacementConstraint::Type FControlRigSchematicModel::GetPlacementForNode(const FSchematicGraphNode* InNode) const
{
	if(const FControlRigSchematicRigElementKeyNode* Node = Cast<FControlRigSchematicRigElementKeyNode>(InNode))
	{
		if(Node->GetKey().Type == ERigElementType::Connector)
		{
			if(!IsConnectorResolved(Node->GetKey()))
			{
				return ESchematicGraphPlacementConstraint::BottomRight;
			}
		}
	}
	return Super::GetPlacementForNode(InNode);
}

const FSlateBrush* FControlRigSchematicModel::GetBrushForLink(const FSchematicGraphLink* InLink) const
{
	if(InLink->IsA<FControlRigSchematicRigElementKeyLink>())
	{
		if(const FSchematicGraphNode* SourceNode = FindNode(InLink->GetSourceNodeGuid()))
		{
			if(const FSchematicGraphNode* TargetNode = FindNode(InLink->GetTargetNodeGuid()))
			{
				if(SourceNode->IsA<FControlRigSchematicRigElementKeyNode>() &&
					TargetNode->IsA<FControlRigSchematicRigElementKeyNode>())
				{
					static const FSlateBrush* LinkBrush = FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.Link");
					return LinkBrush;
				}
			}
		}
	}
	return Super::GetBrushForLink(InLink);
}

FLinearColor FControlRigSchematicModel::GetColorForLink(const FSchematicGraphLink* InLink) const
{
	if(const FSchematicGraphNode* SourceNode = FindNode(InLink->GetSourceNodeGuid()))
	{
		if(const FSchematicGraphNode* TargetNode = FindNode(InLink->GetTargetNodeGuid()))
		{
			if(SourceNode->IsA<FControlRigSchematicRigElementKeyNode>() &&
				TargetNode->IsA<FControlRigSchematicRigElementKeyNode>())
			{
				// semi transparent dark gray
				return FLinearColor(0.7, 0.7, 0.7, .5);
			}
		}
	}
	return Super::GetColorForLink(InLink);
}

ESchematicGraphVisibility::Type FControlRigSchematicModel::GetVisibilityForTag(const FSchematicGraphTag* InTag) const
{
	if(InTag->IsA<FSchematicGraphGroupTag>())
	{
		if(const FControlRigSchematicRigElementKeyNode* Node = Cast<FControlRigSchematicRigElementKeyNode>(InTag->GetNode()))
		{
			if(Node->GetKey().Type == ERigElementType::Socket)
			{
				if(Node->GetNumChildNodes() < 3)
				{
					return ESchematicGraphVisibility::Hidden;
				}
			}
		}
	}
	return Super::GetVisibilityForTag(InTag);
}

void FControlRigSchematicModel::HandleSchematicNodeClicked(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FPointerEvent& InMouseEvent)
{
	for (const TSharedPtr<FSchematicGraphNode>& Node : Nodes)
	{
		Node->SetSelected(Node->GetGuid() == InNode->GetGuid());
	}

	if(const FControlRigSchematicRigElementKeyNode* Node =
			Cast<FControlRigSchematicRigElementKeyNode>(InNode->GetNodeData()))
	{
		if (ControlRigBeingDebuggedPtr.IsValid())
		{
			if (URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
			{
				if (URigHierarchyController* Controller = Hierarchy->GetController())
				{
					const TArray<FRigElementKey> Selection = {Node->GetKey()};
					Controller->SetSelection(Selection);
				}
			}
		}
	}
}

void FControlRigSchematicModel::HandleSchematicBeginDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation)
{
	if (!ControlRigBlueprint.IsValid())
	{
		return;
	}

	if (!ControlRigBeingDebuggedPtr.IsValid())
	{
		return;
	}

	const FControlRigSchematicRigElementKeyNode* ElementKeyNode =
		Cast<FControlRigSchematicRigElementKeyNode>(InNode->GetNodeData());
	if(ElementKeyNode == nullptr)
	{
		return;
	}
	
	URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey DraggedKey = ElementKeyNode->GetKey();
	if (!DraggedKey.IsValid())
	{
		return;
	}

	FRigBaseElement* Element = Hierarchy->Find(DraggedKey);
	if (!Element)
	{
		return;
	}
	
	const FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Element);
	if (!Connector)
	{
		return;
	}

	const UModularRig* ModularRig = Cast<UModularRig>(ControlRigBeingDebuggedPtr);
	if (!ModularRig)
	{
		return;
	}

	FString ModulePath = Hierarchy->GetNameMetadata(DraggedKey, URigHierarchy::NameSpaceMetadataName, NAME_None).ToString();
	ModulePath.RemoveFromEnd(UModularRig::NamespaceSeparator);
	const FRigModuleInstance* ModuleInstance = ModularRig->FindModule(ModulePath);
	if (!ModuleInstance)
	{
		return;
	}

	if (UModularRigController* Controller = ControlRigBlueprint->GetModularRigController())
	{
		Controller->DisconnectConnector(Connector->GetKey(), true);
	}

	const UModularRigRuleManager* RuleManager = Hierarchy->GetRuleManager();
	const FModularRigResolveResult Result = RuleManager->FindMatches(Connector, ModuleInstance, ControlRigBeingDebuggedPtr->GetElementKeyRedirector());

	const TArray<FRigElementResolveResult> Matches = Result.GetMatches();

	for (const FRigElementResolveResult& Match : Matches)
	{
		// Create a temporary node that will be active only while this drag operation exists
		if (!ContainsElementKeyNode(Match.GetKey()))
		{
			const FSchematicGraphNode* NewNode = AddElementKeyNode(Match.GetKey());
			TemporaryNodeGuids.Add(NewNode->GetGuid());
		}
	}

	// Fade all the unmatched nodes
	for (const TSharedPtr<FSchematicGraphNode>& Node : Nodes)
	{
		if(FControlRigSchematicRigElementKeyNode* ExistingElementKeyNode = Cast<FControlRigSchematicRigElementKeyNode>(Node.Get()))
		{
			ExistingElementKeyNode->SetVisibility(Matches.ContainsByPredicate([ExistingElementKeyNode](const FRigElementResolveResult& Match)
			{
				return ExistingElementKeyNode->GetKey() == Match.GetKey();
			}) ? ESchematicGraphVisibility::Visible : ESchematicGraphVisibility::Hidden);
		}
	};
}

void FControlRigSchematicModel::HandleSchematicEndDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation)
{
	for (FGuid& TempNodeGuid : TemporaryNodeGuids)
	{
		RemoveNode(TempNodeGuid);
	}
	TemporaryNodeGuids.Reset();

	for (const TSharedPtr<FSchematicGraphNode>& Node : Nodes)
	{
		Node->SetVisibility(ESchematicGraphVisibility::Visible);
	}
	UpdateElementKeyLinks();
}

void FControlRigSchematicModel::HandleSchematicDrop(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropEvent& InDragDropEvent)
{
	if (!ControlRigBlueprint.IsValid())
	{
		return;
	}

	const FControlRigSchematicRigElementKeyNode* ElementKeyNode =
		Cast<FControlRigSchematicRigElementKeyNode>(InNode->GetNodeData());
	if(ElementKeyNode == nullptr)
	{
		return;
	}
	
	UControlRig* ControlRig = ControlRigBlueprint->GetDebuggedControlRig();
	if (!ControlRig)
	{
		return;
	}

	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey& TargetKey = ElementKeyNode->GetKey();
	FRigBaseElement* Target = Hierarchy->Find(TargetKey);
	if (!Target)
	{
		return;
	}

	TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>();
	TSharedPtr<FSchematicGraphNodeDragDropOp> SchematicDragDropOp = InDragDropEvent.GetOperationAs<FSchematicGraphNodeDragDropOp>();
	if (AssetDragDropOp.IsValid())
	{
		for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
		{
			UClass* AssetClass = AssetData.GetClass();
			if (!AssetClass->IsChildOf(UControlRigBlueprint::StaticClass()))
			{
				continue;
			}

			if(UControlRigBlueprint* AssetBlueprint = Cast<UControlRigBlueprint>(AssetData.GetAsset()))
			{
				FFunctionGraphTask::CreateAndDispatchWhenReady([this, AssetBlueprint, TargetKey]()
				{
					if (UModularRigController* Controller = ControlRigBlueprint->GetModularRigController())
					{
						UModularRig* ControlRig = Cast<UModularRig>(ControlRigBlueprint->GetDebuggedControlRig());
						if (!ControlRig)
						{
							return;
						}

						URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
						if (!Hierarchy)
						{
							return;
						}

						const FName ModuleName = Controller->GetSafeNewName(FString(), FRigName(AssetBlueprint->RigModuleSettings.Identifier.Name));
						const FString ModulePath = Controller->AddModule(ModuleName, AssetBlueprint->GetControlRigClass(), FString());
						if(!ModulePath.IsEmpty())
						{
							FRigElementKey PrimaryConnectorKey;
							TArray<FRigConnectorElement*> Connectors = Hierarchy->GetElementsOfType<FRigConnectorElement>();
							for (FRigConnectorElement* Connector : Connectors)
							{
								if (Connector->Settings.Type == EConnectorType::Primary)
								{
									FString Path, Name;
									Connector->GetName().Split(UModularRig::NamespaceSeparator, &Path, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
									if (Path == ModulePath)
									{
										PrimaryConnectorKey = Connector->GetKey();
										break;
									}
								}
							}

							const FName TargetModulePath = Hierarchy->GetNameMetadata(TargetKey, URigHierarchy::NameSpaceMetadataName, NAME_None);
							if(!TargetModulePath.IsNone())
							{
								(void)Controller->ReparentModule(ModulePath, TargetModulePath.ToString());
							}
							Controller->ConnectConnectorToElement(PrimaryConnectorKey, TargetKey, true, ControlRig->GetModularRigSettings().bAutoResolve);
						}
					}
				}, TStatId(), NULL, ENamedThreads::GameThread);
			}
		}
	}
	else if(SchematicDragDropOp.IsValid())
	{
		const TArray<FGuid> Sources = SchematicDragDropOp->GetElements();
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, Sources, TargetKey]()
		{
			UModularRig* ControlRig = Cast<UModularRig>(ControlRigBlueprint->GetDebuggedControlRig());
			if (!ControlRig)
			{
				return;
			}

			URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
			if (!Hierarchy)
			{
				return;
			}

			if (UModularRigController* Controller = ControlRigBlueprint->GetModularRigController())
			{
				for (const TPair<FRigElementKey, FGuid> Pair : RigElementKeyToGuid)
				{
					if(Sources.Contains(Pair.Value))
					{
						if (Hierarchy->Find<FRigConnectorElement>(Pair.Key))
						{
							Controller->ConnectConnectorToElement(Pair.Key, TargetKey, true, ControlRig->GetModularRigSettings().bAutoResolve);
							break;
						}
					}
				}
			}
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
}

bool FControlRigSchematicModel::IsConnectorResolved(const FRigElementKey& InConnectorKey, FRigElementKey* OutKey) const
{
	if(ControlRigBlueprint.IsValid() && InConnectorKey.Type == ERigElementType::Connector)
	{
		if(const FRigElementKey* TargetKey = ControlRigBlueprint->ConnectionMap.Find(InConnectorKey))
		{
			// make sure the target exists
			if(ControlRigBeingDebuggedPtr.IsValid())
			{
				if(const URigHierarchy* DebuggedHierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
				{
					if(!DebuggedHierarchy->Contains(*TargetKey))
					{
						return false;
					}
				}
			}
			
			if(OutKey)
			{
				*OutKey = *TargetKey;
			}
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
