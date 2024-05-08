// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundBuilderSubsystem.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Metasound.h"
#include "MetasoundAssetSubsystem.h"
#include "MetasoundDataReference.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"
#include "MetasoundVertex.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateInput.h"
#include "UObject/PerPlatformProperties.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundBuilderSubsystem)


namespace Metasound::Engine
{
	namespace BuilderSubsystemPrivate
	{
		template <typename TLiteralType>
		FMetasoundFrontendLiteral CreatePODMetaSoundLiteral(const TLiteralType& Value, FName& OutDataType)
		{
			OutDataType = GetMetasoundDataTypeName<TLiteralType>();

			FMetasoundFrontendLiteral Literal;
			Literal.Set(Value);
			return Literal;
		}

		TUniquePtr<INode> CreateDynamicNodeFromFrontendLiteral(const FName DataType, const FMetasoundFrontendLiteral& InLiteral)
		{
			using namespace Frontend;
			FLiteral ValueLiteral = InLiteral.ToLiteral(DataType);

			// TODO: Node name "Literal" is always the same.  Consolidate and deprecate providing unique node name to avoid unnecessary FName table bloat.
			FLiteralNodeConstructorParams Params { "Literal", FGuid::NewGuid(), MoveTemp(ValueLiteral) };
			return IDataTypeRegistry::Get().CreateLiteralNode(DataType, MoveTemp(Params));
		}
	} // namespace BuilderSubsystemPrivate
} // namespace Metasound::Engine


TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundPatchBuilder::Build(UObject* Parent, const FMetaSoundBuilderOptions& InBuilderOptions) const
{
	return &BuildInternal<UMetaSoundPatch>(Parent, InBuilderOptions);
}

const UClass& UMetaSoundPatchBuilder::GetBaseMetaSoundUClass() const
{
	return *UMetaSoundPatch::StaticClass();
}

void UMetaSoundPatchBuilder::OnAssetReferenceAdded(TScriptInterface<IMetaSoundDocumentInterface> DocInterface)
{
	check(DocInterface.GetObject());
	Builder.CastDocumentObjectChecked<UMetaSoundPatch>().ReferencedAssetClassObjects.Add(DocInterface.GetObject());
}

void UMetaSoundPatchBuilder::OnRemovingAssetReference(TScriptInterface<IMetaSoundDocumentInterface> DocInterface)
{
	check(DocInterface.GetObject());
	Builder.CastDocumentObjectChecked<UMetaSoundPatch>().ReferencedAssetClassObjects.Remove(DocInterface.GetObject());
}

UMetaSoundBuilderBase& UMetaSoundBuilderSubsystem::AttachBuilderToAssetChecked(UObject& InObject) const
{
	const UClass* BaseClass = InObject.GetClass();
	if (BaseClass == UMetaSoundSource::StaticClass())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UMetaSoundSourceBuilder* NewBuilder = AttachSourceBuilderToAsset(CastChecked<UMetaSoundSource>(&InObject));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return *NewBuilder;
	}
	else if (BaseClass == UMetaSoundPatch::StaticClass())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UMetaSoundPatchBuilder* NewBuilder = AttachPatchBuilderToAsset(CastChecked<UMetaSoundPatch>(&InObject));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return *NewBuilder;
	}
	else
	{
		checkf(false, TEXT("UClass '%s' is not a base MetaSound that supports attachment via the MetaSoundBuilderSubsystem"), *BaseClass->GetFullName());
		return *NewObject<UMetaSoundPatchBuilder>();
	}
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::AttachPatchBuilderToAsset(UMetaSoundPatch* InPatch) const
{
#if WITH_EDITORONLY_DATA
	using namespace Metasound::Engine;

	if (InPatch)
	{
		return &FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding<UMetaSoundPatchBuilder>(*InPatch);
	}
#endif // WITH_EDITORONLY_DATA

	return nullptr;
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::AttachSourceBuilderToAsset(UMetaSoundSource* InSource) const
{
#if WITH_EDITORONLY_DATA
	using namespace Metasound::Engine;

	if (InSource)
	{
		UMetaSoundSourceBuilder& SourceBuilder = FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding<UMetaSoundSourceBuilder>(*InSource);
		return &SourceBuilder;
	}
#endif // WITH_EDITORONLY_DATA

	return nullptr;
}

void UMetaSoundSourceBuilder::Audition(UObject* Parent, UAudioComponent* AudioComponent, FOnCreateAuditionGeneratorHandleDelegate CreateGenerator, bool bLiveUpdatesEnabled)
{
	using namespace Metasound;
	using namespace Metasound::DynamicGraph;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSourceBuilder::Audition);

	if (!AudioComponent)
	{
		UE_LOG(LogMetaSound, Error, TEXT("Failed to audition MetaSoundBuilder '%s': No AudioComponent supplied"), *GetFullName());
		return;
	}

	UMetaSoundSource& MetaSoundSource = GetMetaSoundSource();
	RegisterGraphIfOutstandingTransactions(MetaSoundSource);

	// Must be called post register as register ensures cached runtime data passed to transactor is up-to-date
	MetaSoundSource.SetDynamicGeneratorEnabled(bLiveUpdatesEnabled);
	MetaSoundSource.ConformObjectToDocument();

	AudioComponent->SetSound(&MetaSoundSource);

	if (CreateGenerator.IsBound())
	{
		UMetasoundGeneratorHandle* NewHandle = UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(AudioComponent);
		checkf(NewHandle, TEXT("BindToGeneratorDelegate Failed when attempting to audition MetaSoundSource builder '%s'"), *GetName());
		CreateGenerator.Execute(NewHandle);
	}

	if (bLiveUpdatesEnabled)
	{
		LiveComponentIDs.Add(AudioComponent->GetAudioComponentID());
		LiveComponentHandle = AudioComponent->OnAudioFinishedNative.AddUObject(this, &UMetaSoundSourceBuilder::OnLiveComponentFinished);
	}

	AudioComponent->Play();
}

void UMetaSoundSourceBuilder::OnLiveComponentFinished(UAudioComponent* AudioComponent)
{
	LiveComponentIDs.RemoveSwap(AudioComponent->GetAudioComponentID(), EAllowShrinking::No);
	if (LiveComponentIDs.IsEmpty())
	{
		AudioComponent->OnAudioFinishedNative.Remove(LiveComponentHandle);
	}
}

bool UMetaSoundSourceBuilder::ExecuteAuditionableTransaction(FAuditionableTransaction Transaction) const
{
	using namespace Metasound::Engine::BuilderSubsystemPrivate;
	using namespace Metasound::DynamicGraph;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSourceBuilder::ExecuteAuditionableTransaction);

	TSharedPtr<FDynamicOperatorTransactor> Transactor = GetMetaSoundSource().GetDynamicGeneratorTransactor();
	if (Transactor.IsValid())
	{
		return Transaction(*Transactor);
	}

	return false;
}

TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundSourceBuilder::Build(UObject* Parent, const FMetaSoundBuilderOptions& InBuilderOptions) const
{
	return &BuildInternal<UMetaSoundSource>(Parent, InBuilderOptions);
}

const Metasound::Engine::FOutputAudioFormatInfoPair* UMetaSoundSourceBuilder::FindOutputAudioFormatInfo() const
{
	using namespace Metasound::Engine;

	const FOutputAudioFormatInfoMap& FormatInfo = GetOutputAudioFormatInfo();

	auto Predicate = [this](const FOutputAudioFormatInfoPair& Pair)
	{
		const FMetasoundFrontendDocument& Document = Builder.GetConstDocument();
		return Document.Interfaces.Contains(Pair.Value.InterfaceVersion);
	};

	return Algo::FindByPredicate(FormatInfo, Predicate);
}

const UClass& UMetaSoundSourceBuilder::GetBaseMetaSoundUClass() const
{
	return *UMetaSoundSource::StaticClass();
}

bool UMetaSoundSourceBuilder::GetLiveUpdatesEnabled() const
{
	return GetMetaSoundSource().GetDynamicGeneratorTransactor().IsValid();
}

const UMetaSoundSource& UMetaSoundSourceBuilder::GetMetaSoundSource() const
{
	return GetConstBuilder().CastDocumentObjectChecked<UMetaSoundSource>();
}

UMetaSoundSource& UMetaSoundSourceBuilder::GetMetaSoundSource()
{
	return Builder.CastDocumentObjectChecked<UMetaSoundSource>();
}

void UMetaSoundSourceBuilder::InitDelegates(Metasound::Frontend::FDocumentModifyDelegates& OutDocumentDelegates)
{
	Super::InitDelegates(OutDocumentDelegates);

	OutDocumentDelegates.EdgeDelegates.OnEdgeAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnEdgeAdded);
	OutDocumentDelegates.EdgeDelegates.OnRemoveSwappingEdge.AddUObject(this, &UMetaSoundSourceBuilder::OnRemoveSwappingEdge);

	OutDocumentDelegates.InterfaceDelegates.OnInputAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnInputAdded);
	OutDocumentDelegates.InterfaceDelegates.OnOutputAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnOutputAdded);
	OutDocumentDelegates.InterfaceDelegates.OnRemovingInput.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingInput);
	OutDocumentDelegates.InterfaceDelegates.OnRemovingOutput.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingOutput);

	OutDocumentDelegates.NodeDelegates.OnNodeAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnNodeAdded);
	OutDocumentDelegates.NodeDelegates.OnNodeInputLiteralSet.AddUObject(this, &UMetaSoundSourceBuilder::OnNodeInputLiteralSet);
	OutDocumentDelegates.NodeDelegates.OnRemoveSwappingNode.AddUObject(this, &UMetaSoundSourceBuilder::OnRemoveSwappingNode);
	OutDocumentDelegates.NodeDelegates.OnRemovingNodeInputLiteral.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingNodeInputLiteral);
}

void UMetaSoundSourceBuilder::OnAssetReferenceAdded(TScriptInterface<IMetaSoundDocumentInterface> DocInterface)
{
	check(DocInterface.GetObject());
	GetMetaSoundSource().ReferencedAssetClassObjects.Add(DocInterface.GetObject());
}

void UMetaSoundSourceBuilder::OnEdgeAdded(int32 EdgeIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
	const FMetasoundFrontendEdge& NewEdge = Doc.RootGraph.Graph.Edges[EdgeIndex];
	ExecuteAuditionableTransaction([this, &NewEdge](Metasound::DynamicGraph::FDynamicOperatorTransactor& Transactor)
	{
		const FMetaSoundFrontendDocumentBuilder& Builder = GetConstBuilder();
		const FMetasoundFrontendVertex* FromNodeOutput = Builder.FindNodeOutput(NewEdge.FromNodeID, NewEdge.FromVertexID);
		const FMetasoundFrontendVertex* ToNodeInput = Builder.FindNodeInput(NewEdge.ToNodeID, NewEdge.ToVertexID);
		if (FromNodeOutput && ToNodeInput)
		{
			Transactor.AddDataEdge(NewEdge.FromNodeID, FromNodeOutput->Name, NewEdge.ToNodeID, ToNodeInput->Name);
			return true;
		}

		return false;
	});
}

TOptional<Metasound::FAnyDataReference> UMetaSoundSourceBuilder::CreateDataReference(
	const Metasound::FOperatorSettings& InOperatorSettings,
	FName DataType,
	const Metasound::FLiteral& InLiteral,
	Metasound::EDataReferenceAccessType AccessType)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	return IDataTypeRegistry::Get().CreateDataReference(DataType, AccessType, InLiteral, InOperatorSettings);
};

void UMetaSoundSourceBuilder::OnInputAdded(int32 InputIndex)
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, InputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassInput& NewInput = GraphClass.Interface.Inputs[InputIndex];

		constexpr bool bCreateUObjectProxies = true;
		UMetaSoundSource& Source = GetMetaSoundSource();
		Source.RuntimeInputData.InputMap.Add(NewInput.Name, UMetaSoundSource::CreateRuntimeInput(IDataTypeRegistry::Get(), NewInput, bCreateUObjectProxies));

		for (uint64 AudioComponentID : LiveComponentIDs)
		{
			if (UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(AudioComponentID))
			{
				if (FAudioDevice* AudioDevice = AudioComponent->GetAudioDevice())
				{
					AudioDevice->SendCommandToActiveSounds(AudioComponentID, [NewInputName = NewInput.Name](FActiveSound& ActiveSound)
					{
						static_cast<FMetaSoundParameterTransmitter*>(ActiveSound.GetTransmitter())->AddAvailableParameter(NewInputName);
					});
				}
			}
		}

		const FLiteral NewInputLiteral = NewInput.DefaultLiteral.ToLiteral(NewInput.TypeName);
		Transactor.AddInputDataDestination(NewInput.NodeID, NewInput.Name, NewInputLiteral, &UMetaSoundSourceBuilder::CreateDataReference);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnNodeAdded(int32 NodeIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, NodeIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendNode& AddedNode = GraphClass.Graph.Nodes[NodeIndex];

		const FMetasoundFrontendClass* NodeClass = Builder.FindDependency(AddedNode.ClassID);
		checkf(NodeClass, TEXT("Node successfully added to graph but document is missing associated dependency"));

		const FNodeRegistryKey& ClassKey = FNodeRegistryKey(NodeClass->Metadata);
		FMetasoundFrontendRegistryContainer& NodeRegistry = *FMetasoundFrontendRegistryContainer::Get();
		IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();

		TUniquePtr<INode> NewNode;

		switch (NodeClass->Metadata.GetType())
		{
			case EMetasoundFrontendClassType::VariableDeferredAccessor:
			case EMetasoundFrontendClassType::VariableAccessor:
			case EMetasoundFrontendClassType::VariableMutator:
			case EMetasoundFrontendClassType::External:
			case EMetasoundFrontendClassType::Graph:
			{
				const Metasound::FNodeInitData InitData { AddedNode.Name, AddedNode.GetID() };
				NewNode = NodeRegistry.CreateNode(ClassKey, InitData);
			}
			break;

			case EMetasoundFrontendClassType::Input:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				const FMetasoundFrontendVertex& InputVertex = AddedNode.Interface.Inputs.Last();

				const FMetasoundFrontendLiteral* DefaultLiteral = nullptr;
				auto HasEqualVertexID = [&VertexID = InputVertex.VertexID](const FMetasoundFrontendVertexLiteral& InVertexLiteral)
				{
					return InVertexLiteral.VertexID == VertexID;
				};

				// Check for default literal on Node
				if (const FMetasoundFrontendVertexLiteral* VertexLiteralOnNode = AddedNode.InputLiterals.FindByPredicate(HasEqualVertexID))
				{
					DefaultLiteral = &(VertexLiteralOnNode->Value);
				}
				// Check for default literal on class input
				else if (const FMetasoundFrontendClassInput* GraphInput = Builder.FindGraphInput(InputVertex.Name))
				{
					DefaultLiteral = &(GraphInput->DefaultLiteral);
				}
				else
				{
					// As a last resort, get default literal on node class 
					DefaultLiteral = &(NodeClass->Interface.Inputs.Last().DefaultLiteral);
				}

				FInputNodeConstructorParams InitData
				{
					AddedNode.Name,
					AddedNode.GetID(),
					InputVertex.Name,
					DefaultLiteral->ToLiteral(DataTypeName)
				};

				NewNode = DataTypeRegistry.CreateInputNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Variable:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				FDefaultLiteralNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), DataTypeRegistry.CreateDefaultLiteral(DataTypeName)};
				NewNode = DataTypeRegistry.CreateVariableNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Literal:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				FDefaultLiteralNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), DataTypeRegistry.CreateDefaultLiteral(DataTypeName)};
				NewNode = DataTypeRegistry.CreateLiteralNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Output:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				const FMetasoundFrontendVertex& OutputVertex = AddedNode.Interface.Outputs.Last();
				FDefaultNamedVertexNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), OutputVertex.Name };
				NewNode = DataTypeRegistry.CreateOutputNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Template:
			default:
			static_assert(static_cast<int32>(EMetasoundFrontendClassType::Invalid) == 10, "Possible missed EMetasoundFrontendClassType case coverage");
		};

		if (!NewNode.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' failed to create and forward added node '%s' to live update transactor."), *GetName(), *AddedNode.Name.ToString());
			return false;
		}

		Transactor.AddNode(AddedNode.GetID(), MoveTemp(NewNode));
		return true;
	});
}

void UMetaSoundSourceBuilder::OnNodeInputLiteralSet(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendGraphClass& GraphClass = Builder.GetConstDocument().RootGraph;
	const FMetasoundFrontendNode& Node = GraphClass.Graph.Nodes[NodeIndex];
	const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

	// Only send the literal down if not connected, as the graph core layer
	// will disconnect if a new literal is sent and edge already exists.
	if (!Builder.IsNodeInputConnected(Node.GetID(), Input.VertexID))
	{
		ExecuteAuditionableTransaction([this, &Node, &Input, &LiteralIndex](FDynamicOperatorTransactor& Transactor)
		{
			using namespace Metasound;
			using namespace Metasound::Engine;

			const FMetasoundFrontendLiteral& InputDefault = Node.InputLiterals[LiteralIndex].Value;
			TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(Input.TypeName, InputDefault);
			Transactor.SetValue(Node.GetID(), Input.Name, MoveTemp(LiteralNode));
			return true;
		});
	}
}

void UMetaSoundSourceBuilder::OnOutputAdded(int32 OutputIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, OutputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassOutput& NewOutput = GraphClass.Interface.Outputs[OutputIndex];

		Transactor.AddOutputDataSource(NewOutput.NodeID, NewOutput.Name);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemoveSwappingEdge(int32 SwapIndex, int32 LastIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
	const FMetasoundFrontendEdge& EdgeBeingRemoved = Doc.RootGraph.Graph.Edges[SwapIndex];
	ExecuteAuditionableTransaction([this, EdgeBeingRemoved](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Engine;

		const FMetaSoundFrontendDocumentBuilder& Builder = GetConstBuilder();
		const FMetasoundFrontendVertex* FromNodeOutput = Builder.FindNodeOutput(EdgeBeingRemoved.FromNodeID, EdgeBeingRemoved.FromVertexID);
		const FMetasoundFrontendVertex* ToNodeInput = Builder.FindNodeInput(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
		if (FromNodeOutput && ToNodeInput)
		{
			const FMetasoundFrontendLiteral* InputDefault = Builder.GetNodeInputDefault(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
			if (!InputDefault)
			{
				InputDefault = Builder.GetNodeInputClassDefault(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
			}

			if (ensureAlwaysMsgf(InputDefault, TEXT("Could not dynamically assign default literal upon removing edge: literal should be assigned by either the frontend document's input or the class definition")))
			{
				TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(ToNodeInput->TypeName, *InputDefault);
				Transactor.RemoveDataEdge(EdgeBeingRemoved.FromNodeID, FromNodeOutput->Name, EdgeBeingRemoved.ToNodeID, ToNodeInput->Name, MoveTemp(LiteralNode));
				return true;
			}
		}

		return false;
	});
}

void UMetaSoundSourceBuilder::OnRemovingAssetReference(TScriptInterface<IMetaSoundDocumentInterface> DocInterface)
{
	check(DocInterface.GetObject());
	GetMetaSoundSource().ReferencedAssetClassObjects.Remove(DocInterface.GetObject());
}

void UMetaSoundSourceBuilder::OnRemovingInput(int32 InputIndex)
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, InputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassInput& InputBeingRemoved = GraphClass.Interface.Inputs[InputIndex];

		UMetaSoundSource& Source = GetMetaSoundSource();
		Source.RuntimeInputData.InputMap.Remove(InputBeingRemoved.Name);

		Transactor.RemoveInputDataDestination(InputBeingRemoved.Name);

		for (uint64 AudioComponentID : LiveComponentIDs)
		{
			if (UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(AudioComponentID))
			{
				if (FAudioDevice* AudioDevice = AudioComponent->GetAudioDevice())
				{
					AudioDevice->SendCommandToActiveSounds(AudioComponentID, [InputRemoved = InputBeingRemoved.Name](FActiveSound& ActiveSound)
						{
							static_cast<FMetaSoundParameterTransmitter*>(ActiveSound.GetTransmitter())->RemoveAvailableParameter(InputRemoved);
						});
				}
			}
		}

		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemoveSwappingNode(int32 SwapIndex, int32 LastIndex) const
{
	using namespace Metasound::DynamicGraph;

	// Last index will just be re-added, so this aspect of the swap is ignored by transactor
	// (i.e. no sense removing and re-adding the node that is swapped from the end as this
	// would potentially disconnect that node in the runtime graph model).
	ExecuteAuditionableTransaction([this, SwapIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendNode& NodeBeingRemoved = GraphClass.Graph.Nodes[SwapIndex];
		const FGuid& NodeID = NodeBeingRemoved.GetID();
		Transactor.RemoveNode(NodeID);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemovingNodeInputLiteral(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendGraphClass& GraphClass = Builder.GetConstDocument().RootGraph;
	const FMetasoundFrontendNode& Node = GraphClass.Graph.Nodes[NodeIndex];
	const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

	// Only send the literal down if not connected, as the graph core layer will disconnect.
	if (!Builder.IsNodeInputConnected(Node.GetID(), Input.VertexID))
	{
		ExecuteAuditionableTransaction([this, &NodeIndex, &VertexIndex, &LiteralIndex](FDynamicOperatorTransactor& Transactor)
		{
			using namespace Metasound;
			using namespace Metasound::Engine;
			using namespace Metasound::Frontend;

			const TArray<FMetasoundFrontendNode>& Nodes = Builder.GetConstDocument().RootGraph.Graph.Nodes;
			const FMetasoundFrontendNode& Node = Nodes[NodeIndex];
			const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

			const FMetasoundFrontendLiteral* InputDefault = Builder.GetNodeInputClassDefault(Node.GetID(), Input.VertexID);
			if (ensureAlwaysMsgf(InputDefault, TEXT("Could not dynamically assign default literal from class definition upon removing input '%s' literal: document's dependency entry invalid and has no default assigned"), *Input.Name.ToString()))
			{
				TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(Input.TypeName, *InputDefault);
				Transactor.SetValue(Node.GetID(), Input.Name, MoveTemp(LiteralNode));
				return true;
			}

			return false;
		});
	}
}

void UMetaSoundSourceBuilder::OnRemovingOutput(int32 OutputIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, OutputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetConstDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassOutput& OutputBeingRemoved = GraphClass.Interface.Outputs[OutputIndex];

		Transactor.RemoveOutputDataSource(OutputBeingRemoved.Name);
		return true;
	});
}

void UMetaSoundSourceBuilder::SetBlockRateOverride(float BlockRate)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().BlockRateOverride.Default = BlockRate;
#endif //WITH_EDITORONLY_DATA
}

void UMetaSoundSourceBuilder::SetFormat(EMetaSoundOutputAudioFormat OutputFormat, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	// Convert to non-preset MetaSoundSource since interface data is being altered
	Builder.ConvertFromPreset();

	const FOutputAudioFormatInfoMap& FormatMap = GetOutputAudioFormatInfo();

	// Determine which interfaces to add and remove from the document due to the
	// output format being changed.
	TArray<FMetasoundFrontendVersion> OutputFormatsToAdd;
	if (const FOutputAudioFormatInfo* FormatInfo = FormatMap.Find(OutputFormat))
	{
		OutputFormatsToAdd.Add(FormatInfo->InterfaceVersion);
	}

	TArray<FMetasoundFrontendVersion> OutputFormatsToRemove;

	const FMetasoundFrontendDocument& Document = GetConstBuilder().GetConstDocument();
	for (const FOutputAudioFormatInfoPair& Pair : FormatMap)
	{
		const FMetasoundFrontendVersion& FormatVersion = Pair.Value.InterfaceVersion;
		if (Document.Interfaces.Contains(FormatVersion))
		{
			if (!OutputFormatsToAdd.Contains(FormatVersion))
			{
				OutputFormatsToRemove.Add(FormatVersion);
			}
		}
	}

	FModifyInterfaceOptions Options(OutputFormatsToRemove, OutputFormatsToAdd);

#if WITH_EDITORONLY_DATA
	Options.bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA

	const bool bSuccess = Builder.ModifyInterfaces(MoveTemp(Options));
	OutResult = bSuccess ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

#if WITH_EDITORONLY_DATA
void UMetaSoundSourceBuilder::SetPlatformBlockRateOverride(const FPerPlatformFloat& PlatformBlockRate)
{
	GetMetaSoundSource().BlockRateOverride = PlatformBlockRate;
}

void UMetaSoundSourceBuilder::SetPlatformSampleRateOverride(const FPerPlatformInt& PlatformSampleRate)
{
	GetMetaSoundSource().SampleRateOverride = PlatformSampleRate;
}
#endif // WITH_EDITORONLY_DATA

void UMetaSoundSourceBuilder::SetQuality(FName Quality)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().QualitySetting = Quality;
#endif //WITH_EDITORONLY_DATA	
}

void UMetaSoundSourceBuilder::SetSampleRateOverride(int32 SampleRate)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().SampleRateOverride.Default = SampleRate;
#endif //WITH_EDITORONLY_DATA	
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::CreatePatchBuilder(FName BuilderName, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Engine;

	OutResult = EMetaSoundBuilderResult::Succeeded;
	UMetaSoundPatchBuilder& NewBuilder = FDocumentBuilderRegistry::GetChecked().CreateTransientBuilder<UMetaSoundPatchBuilder>(BuilderName);
	return &NewBuilder;
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::CreateSourceBuilder(
	FName BuilderName,
	FMetaSoundBuilderNodeOutputHandle& OnPlayNodeOutput,
	FMetaSoundBuilderNodeInputHandle& OnFinishedNodeInput,
	TArray<FMetaSoundBuilderNodeInputHandle>& AudioOutNodeInputs,
	EMetaSoundBuilderResult& OutResult,
	EMetaSoundOutputAudioFormat OutputFormat,
	bool bIsOneShot)
{
	using namespace Metasound::Engine;
	using namespace Metasound::Engine::BuilderSubsystemPrivate;
	using namespace Metasound::Frontend;

	OnPlayNodeOutput = { };
	OnFinishedNodeInput = { };
	AudioOutNodeInputs.Reset();

	UMetaSoundSourceBuilder& NewBuilder = FDocumentBuilderRegistry::GetChecked().CreateTransientBuilder<UMetaSoundSourceBuilder>(BuilderName);
	OutResult = EMetaSoundBuilderResult::Succeeded;
	if (OutputFormat != EMetaSoundOutputAudioFormat::Mono)
	{
		NewBuilder.SetFormat(OutputFormat, OutResult);
	}
	
	if (OutResult == EMetaSoundBuilderResult::Succeeded)
	{
		TArray<FMetaSoundNodeHandle> AudioOutputNodes;
		if (const Metasound::Engine::FOutputAudioFormatInfoPair* FormatInfo = NewBuilder.FindOutputAudioFormatInfo())
		{
			AudioOutputNodes = NewBuilder.FindInterfaceOutputNodes(FormatInfo->Value.InterfaceVersion.Name, OutResult);
		}
		else
		{
			OutResult = EMetaSoundBuilderResult::Failed;
		}

		if (OutResult == EMetaSoundBuilderResult::Succeeded)
		{
			Algo::Transform(AudioOutputNodes, AudioOutNodeInputs, [&NewBuilder, &BuilderName](const FMetaSoundNodeHandle& AudioOutputNode) -> FMetaSoundBuilderNodeInputHandle
			{
				EMetaSoundBuilderResult Result;
				TArray<FMetaSoundBuilderNodeInputHandle> Inputs = NewBuilder.FindNodeInputs(AudioOutputNode, Result);
				if (!Inputs.IsEmpty())
				{
					return Inputs.Last();
				}

				UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find expected audio output node input vertex. Returned vertices set may be incomplete."), *BuilderName.ToString());
				return { };
			});
		}
		else
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find expected audio output format and/or associated output nodes."), *BuilderName.ToString());
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to set output format when initializing."), *BuilderName.ToString());
		return nullptr;
	}

	{
		FMetaSoundNodeHandle OnPlayNode = NewBuilder.FindGraphInputNode(SourceInterface::Inputs::OnPlay, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to add required interface '%s' when attempting to create MetaSound Source Builder"), *BuilderName.ToString(), * SourceInterface::GetVersion().ToString());
			return nullptr;
		}

		TArray<FMetaSoundBuilderNodeOutputHandle> Outputs = NewBuilder.FindNodeOutputs(OnPlayNode, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find output vertex for 'OnPlay' input node when attempting to create MetaSound Source Builder"), *BuilderName.ToString());
			return nullptr;
		}

		check(!Outputs.IsEmpty());
		OnPlayNodeOutput = Outputs.Last();
	}

	if (bIsOneShot)
	{
		FMetaSoundNodeHandle OnFinishedNode = NewBuilder.FindGraphOutputNode(SourceOneShotInterface::Outputs::OnFinished, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to add '%s' interface; interface definition may not be registered."), *BuilderName.ToString(), *SourceOneShotInterface::GetVersion().ToString());
		}

		TArray<FMetaSoundBuilderNodeInputHandle> Inputs = NewBuilder.FindNodeInputs(OnFinishedNode, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find input vertex for 'OnFinished' output node when attempting to create MetaSound Source Builder"), *BuilderName.ToString());
			return nullptr;
		}

		check(!Inputs.IsEmpty());
		OnFinishedNodeInput = Inputs.Last();
	}
	else
	{
		NewBuilder.RemoveInterface(SourceOneShotInterface::GetVersion().Name, OutResult);
	}

	return &NewBuilder;
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::CreatePatchPresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Engine;

	if (ReferencedNodeClass)
	{
		UMetaSoundPatchBuilder& Builder = FDocumentBuilderRegistry::GetChecked().CreateTransientBuilder<UMetaSoundPatchBuilder>(BuilderName);
		Builder.ConvertToPreset(ReferencedNodeClass, OutResult);
		return &Builder;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return nullptr;
}

UMetaSoundBuilderBase& UMetaSoundBuilderSubsystem::CreatePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedPatchClass, EMetaSoundBuilderResult& OutResult)
{
	const UClass& Class = ReferencedPatchClass->GetBaseMetaSoundUClass();
	if (&Class == UMetaSoundSource::StaticClass())
	{
		return *CreateSourcePresetBuilder(BuilderName, ReferencedPatchClass, OutResult);
	}
	else if (&Class == UMetaSoundPatch::StaticClass())
	{
		return *CreatePatchPresetBuilder(BuilderName, ReferencedPatchClass, OutResult);
	}
	else
	{
		checkf(false, TEXT("UClass '%s' cannot be built to a MetaSound preset"), *Class.GetFullName());
		return *NewObject<UMetaSoundPatchBuilder>();
	}
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::CreateSourcePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Engine;

	if (ReferencedNodeClass)
	{
		UMetaSoundSourceBuilder& Builder = FDocumentBuilderRegistry::GetChecked().CreateTransientBuilder<UMetaSoundSourceBuilder>();
		Builder.ConvertToPreset(ReferencedNodeClass, OutResult);
		return &Builder;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return nullptr;
}

UMetaSoundBuilderSubsystem* UMetaSoundBuilderSubsystem::Get()
{
	if (GEngine)
	{
		if (UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>())
		{
			return BuilderSubsystem;
		}
	}

	return nullptr;
}

UMetaSoundBuilderSubsystem& UMetaSoundBuilderSubsystem::GetChecked()
{
	checkf(GEngine, TEXT("Cannot access UMetaSoundBuilderSubsystem without engine loaded"));
	UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
	checkf(BuilderSubsystem, TEXT("Failed to find initialized 'UMetaSoundBuilderSubsystem"));
	return *BuilderSubsystem;
}

const UMetaSoundBuilderSubsystem* UMetaSoundBuilderSubsystem::GetConst()
{
	if (GEngine)
	{
		if (const UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<const UMetaSoundBuilderSubsystem>())
		{
			return BuilderSubsystem;
		}
	}

	return nullptr;
}

const UMetaSoundBuilderSubsystem& UMetaSoundBuilderSubsystem::GetConstChecked()
{
	checkf(GEngine, TEXT("Cannot access UMetaSoundBuilderSubsystem without engine loaded"));
	UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
	checkf(BuilderSubsystem, TEXT("Failed to find initialized 'UMetaSoundBuilderSubsystem"));
	return *BuilderSubsystem;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateBoolMetaSoundLiteral(bool Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateBoolArrayMetaSoundLiteral(const TArray<bool>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateFloatMetaSoundLiteral(float Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateFloatArrayMetaSoundLiteral(const TArray<float>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateIntMetaSoundLiteral(int32 Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateIntArrayMetaSoundLiteral(const TArray<int32>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateStringMetaSoundLiteral(const FString& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateStringArrayMetaSoundLiteral(const TArray<FString>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateObjectMetaSoundLiteral(UObject* Value)
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Value);
	return Literal;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateObjectArrayMetaSoundLiteral(const TArray<UObject*>& Value)
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Value);
	return Literal;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateMetaSoundLiteralFromParam(const FAudioParameter& Param)
{
	return FMetasoundFrontendLiteral { Param };
}

bool UMetaSoundBuilderSubsystem::DetachBuilderFromAsset(const FMetasoundFrontendClassName& InClassName) const
{
	using namespace Metasound::Frontend;
	return IDocumentBuilderRegistry::GetChecked().FinishBuilding(InClassName);
}

UMetaSoundBuilderBase* UMetaSoundBuilderSubsystem::FindBuilder(FName BuilderName)
{
	return NamedBuilders.FindRef(BuilderName);
}

UMetaSoundBuilderBase* UMetaSoundBuilderSubsystem::FindBuilderOfDocument(TScriptInterface<const IMetaSoundDocumentInterface> InMetaSound) const
{
	using namespace Metasound::Engine;

	if (const UObject* MetaSoundObject = InMetaSound.GetObject())
	{
		const FMetasoundFrontendDocument& Document = InMetaSound->GetConstDocument();
		const FMetasoundFrontendClassName& InClassName = Document.RootGraph.Metadata.GetClassName();

		return FDocumentBuilderRegistry::GetChecked().FindBuilderObject(InClassName);
	}

	return nullptr;
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::FindPatchBuilder(FName BuilderName)
{
	if (UMetaSoundBuilderBase* Builder = FindBuilder(BuilderName))
	{
		return Cast<UMetaSoundPatchBuilder>(Builder);
	}

	return nullptr;
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::FindSourceBuilder(FName BuilderName)
{
	if (UMetaSoundBuilderBase* Builder = FindBuilder(BuilderName))
	{
		return Cast<UMetaSoundSourceBuilder>(Builder);
	}

	return nullptr;
}

void UMetaSoundBuilderSubsystem::InvalidateDocumentCache(const FMetasoundFrontendClassName& InClassName) const
{
	using namespace Metasound::Engine;
	if (FMetaSoundFrontendDocumentBuilder* Builder = FDocumentBuilderRegistry::GetChecked().FindBuilder(InClassName))
	{
		Builder->Reload();
	}
}

bool UMetaSoundBuilderSubsystem::IsInterfaceRegistered(FName InInterfaceName) const
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendInterface Interface;
	return ISearchEngine::Get().FindInterfaceWithHighestVersion(InInterfaceName, Interface);
}

void UMetaSoundBuilderSubsystem::RegisterBuilder(FName BuilderName, UMetaSoundBuilderBase* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

void UMetaSoundBuilderSubsystem::RegisterPatchBuilder(FName BuilderName, UMetaSoundPatchBuilder* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

void UMetaSoundBuilderSubsystem::RegisterSourceBuilder(FName BuilderName, UMetaSoundSourceBuilder* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

bool UMetaSoundBuilderSubsystem::UnregisterBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}

bool UMetaSoundBuilderSubsystem::UnregisterPatchBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}

bool UMetaSoundBuilderSubsystem::UnregisterSourceBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}
