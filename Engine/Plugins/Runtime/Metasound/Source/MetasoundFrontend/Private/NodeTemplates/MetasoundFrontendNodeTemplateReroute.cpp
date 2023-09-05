// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"

#include "Algo/AnyOf.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendRegistries.h"
#include "NodeTemplates/MetasoundFrontendDocumentTemplatePreprocessor.h"


namespace Metasound::Frontend
{
	namespace ReroutePrivate
	{
		class FRerouteNodeTemplatePreprocessTransform : public INodeTransform
		{
		public:
			FRerouteNodeTemplatePreprocessTransform() = default;
			virtual ~FRerouteNodeTemplatePreprocessTransform() = default;

			virtual bool Transform(const FMetasoundFrontendNode& InNode, FMetaSoundFrontendDocumentBuilder& OutBuilder) const override;
		};

		bool FRerouteNodeTemplatePreprocessTransform::Transform(const FMetasoundFrontendNode& InNode, FMetaSoundFrontendDocumentBuilder& OutBuilder) const
		{
			using namespace ReroutePrivate;
				
			// Find the input and output edges for this node
			const FMetasoundFrontendEdge* InputEdge = nullptr;
			{
				if (!ensure(InNode.Interface.Inputs.Num() == 1))
				{
					return false;
				}

				const FMetasoundFrontendVertex& InputVertex = InNode.Interface.Inputs.Last();
				FMetasoundFrontendVertexHandle InputNodeVertexHandle { InNode.GetID(), InputVertex.VertexID };
				TArray<const FMetasoundFrontendEdge*> InputEdges = OutBuilder.FindEdges(InNode.GetID(), InputVertex.VertexID);

				// This can happen if the reroute node isn't provided an input, so its perfectly
				// acceptable to just ignore this node as it ultimately provides no sourced input.
				if (InputEdges.IsEmpty())
				{
					return false;
				}
				InputEdge = InputEdges.Last();
			}

			TArray<const FMetasoundFrontendEdge*> OutputEdges;
			const FMetasoundFrontendVertex& OutputVertex = InNode.Interface.Outputs.Last();
			const FMetasoundFrontendVertexHandle OutputVertexHandle { InNode.GetID(), OutputVertex.VertexID };
			{
				if (!ensure(InNode.Interface.Outputs.Num() == 1))
				{
					return false;
				}

				OutputEdges = OutBuilder.FindEdges(InNode.GetID(), OutputVertex.VertexID);

				// This can happen if the reroute node isn't provided any outputs to connect to, so its
				// perfectly acceptable to just ignore this node as it ultimately provides no sourced input.
				if (OutputEdges.IsEmpty())
				{
					return false;
				}
			}

			// Update the output edges with the input edge 
			FMetasoundFrontendVertexHandle NewOutputEdgeNodeVertexHandle { InputEdge->FromNodeID, InputEdge->FromVertexID };

			for (const FMetasoundFrontendEdge* OutputEdge : OutputEdges)
			{
				check(OutputEdge);

				// Must copied before removal as pointer will become invalid
				FMetasoundFrontendEdge Edge = *OutputEdge;
				OutBuilder.RemoveEdge(Edge);

				Edge.FromNodeID = InputEdge->FromNodeID;
				Edge.FromVertexID = InputEdge->FromVertexID;
				OutBuilder.AddEdge(MoveTemp(Edge));
			}

			return true;
		}
	}

	const FMetasoundFrontendClassName FRerouteNodeTemplate::ClassName { "UE", "Reroute", "" };

	const FMetasoundFrontendVersion FRerouteNodeTemplate::Version { ClassName.GetFullName(), { 1, 0 } };

	TUniquePtr<INodeTransform> FRerouteNodeTemplate::GenerateNodeTransform(FMetasoundFrontendDocument& InPreprocessedDocument) const
	{
		return GenerateNodeTransform();
	}

	TUniquePtr<INodeTransform> FRerouteNodeTemplate::GenerateNodeTransform() const
	{
		using namespace ReroutePrivate;
		return TUniquePtr<INodeTransform>(new FRerouteNodeTemplatePreprocessTransform());
	}

	const FMetasoundFrontendClass& FRerouteNodeTemplate::GetFrontendClass() const
	{
		auto CreateFrontendClass = []()
		{
			FMetasoundFrontendClass Class;
			Class.Metadata.SetClassName(ClassName);

#if WITH_EDITOR
			Class.Metadata.SetSerializeText(false);
			Class.Metadata.SetAuthor(Metasound::PluginAuthor);
			Class.Metadata.SetDescription(Metasound::PluginNodeMissingPrompt);

			FMetasoundFrontendClassStyleDisplay& StyleDisplay = Class.Style.Display;
			StyleDisplay.ImageName = "MetasoundEditor.Graph.Node.Class.Reroute";
			StyleDisplay.bShowInputNames = false;
			StyleDisplay.bShowOutputNames = false;
			StyleDisplay.bShowLiterals = false;
			StyleDisplay.bShowName = false;
#endif // WITH_EDITOR

			Class.Metadata.SetType(EMetasoundFrontendClassType::Template);
			Class.Metadata.SetVersion(Version.Number);


			return Class;
		};

		static const FMetasoundFrontendClass FrontendClass = CreateFrontendClass();
		return FrontendClass;
	}

	FMetasoundFrontendNodeInterface FRerouteNodeTemplate::CreateNodeInterfaceFromDataType(FName InDataType)
	{
		auto CreateNewVertex = [&] { return FMetasoundFrontendVertex { "Value", InDataType, FGuid::NewGuid() }; };

		FMetasoundFrontendNodeInterface NewInterface;
		NewInterface.Inputs.Add(CreateNewVertex());
		NewInterface.Outputs.Add(CreateNewVertex());

		return NewInterface;
	}

	const FNodeRegistryKey& FRerouteNodeTemplate::GetRegistryKey()
	{
		static const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(
			EMetasoundFrontendClassType::Template,
			ClassName.ToString(), 
			Version.Number.Major, 
			Version.Number.Minor);

		return RegistryKey;
	}

	const FMetasoundFrontendVersion& FRerouteNodeTemplate::GetVersion() const
	{
		return Version;
	}

#if WITH_EDITOR
	bool FRerouteNodeTemplate::HasRequiredConnections(FConstNodeHandle InNodeHandle) const
	{
		TArray<FConstOutputHandle> Outputs = InNodeHandle->GetConstOutputs();
		TArray<FConstInputHandle> Inputs = InNodeHandle->GetConstInputs();

		const bool bConnectedToNonRerouteOutputs = Algo::AnyOf(Outputs, [](const FConstOutputHandle& OutputHandle) { return Frontend::FindReroutedOutput(OutputHandle)->IsValid(); });
		const bool bConnectedToNonRerouteInputs = Algo::AnyOf(Inputs, [](const FConstInputHandle& InputHandle)
		{
			TArray<FConstInputHandle> Inputs;
			Frontend::FindReroutedInputs(InputHandle, Inputs);
			return !Inputs.IsEmpty();
		});

		return bConnectedToNonRerouteOutputs || bConnectedToNonRerouteOutputs == bConnectedToNonRerouteInputs;
	}
#endif // WITH_EDITOR

	bool FRerouteNodeTemplate::IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const
	{
		if (InNodeInterface.Inputs.Num() != 1)
		{
			return false;
		}
			
		if (InNodeInterface.Outputs.Num() != 1)
		{
			return false;
		}

		const FName DataType = InNodeInterface.Inputs.Last().TypeName;
		if (DataType != InNodeInterface.Outputs.Last().TypeName)
		{
			return false;
		}

		return IDataTypeRegistry::Get().IsRegistered(DataType);
	}
} // namespace Metasound::Frontend
