// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistryContainerImpl.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
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
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"

bool bBusyWaitOnAsyncRegistrationTasks = true;
static FAutoConsoleVariableRef CVarAsyncRegistrationTasksBusyWait(
	TEXT("au.MetaSound.BusyWaitOnAsyncRegistrationTasks"),
	bBusyWaitOnAsyncRegistrationTasks,
	TEXT("Use TaskGraph BusyWait instead of simple Wait. Required to avoid hangs on platforms with low number of cores."),
	ECVF_Default);

namespace Metasound::Frontend
{
	namespace RegistryPrivate
	{
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
			FDocumentNodeRegistryEntry(const FMetasoundFrontendGraphClass& InGraphClass, const TSet<FMetasoundFrontendVersion>& InInterfaces, FNodeClassInfo&& InNodeClassInfo, TSharedPtr<const IGraph> InGraph)
			: FrontendClass(InGraphClass)
			, Interfaces(InInterfaces)
			, ClassInfo(MoveTemp(InNodeClassInfo))
			, Graph(InGraph)
			{
				FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
			}

			FDocumentNodeRegistryEntry(const FDocumentNodeRegistryEntry&) = default;

			virtual ~FDocumentNodeRegistryEntry() = default;

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
	} // namespace RegistryPrivate

	void FRegistryContainerImpl::BuildAndRegisterGraphFromDocument(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface, const FProxyDataCache& InProxyDataCache, FNodeClassInfo&& InNodeClassInfo)
	{
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::BuildAndRegisterGraphFromDocument);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Metasound::FRegistryContainerImpl::BuildAndRegisterGraphFromDocument asset %s"), *InNodeClassInfo.AssetPath.ToString()));

		const FMetasoundFrontendDocument& Document = DocumentInterface->GetConstDocument();
		TUniquePtr<FFrontendGraph> FrontendGraph = FFrontendGraphBuilder::CreateGraph(Document, InProxyDataCache, InNodeClassInfo.AssetPath.ToString());
		if (!FrontendGraph.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to build MetaSound graph in asset '%s'"), *InNodeClassInfo.AssetPath.ToString());
		}

		TSharedPtr<const FGraph> GraphToRegister = MakeShareable<const FGraph>(FrontendGraph.Release());

		TUniquePtr<INodeRegistryEntry> RegistryEntry = MakeUnique<FDocumentNodeRegistryEntry>(Document.RootGraph, Document.Interfaces, MoveTemp(InNodeClassInfo), GraphToRegister);

		const FNodeRegistryKey RegistryKey = RegisterNodeInternal(MoveTemp(RegistryEntry));
		RegisterGraphInternal(RegistryKey, DocumentInterface->GetAssetPathChecked(), GraphToRegister);
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
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		checkf(ActiveRegistrationTasks.IsEmpty(), TEXT("Object Referencer cannot be set while registry is actively being manipulated"));
		ObjectReferencer = MoveTemp(InReferencer);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InInitData](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(InInitData); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			// Creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. 
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry) 
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams));
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
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

	FNodeRegistryKey FRegistryContainerImpl::RegisterGraph(const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync)
	{
		using namespace UE;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::RegisterGraph);

		check(InDocumentInterface);
		check(IsInGameThread());

		UObject* OwningObject = InDocumentInterface.GetObject();
		check(OwningObject);
		const FTopLevelAssetPath AssetPath = InDocumentInterface->GetAssetPathChecked();

		const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
		const FNodeRegistryKey RegistryKey(Document.RootGraph);

		if (!RegistryKey.IsValid())
		{
			// Do not attempt to build and register a MetaSound with an invalid registry key
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attemping to register graph for asset %s"), *AssetPath.ToString());
			return RegistryKey;
		}

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::RegisterGraph key:%s, asset %s"), *RegistryKey.ToString(), *AssetPath.ToString()));

		FNodeClassInfo NodeClassInfo(Document.RootGraph, AssetPath);

		// Proxies are created synchronously to avoid creating proxies in async tasks. Proxies
		// are created from UObjects which need to be protected from GC and non-GT access.
		FProxyDataCache ProxyDataCache;
		ProxyDataCache.CreateAndCacheProxies(Document);

		// Store update to newly registered node in history so nodes
		// can be queried by transaction ID
		{
			FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, NodeClassInfo, Timestamp));
		}

		if (bAsync)
		{
			// Wait for any async tasks that are in flight which correspond to the same graph
			WaitForAsyncRegistrationInternal(RegistryKey, &AssetPath);

			Tasks::FTask BuildAndRegisterTask = AsyncRegistrationPipe.Launch(
				UE_SOURCE_LOCATION,
				[RegistryKey, ClassInfo = MoveTemp(NodeClassInfo), AssetPath, DocumentInterface = InDocumentInterface, ProxyDataCache = MoveTemp(ProxyDataCache)]() mutable
				{
					FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();
					// Unregister the graph before re-registering
					if (Registry.IsGraphRegistered(RegistryKey, AssetPath))
					{
						Registry.UnregisterGraphInternal(RegistryKey, AssetPath);
					}

					Registry.BuildAndRegisterGraphFromDocument(DocumentInterface, ProxyDataCache, MoveTemp(ClassInfo));
					Registry.RemoveRegistrationTask(RegistryKey, FNodeRegistryTransaction::ETransactionType::NodeRegistration, DocumentInterface);
				}
			);

			AddRegistrationTask(RegistryKey, *OwningObject, FActiveRegistrationTaskInfo
			{
				FNodeRegistryTransaction::ETransactionType::NodeRegistration,
				BuildAndRegisterTask,
				AssetPath
			});
		}
		else
		{
			if (IsGraphRegistered(RegistryKey, AssetPath))
			{
				UnregisterGraphInternal(RegistryKey, AssetPath);
			}

			// Build and register graph synchronously
			BuildAndRegisterGraphFromDocument(InDocumentInterface, ProxyDataCache, MoveTemp(NodeClassInfo));
		}

		return RegistryKey;
	}

	void FRegistryContainerImpl::AddRegistrationTask(const FNodeRegistryKey& InKey, UObject& InObject, FActiveRegistrationTaskInfo&& TaskInfo)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		if (ObjectReferencer)
		{
			ObjectReferencer->AddObject(&InObject);
		}

		ActiveRegistrationTasks.FindOrAdd(InKey).Add(MoveTemp(TaskInfo));
	}

	void FRegistryContainerImpl::RemoveRegistrationTask(const FNodeRegistryKey& InKey, FNodeRegistryTransaction::ETransactionType TransactionType, const TScriptInterface<IMetaSoundDocumentInterface>& DocumentInterface)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);

		if (TArray<FActiveRegistrationTaskInfo>* TaskInfos = ActiveRegistrationTasks.Find(InKey))
		{
			const FTopLevelAssetPath Path = DocumentInterface->GetAssetPathChecked();
			constexpr bool bAllowShrinking = false;
			const int32 NumRemoved = TaskInfos->RemoveAllSwap([&Path, &TransactionType](const FActiveRegistrationTaskInfo& Info)
			{
				return Info.AssetPath == Path && Info.TransactionType == TransactionType;
			}, bAllowShrinking);

			if (NumRemoved != 1)
			{
				UE_LOG(LogMetaSound, Warning,
					TEXT("Multiple active %s tasks for the same asset may result in incorrect MetaSound graphs being instantiated during runtime. "
						"Registry Key: (%s) "
						"Asset Path: (%s)"),
					*FNodeRegistryTransaction::LexToString(TransactionType),
					*InKey.ToString(),
					*Path.ToString());
			}

			if (TaskInfos->IsEmpty())
			{
				ActiveRegistrationTasks.Remove(InKey);
			}
		}

		if (ObjectReferencer)
		{
			if (UObject* Object = DocumentInterface.GetObject())
			{
				ObjectReferencer->RemoveObject(Object);
			}
		}
	}

	void FRegistryContainerImpl::RegisterGraphInternal(const FNodeRegistryKey& InKey, const FTopLevelAssetPath& InAssetPath, TSharedPtr<const FGraph> InGraph)
	{
		FScopeLock Lock(&RegistryMapsCriticalSection);

		FGraphRegistryKey GraphKey(InKey, InAssetPath);
		if (RegisteredGraphs.Contains(GraphKey))
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Graph is already registered with the same registry key ('%s, %s'). The existing registered graph will be replaced with the new graph."),  *InKey.ToString(), *InAssetPath.ToString());
		}

		RegisteredGraphs.Add(GraphKey, InGraph);
	}

	bool FRegistryContainerImpl::UnregisterGraphInternal(const FNodeRegistryKey& InKey, const FTopLevelAssetPath& InAssetPath)
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::UnregisterGraphInternal key:%s, asset %s"), *InKey.ToString(), *InAssetPath.ToString()));

		FScopeLock Lock(&RegistryMapsCriticalSection);
		{
			const int32 NumRemoved = RegisteredGraphs.Remove(FRegistryContainerImpl::FGraphRegistryKey(InKey, InAssetPath));
			if (NumRemoved != 0)
			{
				UnregisterNodeInternal(InKey);
				return true;
			}
		}

		return false;
	}

	bool FRegistryContainerImpl::UnregisterGraph(const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync)
	{
		using namespace UE;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::UnregisterGraph);

		UObject* OwningObject = InDocumentInterface.GetObject();
		check(OwningObject);
		check(InDocumentInterface);
		check(IsInGameThread());

		const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
		const FNodeRegistryKey RegistryKey(Document.RootGraph);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::UnregisterGraph key %s object %s"), *RegistryKey.ToString(), *OwningObject->GetPathName()));

		// Do not attempt to unregister a MetaSound with an invalid registry key
		if (!RegistryKey.IsValid())
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attempting to unregister graph for object %s"), *OwningObject->GetFullName());
			return false;
		}

		// This is a hack to avoid requiring the asset path to be passed while unregistering.
		// The asset path may be invalid by this point if the object being unregistered is being GC'ed.
		// FNodeClassInfo needs to be deprecated in favor of more precise types as a key, editor data, etc.
		// Its currently kind of a dumping ground.
		FNodeClassInfo NodeClassInfo(Document.RootGraph.Metadata);
		NodeClassInfo.Type = EMetasoundFrontendClassType::External;

		// Store update to unregistered node in history so nodes can be queried by transaction ID
		{
			FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, NodeClassInfo, Timestamp));
		}

		if (bAsync)
		{
			Tasks::FTask UnregisterTask = AsyncRegistrationPipe.Launch(
				UE_SOURCE_LOCATION,
				[RegistryKey, AssetPath = InDocumentInterface->GetAssetPathChecked(), DocumentInterface = InDocumentInterface]()
				{
					FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();
					Registry.UnregisterGraphInternal(RegistryKey, AssetPath);
					Registry.RemoveRegistrationTask(RegistryKey, FNodeRegistryTransaction::ETransactionType::NodeUnregistration, DocumentInterface);
				});

			AddRegistrationTask(RegistryKey, *OwningObject, FActiveRegistrationTaskInfo
			{
				FNodeRegistryTransaction::ETransactionType::NodeUnregistration,
				UnregisterTask,
				InDocumentInterface->GetAssetPathChecked()
			});
		}
		else
		{
			UnregisterGraphInternal(RegistryKey, InDocumentInterface->GetAssetPathChecked());
		}

		return true;
	}

	TSharedPtr<const Metasound::FGraph> FRegistryContainerImpl::GetGraph(const FNodeRegistryKey& InNodeRegistryKey, const FTopLevelAssetPath& InAssetPath) const
	{
		WaitForAsyncGraphRegistration(InNodeRegistryKey, InAssetPath);

		TSharedPtr<const FGraph> Graph;
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedPtr<const FGraph>* RegisteredGraph = RegisteredGraphs.Find(FGraphRegistryKey { InNodeRegistryKey, InAssetPath }))
			{
				Graph = *RegisteredGraph;
			}
		}

		if (!Graph)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find graph with registry key (%s) and asset (%s)."),  *InNodeRegistryKey.ToString(), *InAssetPath.ToString());
		}

		return Graph;
	}

	FNodeRegistryKey FRegistryContainerImpl::RegisterNodeInternal(TUniquePtr<INodeRegistryEntry>&& InEntry)
	{
		METASOUND_LLM_SCOPE;

		FNodeRegistryKey Key;

		if (InEntry.IsValid())
		{
			TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

			Key = FNodeRegistryKey(Entry->GetClassInfo());
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::RegisterNodeInternal with key %s"), *Key.ToString()))
			{
				FScopeLock Lock(&RegistryMapsCriticalSection);

				// check to see if an identical node was already registered, and log
				if (const TSharedRef<INodeRegistryEntry>* ExistingEntry = RegisteredNodes.Find(Key))
				{
					const FNodeClassInfo& ClassInfo = (*ExistingEntry)->GetClassInfo();
					UE_LOG(LogMetaSound, Error,
						TEXT("Node with registry key '%s' already registered by asset '%s' encountered while registering node with asset %s. MetaSounds which depend on these assets may utilize incorrect asset."),
						*Key.ToString(),
						*ClassInfo.AssetPath.ToString(),
						*Entry->GetClassInfo().AssetPath.ToString());
				}

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodes.Add(Key, Entry);
			}
		}

		return Key;
	}

	FNodeRegistryKey FRegistryContainerImpl::RegisterNode(TUniquePtr<INodeRegistryEntry>&& InEntry)
	{
		const FNodeClassInfo ClassInfo = InEntry->GetClassInfo();
		const FNodeRegistryKey Key = RegisterNodeInternal(MoveTemp(InEntry));

		if (Key.IsValid())
		{
			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID
			const FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, ClassInfo, Timestamp));
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

			Key = FNodeRegistryKey(Entry->GetClassInfo());

			{
				FScopeLock Lock(&RegistryMapsCriticalSection);
				// check to see if an identical node was already registered, and log
				ensureAlwaysMsgf(
					!RegisteredNodeTemplates.Contains(Key),
					TEXT("Node template with registry key '%s' already registered. The previously registered node will be overwritten."),
					*Key.ToString());

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodeTemplates.Add(Key, Entry);
			}

			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID

			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));
		}

		return Key;
	}

	bool FRegistryContainerImpl::UnregisterNodeInternal(const FNodeRegistryKey& InKey, FNodeClassInfo* OutClassInfo)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::UnregisterNodeInternal key %s"), *InKey.ToString()))

			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* EntryPtr = RegisteredNodes.Find(InKey))
			{
				const TSharedRef<INodeRegistryEntry>& Entry = *EntryPtr;
				if (OutClassInfo)
				{
					*OutClassInfo = Entry->GetClassInfo();
				}
				const uint32 NumRemoved = RegisteredNodes.RemoveSingle(InKey, Entry);
				if (ensure(NumRemoved == 1))
				{
					return true;
				}
			}
		}

		if (OutClassInfo)
		{
			*OutClassInfo = { };
		}
		return false;
	}

	bool FRegistryContainerImpl::UnregisterNode(const FNodeRegistryKey& InKey)
	{
		FNodeClassInfo ClassInfo;
		if (UnregisterNodeInternal(InKey, &ClassInfo))
		{
			const FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, ClassInfo, Timestamp));

			return true;
		}

		return false;
	}

	bool FRegistryContainerImpl::UnregisterNodeTemplate(const FNodeRegistryKey& InKey)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
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
		auto IsNodeRegisteredInternal = [this, &InKey]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			return RegisteredNodes.Contains(InKey) || RegisteredNodeTemplates.Contains(InKey);
		};

		if (IsNodeRegisteredInternal())
		{
			return true;
		}
		else
		{
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return IsNodeRegisteredInternal();
		}
	}

	bool FRegistryContainerImpl::IsGraphRegistered(const FNodeRegistryKey& InKey, const FTopLevelAssetPath& InAssetPath) const
	{
		WaitForAsyncGraphRegistration(InKey, InAssetPath);

		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			FGraphRegistryKey GraphRegistryKey { InKey, InAssetPath };
			return RegisteredGraphs.Contains(GraphRegistryKey);
		}
	}

	bool FRegistryContainerImpl::IsNodeNative(const FNodeRegistryKey& InKey) const
	{
		bool bIsNative = false;
		auto SetIsNative = [&bIsNative](const INodeRegistryEntry& Entry) 
		{ 
			bIsNative = Entry.IsNative();
		};

		if (AccessNodeEntryThreadSafe(InKey, SetIsNative))
		{
			return bIsNative;
		}

		if (const INodeRegistryTemplateEntry* TemplateEntry = FindNodeTemplateEntry(InKey))
		{
			return true;
		}
		return false;
	}

	bool FRegistryContainerImpl::FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
	{
		auto SetFrontendClass = [&OutClass](const INodeRegistryEntry& Entry)
		{
			OutClass = Entry.GetFrontendClass();
		};

		if (AccessNodeEntryThreadSafe(InKey, SetFrontendClass))
		{
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
		static bool bHasWarningBeenIssued = false;
		if (!bHasWarningBeenIssued)
		{
			// This function is known to be thread-unsafe and should not longer be used.
			// This implementation exists to support deprecated usage. 
			UE_LOG(LogMetaSound, Warning, TEXT("Accessing non-thread-safe implementation of FindImplementedInterfacesFromRegistered(...) is known to cause crashes. Please update your code to use the non-deprecated version of this function with the same name"));
			bHasWarningBeenIssued = true;
		}

		const TSet<FMetasoundFrontendVersion>* Interfaces = nullptr;
		AccessNodeEntryThreadSafe(InKey, 
			[&Interfaces](const INodeRegistryEntry& Entry)
			{
				Interfaces = Entry.GetImplementedInterfaces();
			}
		);

		return Interfaces;
	}

	bool FRegistryContainerImpl::FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfaceVersions) const 
	{
		bool bDidCopy = false;

		auto CopyImplementedInterfaces = [&OutInterfaceVersions, &bDidCopy](const INodeRegistryEntry& Entry)
		{
			if (const TSet<FMetasoundFrontendVersion>* Interfaces = Entry.GetImplementedInterfaces())
			{
				OutInterfaceVersions = *Interfaces;
				bDidCopy = true;
			}
		};

		AccessNodeEntryThreadSafe(InKey, CopyImplementedInterfaces);

		return bDidCopy;
	}

	bool FRegistryContainerImpl::FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
	{
		auto CopyClassInfo = [&OutInfo](const INodeRegistryEntry& Entry)
		{
			OutInfo = Entry.GetClassInfo();
		};
		if (AccessNodeEntryThreadSafe(InKey, CopyClassInfo))
		{
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
					OutKey = FNodeRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InDataTypeName, Class))
				{
					OutKey = FNodeRegistryKey(Class.Metadata);
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
			OutKey = FNodeRegistryKey(Class.Metadata);
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
					OutKey = FNodeRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InDataTypeName, Class))
				{
					OutKey = FNodeRegistryKey(Class.Metadata);
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

	bool FRegistryContainerImpl::AccessNodeEntryThreadSafe(const FNodeRegistryKey& InKey, TFunctionRef<void(const INodeRegistryEntry&)> InFunc) const
	{
		auto TryAccessNodeEntry = [this, &InKey, &InFunc]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodes.Find(InKey))
			{
				InFunc(*(*Entry));
				return true;
			}
			return false;
		};

		if (TryAccessNodeEntry())
		{
			return true;
		}
		else
		{
			// Wait for any async registration tasks related to the registry key. 
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return TryAccessNodeEntry();
		}
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

	void FRegistryContainerImpl::WaitForAsyncGraphRegistration(const FNodeRegistryKey& InRegistryKey, const FTopLevelAssetPath& InAssetPath) const
	{
		WaitForAsyncRegistrationInternal(InRegistryKey, &InAssetPath);
	}

	void FRegistryContainerImpl::WaitForAsyncRegistrationInternal(const FNodeRegistryKey& InRegistryKey, const FTopLevelAssetPath* InAssetPath) const
	{
		using namespace UE::Tasks;

		if (AsyncRegistrationPipe.IsInContext())
		{
			// It is not safe to wait for an async registration task from within the async registration pipe because it will result in a deadlock. 
			UE_LOG(LogMetaSound, Verbose, TEXT("Async registration pipe is already in context for registering key %s. Task will not be waited for."), *InRegistryKey.ToString());
			return;
		}

		TArray<FTask> TasksToWaitFor;
		{
			FScopeLock Lock(&ActiveRegistrationTasksCriticalSection);
			if (const TArray<FActiveRegistrationTaskInfo>* FoundTasks = ActiveRegistrationTasks.Find(InRegistryKey))
			{
				// Filter by asset path or ignore if not provided
				Algo::TransformIf(*FoundTasks, TasksToWaitFor,
					[&InAssetPath](const FActiveRegistrationTaskInfo& TaskInfo) { return !InAssetPath || TaskInfo.AssetPath == *InAssetPath; },
					[](const FActiveRegistrationTaskInfo& TaskInfo) { return TaskInfo.Task; });
			}
		}

		for (const FTask& Task : TasksToWaitFor)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::WaitForRegistrationTaskToComplete);
			if (Task.IsValid())
			{
				if (bBusyWaitOnAsyncRegistrationTasks)
				{
					Task.BusyWait();
				}
				else
				{
					Task.Wait();
				}
			}	
		}
	}
} // namespace Metasound::Frontend
