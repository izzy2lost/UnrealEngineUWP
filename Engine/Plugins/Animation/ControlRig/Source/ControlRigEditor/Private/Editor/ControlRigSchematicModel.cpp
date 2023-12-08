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
	}
}

void FControlRigSchematicModel::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	switch (InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket ||
					InElement->GetType() == ERigElementType::Connector)
				{
					AddNode(InElement->GetKey().ToString());
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
					RenameNode(OldKey.ToString(), InElement->GetKey().ToString());
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
					RemoveNode(InElement->GetKey().ToString());
				}
			}
			break;
		}
		case ERigHierarchyNotification::HierarchyReset:
		{
			Reset();
			TArray<FRigSocketElement*> Sockets = InHierarchy->GetElementsOfType<FRigSocketElement>();
			for (FRigSocketElement* Socket : Sockets)
			{
				AddNode(Socket->GetKey().ToString());
			}
			if (ControlRigBeingDebuggedPtr.IsValid())
			{
				FRigElementKeyRedirector& Redirector = ControlRigBeingDebuggedPtr->GetElementKeyRedirector();
				TArray<FRigConnectorElement*> Connectors = InHierarchy->GetElementsOfType<FRigConnectorElement>();
				for (FRigConnectorElement* Connector : Connectors)
				{
					if (!Redirector.Contains(Connector->GetKey()))
					{
						AddNode(Connector->GetKey().ToString());
					}
				}
			}
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
				FRigElementKeyRedirector& Redirector = ControlRigBeingDebuggedPtr->GetElementKeyRedirector();
				for (TPair<FRigElementKey, FRigElementKey>& Pair : Redirector.ExternalKeys)
				{
					RemoveNode(Pair.Key.ToString());
				}
			}
			break;
		}
	}
}

void FControlRigSchematicModel::HandleSchematicNodeClicked(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode)
{
	for (FSchematicGraphNode* Node : Nodes)
	{
		Node->bIsSelected = Node == InNode->NodeData;
	}

	if (ControlRigBeingDebuggedPtr.IsValid())
	{
		if (URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
		{
			if (URigHierarchyController* Controller = Hierarchy->GetController())
			{
				TArray<FRigElementKey> Selection = {FRigElementKey(InNode->NodeData->Name)};
				Controller->SetSelection(Selection);
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

	URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	FRigElementKey DraggedKey(InNode->NodeData->Name);
	if (!DraggedKey.IsValid())
	{
		return;
	}

	FRigBaseElement* Element = Hierarchy->Find(DraggedKey);
	if (!Element)
	{
		return;
	}
	
	FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Element);
	if (!Connector)
	{
		return;
	}

	UModularRig* ModularRig = Cast<UModularRig>(ControlRigBeingDebuggedPtr);
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

	UModularRigRuleManager* RuleManager = Hierarchy->GetRuleManager();
	FModularRigResolveResult Result = RuleManager->FindMatches(Connector, ModuleInstance, ControlRigBeingDebuggedPtr->GetElementKeyRedirector());

	TArray<FRigElementResolveResult> Matches = Result.GetMatches();
	TArray<FString> NameMatches;
	Algo::Transform(Matches, NameMatches, [](const FRigElementResolveResult& Match)
	{
		return Match.GetKey().ToString();
	});

	// Fade all the unmatched nodes
	for (FSchematicGraphNode* Node : Nodes)
	{
		Node->bFade = !NameMatches.ContainsByPredicate([Node](const FString& Match)
		{
			return Node->Name == Match;
		});
	};

	// Unfade the ones included in the match
	for (const FString& Match : NameMatches)
	{
		bool bExists = Nodes.ContainsByPredicate([Match](const FSchematicGraphNode* Node)
		{
			return Match == Node->Name;
		});

		// Create a temporary node that will be active only while this drag operation exists
		if (!bExists)
		{
			FSchematicGraphNode* NewNode = AddNode(Match);
			TemporaryNodes.Add(Match);
		}
	}
}

void FControlRigSchematicModel::HandleSchematicEndDrag(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropOperation& InDragDropOperation)
{
	for (FString& TempNode : TemporaryNodes)
	{
		RemoveNode(TempNode);
	}
	TemporaryNodes.Reset();

	for (FSchematicGraphNode* Node : Nodes)
	{
		Node->bFade = false;
	}
}

void FControlRigSchematicModel::HandleUpdateSchematicNodes(SSchematicGraphPanel* InPanel, TSharedPtr<SSchematicGraphNode> InNode)
{
	if (!ControlRigBlueprint.IsValid())
	{
		return;
	}

	if (InPanel->bIsOverlay)
	{
		FTransform Transform;
		if (ControlRigBeingDebuggedPtr.IsValid())
		{
			URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy();
			FRigElementKey ElementKey(InNode->NodeData->Name);

			if (ElementKey.IsTypeOf(ERigElementType::Socket))
			{
				Transform = Hierarchy->GetGlobalTransform(ElementKey);
				FVector2D PixelPos = ControlRigEditor.Pin()->ComputePersonaProjectedScreenPos(Transform.GetLocation());
				InNode->SetPosition(PixelPos, true);

				if (const FRigElementKey* ConnectorKey = ControlRigBeingDebuggedPtr->GetElementKeyRedirector().FindReverse(ElementKey))
				{
					InNode->Brush = *FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.SocketResolved");
					InNode->Size->Set(InNode->OriginalSize*0.5);
				}
				else
				{
					InNode->Brush = *FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.SocketUnresolved");
					InNode->Size->Set(InNode->OriginalSize);
				}

				if (FRigSocketElement* SocketElement = Cast<FRigSocketElement>(Hierarchy->Find(ElementKey)))
				{
					InNode->Brush.TintColor = SocketElement->GetColor(Hierarchy);
				}
				else
				{
					InNode->Brush.TintColor = FStyleColors::AccentBlue;
				}
			}
			else if(ElementKey.IsTypeOf(ERigElementType::Connector))
			{
				InNode->NodeData->Placement = ESchematicGraphNodePlacement::BottomRight;
				InNode->Size->Set(InNode->OriginalSize);
				if (FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Hierarchy->Find(ElementKey)))
				{
					if (ConnectorElement->Settings.Type == EConnectorType::Primary)
					{
						InNode->Brush = *FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorPrimary");
					}
					else if(ConnectorElement->Settings.Type == EConnectorType::Secondary)
					{
						if (ConnectorElement->Settings.bOptional)
						{
							InNode->Brush = *FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorOptional");
						}
						else
						{
							InNode->Brush = *FControlRigEditorStyle::Get().GetBrush( "ControlRig.Schematic.ConnectorSecondary");
						}
					}
				}
			}
			else
			{
				Transform = Hierarchy->GetGlobalTransform(ElementKey);
				FVector2D PixelPos = ControlRigEditor.Pin()->ComputePersonaProjectedScreenPos(Transform.GetLocation());
				InNode->SetPosition(PixelPos, true);

				InNode->Size->Set(FVector2d(20, 20));
				InNode->Brush.TintColor = FStyleColors::AccentBlue;
			}
		}

	}
	else
	{
		InNode->SetPosition(FVector2D(0, 0));
		InNode->Brush = *FAppStyle::GetBrush("WhiteTexture");
	}
}

void FControlRigSchematicModel::HandleSchematicDrop(SSchematicGraphPanel* InPanel, SSchematicGraphNode* InNode, const FDragDropEvent& InDragDropEvent)
{
	if (!ControlRigBlueprint.IsValid())
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
	
	FRigBaseElement* Target = Hierarchy->Find(FRigElementKey(InNode->NodeData->Name));
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
				if (UModularRigController* Controller = ControlRigBlueprint->GetModularRigController())
				{
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
						Controller->ConnectConnectorToElement(PrimaryConnectorKey, Target->GetKey());
					}
				}
			}
		}
	}
	else if(SchematicDragDropOp.IsValid())
	{
		if (UModularRigController* Controller = ControlRigBlueprint->GetModularRigController())
		{
			TArray<FString> Sources = SchematicDragDropOp->GetElements();
			for (FString& SourceName : Sources)
			{
				FRigElementKey Key(SourceName);
				if (FRigBaseElement* SourceElement = Hierarchy->Find(SourceName))
				{
					if (FRigConnectorElement* Connector = Cast<FRigConnectorElement>(SourceElement))
					{
						Controller->ConnectConnectorToElement(Key, Target->GetKey());
					}
				}
			}
		}
	}
}