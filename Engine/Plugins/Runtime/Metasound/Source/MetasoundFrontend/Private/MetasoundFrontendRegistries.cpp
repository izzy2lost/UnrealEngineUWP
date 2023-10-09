// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistries.h"

#include "Algo/ForEach.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendNodeRegistryPrivate.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"

namespace Metasound
{
	namespace Frontend
	{
		namespace MetasoundFrontendRegistryPrivate
		{
			const FString& GetClassTypeString(EMetasoundFrontendClassType InType)
			{
				static const FString InputType(TEXT("Input"));
				static const FString OutputType(TEXT("Output"));
				static const FString ExternalType(TEXT("External"));
				static const FString VariableAccessorType(TEXT("VariableAccessor"));
				static const FString VariableDeferredAccessorType(TEXT("VariableDeferredAccessor"));
				static const FString VariableMutatorType(TEXT("VariableMutator"));
				static const FString VariableType(TEXT("Variable"));
				static const FString LiteralType(TEXT("Literal"));
				static const FString GraphType(TEXT("Graph"));
				static const FString TemplateType(TEXT("Template"));
				static const FString InvalidType(TEXT("Invalid"));

				switch (InType)
				{
					case EMetasoundFrontendClassType::Input:
						return InputType;

					case EMetasoundFrontendClassType::Output:
						return OutputType;

					case EMetasoundFrontendClassType::External:
						return ExternalType;

					case EMetasoundFrontendClassType::Literal:
						return LiteralType;

					case EMetasoundFrontendClassType::Variable:
						return VariableType;
						
					case EMetasoundFrontendClassType::VariableDeferredAccessor:
						return VariableDeferredAccessorType;

					case EMetasoundFrontendClassType::VariableAccessor:
						return VariableAccessorType;

					case EMetasoundFrontendClassType::VariableMutator:
						return VariableMutatorType;

					case EMetasoundFrontendClassType::Template:
						return TemplateType;

					case EMetasoundFrontendClassType::Graph:
						return GraphType;

					default:
						static_assert(static_cast<uint8>(EMetasoundFrontendClassType::Invalid) == 10, "Missing EMetasoundFrontendClassType case coverage");
						return InvalidType;
				}
			}

			// FGraphNode is used to create unique INodes based off of a IGraph. 
			//
			// Individual nodes need to reflect their InstanceName and InstanceID, but otherwise
			// they simply encapsulate a shared set of behavior. To minimize memory usage, a single
			// shared IGraph is used for all nodes referring to the same IGraph.
			class FGraphNode : public INode
			{
				// This adapter class forwards the correct FBuilderOperatorParams
				// to the graph's operator creation method. Many operator creation
				// methods downcast the supplied INode in `FBuilderOperatorParams`
				// and so it is required that it point to the correct runtime instance
				// when calling CreateOperator(...)
				class FGraphOperatorFactoryAdapter : public IOperatorFactory 
				{
				public:
					FGraphOperatorFactoryAdapter(const IGraph& InGraph)
					: Graph(&InGraph)
					, GraphFactory(InGraph.GetDefaultOperatorFactory())
					{
					}

					virtual ~FGraphOperatorFactoryAdapter() = default;

					virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
					{
						FBuildOperatorParams ForwardParams
						{
							*Graph,  // Point to correct INode instance
							InParams.OperatorSettings,
							InParams.InputData,
							InParams.Environment,
							InParams.Builder
						};
						return GraphFactory->CreateOperator(ForwardParams, OutResults);
					}
				private:
					const IGraph* Graph; // Only store pointer because owning node keeps wrapped IGraph alive.  
					FOperatorFactorySharedRef GraphFactory;
				};

			public:
				FGraphNode(const FNodeInitData& InNodeInitData, TSharedRef<const IGraph> InGraphToWrap)
				: InstanceID(InNodeInitData.InstanceID)
				, Factory(MakeShared<FGraphOperatorFactoryAdapter>(*InGraphToWrap))
				, Graph(MoveTemp(InGraphToWrap))
				{
				}

				virtual const FName& GetInstanceName() const override
				{
					// Use the instance name of underlying graph because it refers
					// to the actual asset name.
					return Graph->GetInstanceName();
				}

				virtual const FGuid& GetInstanceID() const override
				{
					return InstanceID;
				}

				virtual const FNodeClassMetadata& GetMetadata() const override
				{
					return Graph->GetMetadata();
				}

				virtual const FVertexInterface& GetVertexInterface() const override
				{
					return Graph->GetVertexInterface();
				}

				virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
				{
					// Cannot set vertex interface because of const reference to IGraph
					// Return true if the supplied interface is the same as the existing interface.
					return Graph->GetVertexInterface() == InInterface;
				}

				virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
				{
					return Graph->IsVertexInterfaceSupported(InInterface);
				}

				virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override
				{
					return Factory;
				}

			private:

				FGuid InstanceID;
				TSharedRef<FGraphOperatorFactoryAdapter> Factory;
				TSharedRef<const IGraph> Graph;
			};


			// FDocumentNodeRegistryEntry encapsulates a node registry entry for a FGraph
			class FDocumentNodeRegistryEntry : public INodeRegistryEntry
			{
			public:
				FDocumentNodeRegistryEntry(const FMetasoundFrontendGraphClass& InGraphClass, const TSet<FMetasoundFrontendVersion>& InInterfaces, const FNodeClassInfo& InNodeClassInfo, TSharedPtr<const IGraph> InGraph)
				: FrontendClass(InGraphClass)
				, Interfaces(InInterfaces)
				, ClassInfo(InNodeClassInfo)
				, Graph(InGraph)
				{
					FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
				}

				FDocumentNodeRegistryEntry(const FDocumentNodeRegistryEntry&) = default;

				virtual ~FDocumentNodeRegistryEntry() 
				{
				};

				virtual const FNodeClassInfo& GetClassInfo() const override
				{
					return ClassInfo;
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InNodeInitData) const override
				{
					if (Graph.IsValid())
					{
						return MakeUnique<FGraphNode>(InNodeInitData, Graph.ToSharedRef());
					}
					else
					{
						UE_LOG(LogMetaSound, Error, TEXT("Cannot create MetaSound node from asset %s due to prior failure to build graph"), *ClassInfo.AssetPath.ToString());
						return TUniquePtr<INode>();
					}
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override { return nullptr; }

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return FrontendClass;
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FDocumentNodeRegistryEntry>(*this);
				}

				virtual const TSet<FMetasoundFrontendVersion>* GetImplementedInterfaces() const override
				{
					return &Interfaces;
				}

				virtual bool IsNative() const override
				{
					return false;
				}

			private:

				FMetasoundFrontendClass FrontendClass;
				TSet<FMetasoundFrontendVersion> Interfaces;
				FNodeClassInfo ClassInfo;
				TSharedPtr<const IGraph> Graph;
			};

			// Builds an IGraph from a FMetasoundFrontendDocument and registers it in the node registry.
			void BuildAndRegisterGraphFromDocument(const FMetasoundFrontendDocument& InDocument, const FProxyDataCache& InProxyDataCache, const FNodeClassInfo& InNodeClassInfo)
			{
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::BuildAndRegisterGraphFromDocument);

				TUniquePtr<FFrontendGraph> FrontendGraph = FFrontendGraphBuilder::CreateGraph(InDocument, InProxyDataCache, InNodeClassInfo.AssetPath.ToString());
				if (!FrontendGraph.IsValid())
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to build MetaSound graph in asset '%s'"), *InNodeClassInfo.AssetPath.ToString());
				}

				TSharedPtr<const FGraph> GraphToRegister = MakeShareable<const FGraph>(FrontendGraph.Release());

				TUniquePtr<INodeRegistryEntry> RegistryEntry = MakeUnique<FDocumentNodeRegistryEntry>(
						InDocument.RootGraph, 
						InDocument.Interfaces, 
						InNodeClassInfo, 
						GraphToRegister);
				FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();

				FNodeRegistryKey RegistryKey = Registry.RegisterNode(MoveTemp(RegistryEntry));
				Registry.RegisterGraph(RegistryKey, MoveTemp(GraphToRegister));
			}
		}

		FRegistryContainerImpl* FRegistryContainerImpl::LazySingleton = nullptr;

		FRegistryContainerImpl& FRegistryContainerImpl::Get()
		{
			if (!LazySingleton)
			{
				LazySingleton = new Metasound::Frontend::FRegistryContainerImpl();
			}

			return *LazySingleton;
		}

		void FRegistryContainerImpl::Shutdown()
		{
			if (nullptr != LazySingleton)
			{
				delete LazySingleton;
				LazySingleton = nullptr;
			}
		}

		FRegistryContainerImpl::FRegistryContainerImpl()
		: TransactionBuffer(MakeShared<FNodeRegistryTransactionBuffer>())
		, AsyncRegistrationPipe( UE_SOURCE_LOCATION )
		{
		}

		void FRegistryContainerImpl::RegisterPendingNodes()
		{
			METASOUND_LLM_SCOPE;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FRegistryContainerImpl::RegisterPendingNodes);
			{
				FScopeLock ScopeLock(&LazyInitCommandCritSection);

				for (TUniqueFunction<void()>& Command : LazyInitCommands)
				{
					Command();
				}

				LazyInitCommands.Empty();
			}

			// Prime search engine after bulk registration.
			ISearchEngine::Get().Prime();
		}

		bool FRegistryContainerImpl::EnqueueInitCommand(TUniqueFunction<void()>&& InFunc)
		{

			FScopeLock ScopeLock(&LazyInitCommandCritSection);
			if (LazyInitCommands.Num() >= MaxNumNodesAndDatatypesToInitialize)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Registering more that %d nodes and datatypes for metasounds! Consider increasing MetasoundFrontendRegistryContainer::MaxNumNodesAndDatatypesToInitialize."), MaxNumNodesAndDatatypesToInitialize);
			}

			LazyInitCommands.Add(MoveTemp(InFunc));
			return true;
		}

		void FRegistryContainerImpl::SetObjectReferencer(TUniquePtr<IObjectReferencer> InReferencer)
		{
			ObjectReferencer = MoveTemp(InReferencer);
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				return Entry->CreateNode(InInitData);
			}

			// Because creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. only log error and don't throw ensure to avoid blocking start-up if assets are missing. All other
			// CreateNode calls are natively managed and thus better suited to throw ensures.
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey);
			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TArray<::Metasound::Frontend::FConverterNodeInfo> FRegistryContainerImpl::GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType)
		{
			FConverterNodeRegistryKey InKey = { FromDataType, ToDataType };
			if (!ConverterNodeRegistry.Contains(InKey))
			{
				return TArray<FConverterNodeInfo>();
			}
			else
			{
				return ConverterNodeRegistry[InKey].PotentialConverterNodes;
			}
		}

		TUniquePtr<FNodeRegistryTransactionStream> FRegistryContainerImpl::CreateTransactionStream()
		{
			return MakeUnique<FNodeRegistryTransactionStream>(TransactionBuffer);
		}

		FNodeRegistryKey FRegistryContainerImpl::RegisterGraph(const FNodeClassInfo& InNodeClassInfo, const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync)
		{
			using namespace UE;

			check(InDocumentInterface);
			check(IsInGameThread());

			const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
			FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(Document.RootGraph);

			// Proxies are created synchronously to avoid creating proxies in async tasks. Proxies
			// are created from UObjects which need to be protected from GC and non-GT access.
			FProxyDataCache ProxyDataCache;
			ProxyDataCache.CreateAndCacheProxies(Document);

			if (bAsync)
			{	
				// Wait for any async tasks that are in flight which correspond to the same graph
				FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
				if (const FActiveRegistrationTaskInfo* ActiveTaskInfo = ActiveRegistrationTasks.Find(RegistryKey))
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Waiting for async registration task to finish before beginning new registration task for same registration key (%s) with asset (%s))"), *RegistryKey, *InNodeClassInfo.AssetPath.ToString());
					ActiveTaskInfo->Task.Wait(FTimespan::FromSeconds(1.));
				}
			
				Tasks::FTask BuildAndRegisterTask = AsyncRegistrationPipe.Launch(
					UE_SOURCE_LOCATION,
					[RegistryKey, NodeClassInfo=InNodeClassInfo, DocumentInterface=InDocumentInterface, ProxyDataCache=MoveTemp(ProxyDataCache)]()
					{
						// Build and add the graph to the node registry
						MetasoundFrontendRegistryPrivate::BuildAndRegisterGraphFromDocument(DocumentInterface->GetConstDocument(), ProxyDataCache, NodeClassInfo);

						// cleanup async task tracking
						{
							FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();

							FScopeLock LockActiveReg(&Registry.ActiveRegistrationTasksCriticalSection);
							if (Registry.ObjectReferencer)
							{
								Registry.ObjectReferencer->RemoveObject(DocumentInterface.GetObject());
							}

							int32 NumRemoved = Registry.ActiveRegistrationTasks.Remove(RegistryKey);
							if (NumRemoved != 1)
							{
								UE_LOG(LogMetaSound, Warning, TEXT("Multiple active registration tasks for the same asset may result in incorrect MetaSound graphs being instantiated during runtime. Registry Key: (%s) Asset Path: (%s)"), *RegistryKey, *NodeClassInfo.AssetPath.ToString());
							}
						}
					}
				);

				TObjectPtr<UObject> OwningObject = InDocumentInterface.GetObject();
				if (ObjectReferencer)
				{
					ObjectReferencer->AddObject(OwningObject);
				}
				ActiveRegistrationTasks.Add(RegistryKey, FActiveRegistrationTaskInfo{BuildAndRegisterTask, OwningObject});
			}
			else
			{
				// Build and register graph synchronously
				MetasoundFrontendRegistryPrivate::BuildAndRegisterGraphFromDocument(InDocumentInterface->GetConstDocument(), ProxyDataCache, InNodeClassInfo);
			}

			return RegistryKey;
		}

		void FRegistryContainerImpl::RegisterGraph(const FNodeRegistryKey& InKey, TSharedPtr<const FGraph> InGraph)
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);

			if (const TSharedPtr<const FGraph>* ExistingGraph = RegisteredGraphs.Find(InKey))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Multiple graphs are registered with the same registry key (%s). The existing registered graph (%s) will be replaced with the new graph (%s)."),  *InKey, *((*ExistingGraph)->GetInstanceName().ToString()), *(InGraph->GetInstanceName().ToString()));
			}

			RegisteredGraphs.Add(InKey, MoveTemp(InGraph));
		}

		TSharedPtr<const Metasound::FGraph> FRegistryContainerImpl::GetGraph(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const
		{
			WaitForAsyncGraphRegistration(InRegistryKey);

			TSharedPtr<const FGraph> Graph;
			{
				FScopeLock Lock(&RegistryMapsCriticalSection);
				if (const TSharedPtr<const FGraph>* RegisteredGraph = RegisteredGraphs.Find(InRegistryKey))
				{
					Graph = *RegisteredGraph;
				}
			}

			if (!Graph)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find graph with registry key (%s)."),  *InRegistryKey);
			}

			return Graph;
		}

		FNodeRegistryKey FRegistryContainerImpl::RegisterNode(TUniquePtr<INodeRegistryEntry>&& InEntry)
		{
			METASOUND_LLM_SCOPE;

			FNodeRegistryKey Key;

			if (InEntry.IsValid())
			{
				TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

				FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				Key = NodeRegistryKey::CreateKey(Entry->GetClassInfo());

				{
					FScopeLock Lock(&RegistryMapsCriticalSection);

					// check to see if an identical node was already registered, and log
					if (RegisteredNodes.Contains(Key))
					{
						TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe> Node = RegisteredNodes[Key];
						const FNodeClassInfo& ClassInfo = Node->GetClassInfo();
						UE_LOG(LogMetaSound, Error, TEXT("Node with registry key '%s' already registered by asset '%s'. The previously registered node will be overwritten. " 
							"This can happen if two classes share the same name or if METASOUND_REGISTER_NODE is defined in a public header."
						), *Key, *ClassInfo.AssetPath.ToString());
					}

					// Store registry elements in map so nodes can be queried using registry key.
					RegisteredNodes.Add(Key, Entry);
				}

				// Store update to newly registered node in history so nodes
				// can be queried by transaction ID
				
				TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));
			}

			return Key;
		}

		FNodeRegistryKey FRegistryContainerImpl::RegisterNodeTemplate(TUniquePtr<INodeRegistryTemplateEntry>&& InEntry)
		{
			METASOUND_LLM_SCOPE;

			FNodeRegistryKey Key;

			if (InEntry.IsValid())
			{
				TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

				FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				Key = NodeRegistryKey::CreateKey(Entry->GetClassInfo());

				{
					FScopeLock Lock(&RegistryMapsCriticalSection);
					// check to see if an identical node was already registered, and log
					ensureAlwaysMsgf(
						!RegisteredNodeTemplates.Contains(Key),
						TEXT("Node template with registry key '%s' already registered. The previously registered node will be overwritten."),
						*Key);


					// Store registry elements in map so nodes can be queried using registry key.
					RegisteredNodeTemplates.Add(Key, Entry);
				}

				// Store update to newly registered node in history so nodes
				// can be queried by transaction ID

				TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));
			}

			return Key;
		}

		bool FRegistryContainerImpl::UnregisterNode(const FNodeRegistryKey& InKey)
		{
			METASOUND_LLM_SCOPE;
			
			check(IsInGameThread());
			if (NodeRegistryKey::IsValid(InKey))
			{
				if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
				{
					FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

					TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

					{
						FScopeLock Lock(&RegistryMapsCriticalSection);
						RegisteredNodes.Remove(InKey);
						RegisteredGraphs.Remove(InKey);
					}
					return true;
				}
			}

			return false;
		}

		bool FRegistryContainerImpl::UnregisterNodeTemplate(const FNodeRegistryKey& InKey)
		{
			METASOUND_LLM_SCOPE;

			if (NodeRegistryKey::IsValid(InKey))
			{
				if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
				{
					FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

					TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

					{
						FScopeLock Lock(&RegistryMapsCriticalSection);
						RegisteredNodeTemplates.Remove(InKey);
					}
					return true;
				}
			}

			return false;
		}

		bool FRegistryContainerImpl::RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo)
		{
			if (!ConverterNodeRegistry.Contains(InNodeKey))
			{
				ConverterNodeRegistry.Add(InNodeKey);
			}

			FConverterNodeRegistryValue& ConverterNodeList = ConverterNodeRegistry[InNodeKey];

			if (ensureAlways(!ConverterNodeList.PotentialConverterNodes.Contains(InNodeInfo)))
			{
				ConverterNodeList.PotentialConverterNodes.Add(InNodeInfo);
				return true;
			}
			else
			{
				// If we hit this, someone attempted to add the same converter node to our list multiple times.
				return false;
			}
		}

		bool FRegistryContainerImpl::IsNodeRegistered(const FNodeRegistryKey& InKey) const
		{
			WaitForAsyncGraphRegistration(InKey);

			{
				FScopeLock Lock(&RegistryMapsCriticalSection);
				return RegisteredNodes.Contains(InKey) || RegisteredNodeTemplates.Contains(InKey);
			}
		}

		bool FRegistryContainerImpl::IsNodeNative(const FNodeRegistryKey& InKey) const
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				return Entry->IsNative();
			}

			if (const INodeRegistryTemplateEntry* TemplateEntry = FindNodeTemplateEntry(InKey))
			{
				return true;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				OutClass = Entry->GetFrontendClass();
				return true;
			}

			if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				OutClass = Entry->GetFrontendClass();
				return true;
			}

			return false;
		}

		const TSet<FMetasoundFrontendVersion>* FRegistryContainerImpl::FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey) const
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				return Entry->GetImplementedInterfaces();
			}

			return nullptr;
		}

		bool FRegistryContainerImpl::FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				OutInfo = Entry->GetClassInfo();
				return true;
			}

			if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				OutInfo = Entry->GetClassInfo();
				return true;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			switch (InAccessType)
			{
				case EMetasoundFrontendVertexAccessType::Reference:
				{
					if (IDataTypeRegistry::Get().GetFrontendInputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				case EMetasoundFrontendVertexAccessType::Value:
				{
					if (IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				default:
				case EMetasoundFrontendVertexAccessType::Unset:
				{
					return false;
				}
				break;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			if (IDataTypeRegistry::Get().GetFrontendLiteralClass(InDataTypeName, Class))
			{
				OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
				return true;
			}
			return false;
		}

		bool FRegistryContainerImpl::FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			switch (InAccessType)
			{
				case EMetasoundFrontendVertexAccessType::Reference:
				{
					if (IDataTypeRegistry::Get().GetFrontendOutputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				case EMetasoundFrontendVertexAccessType::Value:
				{
					if (IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;
			}

			return false;
		}

		void FRegistryContainerImpl::IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType) const
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Calling FMetasoundRegistryContainer::IterateRegistry(...) is not threadsafe. Please use Metasound::Frontend::ISearchEngine instead"));
			auto WrappedFunc = [&](const TPair<FNodeRegistryKey, TSharedPtr<INodeRegistryEntry, ESPMode::ThreadSafe>>& Pair)
			{
				InIterFunc(Pair.Value->GetFrontendClass());
			};

			if (EMetasoundFrontendClassType::Invalid == InClassType)
			{
				// Iterate through all classes. 
				Algo::ForEach(RegisteredNodes, WrappedFunc);
			}
			else
			{
				// Only call function on classes of certain type.
				auto IsMatchingClassType = [&](const TPair<FNodeRegistryKey, TSharedPtr<INodeRegistryEntry, ESPMode::ThreadSafe>>& Pair)
				{
					return Pair.Value->GetClassInfo().Type == InClassType;
				};
				Algo::ForEachIf(RegisteredNodes, IsMatchingClassType, WrappedFunc);
			}
		}

		const INodeRegistryEntry* FRegistryContainerImpl::FindNodeEntry(const FNodeRegistryKey& InKey) const
		{
			WaitForAsyncGraphRegistration(InKey);

			{
				// This scope lock proctects against race conditions manipulating the `RegisteredNodes` map, but it does not
				// protect against the individual INodeRegistryEntry from being removed after this function returns. Generally
				// this is not an issue as the acess to the INodeRegistryEntry pointer happens on the same thread as calls to 
				// remove node registry entries. 
				FScopeLock Lock(&RegistryMapsCriticalSection);
				if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodes.Find(InKey))
				{
					return &Entry->Get();
				}
			}

			return nullptr;
		}


		const INodeRegistryTemplateEntry* FRegistryContainerImpl::FindNodeTemplateEntry(const FNodeRegistryKey& InKey) const
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodeTemplates.Find(InKey))
			{
				return &Entry->Get();
			}

			return nullptr;
		}

		void FRegistryContainerImpl::WaitForAsyncGraphRegistration(const FNodeRegistryKey& InRegistryKey) const
		{
			UE::Tasks::FTask ActiveRegistrationTask;
			{
				FScopeLock Lock(&ActiveRegistrationTasksCriticalSection);
				if (const FActiveRegistrationTaskInfo* FoundTask = ActiveRegistrationTasks.Find(InRegistryKey))
				{
					ActiveRegistrationTask = FoundTask->Task;
				}
			}

			if (ActiveRegistrationTask.IsValid())
			{
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::WaitForRegistrationTaskToComplete);
				ActiveRegistrationTask.Wait();
			}
		}

		FNodeRegistryTransaction::FNodeRegistryTransaction(ETransactionType InType, const FNodeClassInfo& InNodeClassInfo, FNodeRegistryTransaction::FTimeType InTimestamp)
		: Type(InType)
		, NodeClassInfo(InNodeClassInfo)
		, Timestamp(InTimestamp)
		{
		}

		FNodeRegistryTransaction::ETransactionType FNodeRegistryTransaction::GetTransactionType() const
		{
			return Type;
		}

		const FNodeClassInfo& FNodeRegistryTransaction::GetNodeClassInfo() const
		{
			return NodeClassInfo;
		}

		FNodeRegistryKey FNodeRegistryTransaction::GetNodeRegistryKey() const
		{
			return NodeRegistryKey::CreateKey(NodeClassInfo);
		}

		FNodeRegistryTransaction::FTimeType FNodeRegistryTransaction::GetTimestamp() const
		{
			return Timestamp;
		}

		namespace NodeRegistryKey
		{
			// All registry keys should be created through this function to ensure consistency.
			FNodeRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion)
			{
				using namespace MetasoundFrontendRegistryPrivate;
				if (InType == EMetasoundFrontendClassType::Graph)
				{
					// No graphs are registered. Any registered graph should be registered as an external node.
					InType = EMetasoundFrontendClassType::External;
				}
				const FString RegistryKey = FString::Format(TEXT("{0}_{1}_{2}.{3}"), {*GetClassTypeString(InType), *InFullClassName, InMajorVersion, InMinorVersion});
				return RegistryKey;
			}

			const FNodeRegistryKey& GetInvalid()
			{
				static const FNodeRegistryKey InvalidKey;
				return InvalidKey;
			}

			bool IsValid(const FNodeRegistryKey& InKey)
			{
				return InKey != GetInvalid();
			}

			bool IsEqual(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS)
			{
				return InLHS == InRHS;
			}

			bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
			{
				if (InLHS.GetClassName() == InRHS.GetClassName())
				{
					if (InLHS.GetType() == InRHS.GetType())
					{
						if (InLHS.GetVersion() == InRHS.GetVersion())
						{
							return true;
						}
					}
				}
				return false;
			}

			bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
			{
				if (InLHS.ClassName == InRHS.GetClassName())
				{
					if (InLHS.Type == InRHS.GetType())
					{
						if (InLHS.Version == InRHS.GetVersion())
						{
							return true;
						}
					}
				}
				return false;
			}

			FNodeRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata)
			{
				return CreateKey(EMetasoundFrontendClassType::External, InNodeMetadata.ClassName.GetFullName().ToString(), InNodeMetadata.MajorVersion, InNodeMetadata.MinorVersion);
			}

			FNodeRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
			{
				checkf(InNodeMetadata.GetType() != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use CreateKey overload that is provided FMetasoundFrontendGraphClass"));
				return CreateKey(InNodeMetadata.GetType(), InNodeMetadata.GetClassName().GetFullName().ToString(), InNodeMetadata.GetVersion().Major, InNodeMetadata.GetVersion().Minor);
			}

			FNodeRegistryKey CreateKey(const FMetasoundFrontendGraphClass& InGraphClass)
			{
				return CreateKey(
					EMetasoundFrontendClassType::External, // Overridden as all graphs are considered the same as an external in the registry
					InGraphClass.Metadata.GetClassName().GetFullName().ToString(),
					InGraphClass.Metadata.GetVersion().Major,
					InGraphClass.Metadata.GetVersion().Minor
				);
			}

			FNodeRegistryKey CreateKey(const FNodeClassInfo& InClassInfo)
			{
				checkf(InClassInfo.Type != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use CreateKey overload that is provided FMetasoundFrontendGraphClass"));
				return CreateKey(InClassInfo.Type, InClassInfo.ClassName.GetFullName().ToString(), InClassInfo.Version.Major, InClassInfo.Version.Minor);
			}
		}

		FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendClassMetadata& InMetadata)
			: ClassName(InMetadata.GetClassName())
			, Type(InMetadata.GetType())
			, Version(InMetadata.GetVersion())
		{
		}

		FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FSoftObjectPath& InAssetPath)
			: ClassName(InClass.Metadata.GetClassName())
			, Type(EMetasoundFrontendClassType::External) // Overridden as it is considered the same as an external class in the registry
			, AssetClassID(FGuid(ClassName.Name.ToString()))
			, AssetPath(InAssetPath)
			, Version(InClass.Metadata.GetVersion())
		{
			ensure(!AssetPath.IsNull());
#if WITH_EDITORONLY_DATA
			for (const FMetasoundFrontendClassInput& Input : InClass.Interface.Inputs)
			{
				InputTypes.Add(Input.TypeName);
			}

			for (const FMetasoundFrontendClassOutput& Output : InClass.Interface.Outputs)
			{
				OutputTypes.Add(Output.TypeName);
			}

			bIsPreset = InClass.PresetOptions.bIsPreset;
#endif // WITH_EDITORONLY_DATA
		}
	} // namespace Frontend
} // namespace Metasound


FMetasoundFrontendRegistryContainer* FMetasoundFrontendRegistryContainer::Get()
{
	return &Metasound::Frontend::FRegistryContainerImpl::Get();
}

void FMetasoundFrontendRegistryContainer::ShutdownMetasoundFrontend()
{
	Metasound::Frontend::FRegistryContainerImpl::Shutdown();
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassInfo& InClassInfo)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InClassInfo);
}

bool FMetasoundFrontendRegistryContainer::GetFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
{
	FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get();

	if (ensure(nullptr != Registry))
	{
		return Registry->FindFrontendClassFromRegistered(InKey, OutClass);
	}

	return false;
}

bool FMetasoundFrontendRegistryContainer::GetNodeClassInfoFromRegistered(const FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindNodeClassInfoFromRegistered(InKey, OutInfo);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindInputNodeRegistryKeyForDataType(InDataTypeName, InAccessType, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindVariableNodeRegistryKeyForDataType(InDataTypeName, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InVertexAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindOutputNodeRegistryKeyForDataType(InDataTypeName, InVertexAccessType, OutKey);
	}
	return false;
}
