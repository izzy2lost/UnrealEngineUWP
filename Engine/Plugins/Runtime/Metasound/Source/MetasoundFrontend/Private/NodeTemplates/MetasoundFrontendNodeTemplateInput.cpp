// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTemplates/MetasoundFrontendNodeTemplateInput.h"

#include "Algo/AnyOf.h"
#include "Internationalization/Text.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryKey.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundInputNode.h"

#if WITH_EDITOR
#include "MetasoundFrontendController.h"
#endif // WITH_EDITOR


namespace Metasound::Frontend
{
	namespace InputNodeTemplatePrivate
	{
		// Creates an input template node, sets node position (should only ever be one in style location) from and connects it to the associated input with the given name.
		const FMetasoundFrontendNode* InitTemplateNode(const INodeTemplate& InTemplate, FName InputName, FMetaSoundFrontendDocumentBuilder& InOutBuilder)
		{
			FMetasoundFrontendEdge NewEdge;
			FName TypeName;
#if WITH_EDITORONLY_DATA
			TMap<FGuid, FVector2D> Locations;
#endif // WITH_EDITORONLY_DATA

			// Cache data from Input node pointer as needed as subsequent call to create new template node may invalidate the input node's pointer
			{
				const FMetasoundFrontendNode* InputNode = InOutBuilder.FindGraphInputNode(InputName);
				check(InputNode);
				TypeName = InputNode->Interface.Outputs.Last().TypeName;
				NewEdge.FromNodeID = InputNode->GetID(),
				NewEdge.FromVertexID = InputNode->Interface.Outputs.Last().VertexID;

#if WITH_EDITORONLY_DATA
				Locations = InputNode->Style.Display.Locations;
				ensure(Locations.Num() <= 1);
#endif // WITH_EDITORONLY_DATA
			}

			FNodeTemplateGenerateInterfaceParams Params { { }, { TypeName } };
			const FMetasoundFrontendNode* NewNode = InOutBuilder.AddNodeByTemplate(InTemplate, MoveTemp(Params));
			check(NewNode);
			NewEdge.ToNodeID = NewNode->GetID();
			NewEdge.ToVertexID = NewNode->Interface.Inputs.Last().VertexID;

#if WITH_EDITORONLY_DATA
			for (const TPair<FGuid, FVector2D>& Pair : Locations)
			{
				InOutBuilder.SetNodeLocation(NewEdge.ToNodeID, Pair.Value);
			}
#endif // WITH_EDITORONLY_DATA

			// Add edge between input node and new template node
			InOutBuilder.AddEdge(MoveTemp(NewEdge));

			return NewNode;
		};
	} // namespace InputNodeTemplatePrivate

	const FMetasoundFrontendClassName FInputNodeTemplate::ClassName { "UE", "Input", "Template" };

	const FMetasoundFrontendVersionNumber FInputNodeTemplate::VersionNumber = { 1, 0 } ;

	const FMetasoundFrontendClassName& FInputNodeTemplate::GetClassName() const
	{
		return FInputNodeTemplate::ClassName;
	}

#if WITH_EDITOR
	FText FInputNodeTemplate::GetNodeDisplayName(const IMetaSoundDocumentInterface& Interface, const FGuid& InNodeID) const
	{
		return { };
	}
#endif // WITH_EDITOR

	const FMetasoundFrontendClass& FInputNodeTemplate::GetFrontendClass() const
	{
		auto CreateFrontendClass = []()
		{
			using namespace Metasound;

			FMetasoundFrontendClass Class;
			Class.Metadata.SetClassName(ClassName);

#if WITH_EDITOR
			Class.Metadata.SetSerializeText(false);
			Class.Metadata.SetAuthor(PluginAuthor);
			Class.Metadata.SetDescription(FInputNode::GetInputDescription());

			FMetasoundFrontendClassStyleDisplay& StyleDisplay = Class.Style.Display;
			StyleDisplay.bShowInputNames = false;
			StyleDisplay.bShowOutputNames = true;
			StyleDisplay.bShowLiterals = false;
			StyleDisplay.bShowName = true;
#endif // WITH_EDITOR

			Class.Metadata.SetType(EMetasoundFrontendClassType::Template);
			Class.Metadata.SetVersion(VersionNumber);

			return Class;
		};

		static const FMetasoundFrontendClass FrontendClass = CreateFrontendClass();
		return FrontendClass;
	}

	const FInputNodeTemplate& FInputNodeTemplate::GetChecked()
	{
		const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(GetRegistryKey());
		checkf(Template, TEXT("Failed to find InputNodeTemplate, which is required for migrating editor document data"));
		return static_cast<const FInputNodeTemplate&>(*Template);
	}

	const FNodeRegistryKey& FInputNodeTemplate::GetRegistryKey()
	{
		static const FNodeRegistryKey RegistryKey(EMetasoundFrontendClassType::Template, ClassName, VersionNumber);
		return RegistryKey;
	}

	EMetasoundFrontendVertexAccessType FInputNodeTemplate::GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		const FMetasoundFrontendNode* ConnectedInputNode = nullptr;
		if (const FMetasoundFrontendVertex* ConnectedInputOutput = InBuilder.FindNodeOutputConnectedToNodeInput(InNodeID, InVertexID, &ConnectedInputNode))
		{
			const FMetasoundFrontendClass* InputClass = InBuilder.FindDependency(ConnectedInputNode->ClassID);
			check(InputClass);
			return (InputClass->Interface.Outputs.Last().AccessType);
		}

		return EMetasoundFrontendVertexAccessType::Unset;
	}

	EMetasoundFrontendVertexAccessType FInputNodeTemplate::GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		if (const FMetasoundFrontendNode* Node = InBuilder.FindNode(InNodeID))
		{
			const FMetasoundFrontendVertex& Input = Node->Interface.Inputs.Last();
			return GetNodeInputAccessType(InBuilder, InNodeID, Input.VertexID);
		}

		return EMetasoundFrontendVertexAccessType::Unset;
	}

#if WITH_EDITOR
	FText FInputNodeTemplate::GetOutputPinDisplayName(const IOutputController& InOutput) const
	{
		FConstNodeHandle OwningNode = InOutput.GetOwningNode();
		FConstOutputHandle ConnectedOutput = OwningNode->GetConstInputs().Last()->GetConnectedOutput();
		if (ensureMsgf(ConnectedOutput->IsValid(), TEXT("Input template node should always be connected to associated input node's only output")))
		{
			FConstNodeHandle ConnectedInputNode = ConnectedOutput->GetOwningNode();

			FName NodeName = ConnectedInputNode->GetNodeName();
			FName Namespace;
			FName ParameterName;
			Audio::FParameterPath::SplitName(NodeName, Namespace, ParameterName);

			FText DisplayName = ConnectedInputNode->GetDisplayName();
			if (DisplayName.IsEmpty())
			{
				DisplayName = FText::FromName(NodeName);
			}

			if (!Namespace.IsNone())
			{
				return FText::Format(NSLOCTEXT("MetasoundFrontend", "InputNodeTemplate_DisplayNameWithNamespaceFormat", "{0} ({1})"), DisplayName, FText::FromName(Namespace));
			}

			return DisplayName;
		}

		return FRerouteNodeTemplate::GetOutputPinDisplayName(InOutput);
	}

	bool FInputNodeTemplate::HasRequiredConnections(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, FString* OutMessage) const
	{
		return true;
	}
#endif // WITH_EDITOR

	bool FInputNodeTemplate::IsInputAccessTypeDynamic() const
	{
		return true;
	}

	bool FInputNodeTemplate::IsInputConnectionUserModifiable() const
	{
		return false;
	}

	bool FInputNodeTemplate::IsOutputAccessTypeDynamic() const
	{
		return true;
	}

#if WITH_EDITOR
	bool FInputNodeTemplate::Inject(FMetaSoundFrontendDocumentBuilder& InOutBuilder, bool bForceNodeCreation) const
	{
		bool bInjectedNodes = false;

		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = static_cast<const FMetaSoundFrontendDocumentBuilder&>(InOutBuilder);
		const TArray<FMetasoundFrontendClassInput>& Inputs = ConstBuilder.GetConstDocument().RootGraph.Interface.Inputs;
		for (const FMetasoundFrontendClassInput& Input : Inputs)
		{
			// Potentially not used input, which is perfectly valid so early out
			const FMetasoundFrontendNode* InputNode = InOutBuilder.FindGraphInputNode(Input.Name);
			if (!InputNode)
			{
				continue;
			}

			const FGuid& InputNodeOutputVertexID = InputNode->Interface.Outputs.Last().VertexID;

			TArray<const FMetasoundFrontendNode*> ConnectedInputNodes;
			TArray<const FMetasoundFrontendVertex*> ConnectedInputVertices = InOutBuilder.FindNodeInputsConnectedToNodeOutput(InputNode->GetID(), InputNodeOutputVertexID, &ConnectedInputNodes);

			FMetasoundFrontendEdge EdgeToRemove { InputNode->GetID(), InputNodeOutputVertexID };

			bool bHasTemplateConnection = false;

			TArray<FMetasoundFrontendVertexHandle> ConnectedVertices;
			for (int32 Index = 0; Index < ConnectedInputVertices.Num(); ++Index)
			{
				// Ignore edges already connected to input template nodes & cache connected vertex pair
				// as adding a template node in the subsequent step may invalidate these connected node/vertex
				// pointers.
				const FMetasoundFrontendVertex* ConnectedVertex = ConnectedInputVertices[Index];
				const FMetasoundFrontendNode* ConnectedNode = ConnectedInputNodes[Index];
				const FMetasoundFrontendClass* Class = InOutBuilder.FindDependency(ConnectedNode->ClassID);
				if (Class->Metadata.GetClassName() == FInputNodeTemplate::ClassName)
				{
					bHasTemplateConnection = true;
				}
				else
				{
					ConnectedVertices.Add(FMetasoundFrontendVertexHandle { ConnectedNode->GetID(), ConnectedVertex->VertexID });
				}
			}

			if (ConnectedVertices.IsEmpty())
			{
				if (bForceNodeCreation && !bHasTemplateConnection)
				{
					bInjectedNodes = true;
					InputNodeTemplatePrivate::InitTemplateNode(*this, Input.Name, InOutBuilder);
				}
			}
			else
			{
				bInjectedNodes = true;
				const FMetasoundFrontendNode* TemplateNode = InputNodeTemplatePrivate::InitTemplateNode(*this, Input.Name, InOutBuilder);
				check(TemplateNode);

				for (const FMetasoundFrontendVertexHandle& ConnectedVertex : ConnectedVertices)
				{
					// Swap connections from input node to connected node to now be from template node to connected node
					EdgeToRemove.ToNodeID = ConnectedVertex.NodeID;
					EdgeToRemove.ToVertexID = ConnectedVertex.VertexID;
					InOutBuilder.RemoveEdge(EdgeToRemove);

					InOutBuilder.AddEdge(FMetasoundFrontendEdge
					{
						TemplateNode->GetID(),
						TemplateNode->Interface.Outputs.Last().VertexID,
						ConnectedVertex.NodeID,
						ConnectedVertex.VertexID
					});
				}
			}
		}

		return bInjectedNodes;
	}
#endif // WITH_EDITOR
} // namespace Metasound::Frontend