// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetBase.h"

#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Containers/Set.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IAudioParameterTransmitter.h"
#include "Interfaces/MetasoundFrontendInterface.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "Internationalization/Text.h"
#include "IStructSerializerBackend.h"
#include "Logging/LogMacros.h"
#include "MetasoundAssetManager.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendDocumentVersioning.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGraph.h"
#include "MetasoundJsonBackend.h"
#include "MetasoundLog.h"
#include "MetasoundParameterPack.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundTrace.h"
#include "MetasoundVertex.h"
#include "StructSerializer.h"
#include "Templates/SharedPointer.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "MetaSound"

namespace Metasound
{
	namespace Frontend
	{
		namespace AssetBasePrivate
		{
			// Zero values means, that these don't do anything.
			static float BlockRateOverride = 0;
			static int32 SampleRateOverride = 0;

			void DepthFirstTraversal(const FMetasoundAssetBase& InInitAsset, TFunctionRef<TSet<const FMetasoundAssetBase*>(const FMetasoundAssetBase&)> InVisitFunction)
			{
				// Non recursive depth first traversal.
				TArray<const FMetasoundAssetBase*> Stack({ &InInitAsset });
				TSet<const FMetasoundAssetBase*> Visited;

				while (!Stack.IsEmpty())
				{
					const FMetasoundAssetBase* CurrentNode = Stack.Pop();
					if (!Visited.Contains(CurrentNode))
					{
						TArray<const FMetasoundAssetBase*> Children = InVisitFunction(*CurrentNode).Array();
						Stack.Append(Children);

						Visited.Add(CurrentNode);
					}
				}
			}

			// Registers node by copying document. Updates to document require re-registration.
			// This registry entry does not support node creation as it is only intended to be
			// used when cooking MetaSounds. 
			class FDocumentNodeRegistryEntryForCook : public INodeRegistryEntry
			{
			public:
				FDocumentNodeRegistryEntryForCook(const FMetasoundFrontendDocument& InDocument, const FTopLevelAssetPath& InAssetPath)
					: Interfaces(InDocument.Interfaces)
					, FrontendClass(InDocument.RootGraph)
					, ClassInfo(InDocument.RootGraph, InAssetPath)
				{
					// Copy FrontendClass to preserve original document.
					FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
				}

				FDocumentNodeRegistryEntryForCook(const FDocumentNodeRegistryEntryForCook& InOther) = default;

				virtual ~FDocumentNodeRegistryEntryForCook() = default;

				virtual const FNodeClassInfo& GetClassInfo() const override
				{
					return ClassInfo;
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override { return nullptr; }

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return FrontendClass;
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FDocumentNodeRegistryEntryForCook>(*this);
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
				TSet<FMetasoundFrontendVersion> Interfaces;
				FMetasoundFrontendClass FrontendClass;
				FNodeClassInfo ClassInfo;
			};

			void GetUpdatePathForDocument(const FMetasoundFrontendVersion& InCurrentVersion, const FMetasoundFrontendVersion& InTargetVersion, TArray<const IInterfaceRegistryEntry*>& OutUpgradePath)
			{
				if (InCurrentVersion.Name == InTargetVersion.Name)
				{
					// Get all associated registered interfaces
					TArray<FMetasoundFrontendVersion> RegisteredVersions = ISearchEngine::Get().FindAllRegisteredInterfacesWithName(InTargetVersion.Name);

					// Filter registry entries that exist between current version and target version
					auto FilterRegistryEntries = [&InCurrentVersion, &InTargetVersion](const FMetasoundFrontendVersion& InVersion)
					{
						const bool bIsGreaterThanCurrent = InVersion.Number > InCurrentVersion.Number;
						const bool bIsLessThanOrEqualToTarget = InVersion.Number <= InTargetVersion.Number;

						return bIsGreaterThanCurrent && bIsLessThanOrEqualToTarget;
					};
					RegisteredVersions = RegisteredVersions.FilterByPredicate(FilterRegistryEntries);

					// sort registry entries to create an ordered upgrade path.
					RegisteredVersions.Sort();

					// Get registry entries from registry keys.
					auto GetRegistryEntry = [](const FMetasoundFrontendVersion& InVersion)
					{
						FInterfaceRegistryKey Key = GetInterfaceRegistryKey(InVersion);
						return IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key);
					};
					Algo::Transform(RegisteredVersions, OutUpgradePath, GetRegistryEntry);
				}
			}

			bool UpdateDocumentInterface(const TArray<const IInterfaceRegistryEntry*>& InUpgradePath, const FMetasoundFrontendVersion& InterfaceVersion, FDocumentHandle InDocument)
			{
				const FMetasoundFrontendVersionNumber* LastVersionUpdated = nullptr;
				for (const IInterfaceRegistryEntry* Entry : InUpgradePath)
				{
					if (ensure(nullptr != Entry))
					{
						if (Entry->UpdateRootGraphInterface(InDocument))
						{
							LastVersionUpdated = &Entry->GetInterface().Version.Number;
						}
					}
				}

				if (LastVersionUpdated)
				{
#if WITH_EDITOR
					const FString AssetName = *InDocument->GetRootGraphClass().Metadata.GetDisplayName().ToString();
#else
					const FString AssetName = *InDocument->GetRootGraphClass().Metadata.GetClassName().ToString();
#endif // !WITH_EDITOR
					UE_LOG(LogMetaSound, Display, TEXT("Asset '%s' interface '%s' updated: '%s' --> '%s'"),
						*AssetName,
						*InterfaceVersion.Name.ToString(),
						*InterfaceVersion.Number.ToString(),
						*LastVersionUpdated->ToString());
					return true;
				}

				return false;
			}
		} // namespace AssetBasePrivate

		FConsoleVariableMulticastDelegate CVarMetaSoundBlockRateChanged;

		FAutoConsoleVariableRef CVarMetaSoundBlockRate(
			TEXT("au.MetaSound.BlockRate"),
			AssetBasePrivate::BlockRateOverride,
			TEXT("Sets block rate (blocks per second) of MetaSounds.\n")
			TEXT("Default: 100.0f, Min: 1.0f, Max: 1000.0f"),
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { CVarMetaSoundBlockRateChanged.Broadcast(Var); }),
			ECVF_Default);

		FConsoleVariableMulticastDelegate CVarMetaSoundSampleRateChanged;
		FAutoConsoleVariableRef CVarMetaSoundSampleRate(
			TEXT("au.MetaSound.SampleRate"),
			AssetBasePrivate::SampleRateOverride,
			TEXT("Overrides the sample rate of metasounds. Negative values default to audio mixer sample rate.\n")
			TEXT("Default: 0, Min: 8000, Max: 48000"),
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { CVarMetaSoundSampleRateChanged.Broadcast(Var); }),
			ECVF_Default);

		float GetBlockRateOverride()
		{
			if(AssetBasePrivate::BlockRateOverride > 0)
			{
				return FMath::Clamp(AssetBasePrivate::BlockRateOverride, 
					GetBlockRateClampRange().GetLowerBoundValue(), 
					GetBlockRateClampRange().GetUpperBoundValue()
				);
			}
			return AssetBasePrivate::BlockRateOverride;
		}

		FConsoleVariableMulticastDelegate& GetBlockRateOverrideChangedDelegate()
		{
			return CVarMetaSoundBlockRateChanged;
		}

		int32 GetSampleRateOverride()
		{
			if (AssetBasePrivate::SampleRateOverride > 0)
			{
				return FMath::Clamp(AssetBasePrivate::SampleRateOverride, 
					GetSampleRateClampRange().GetLowerBoundValue(),
					GetSampleRateClampRange().GetUpperBoundValue()
				);
			}
			return AssetBasePrivate::SampleRateOverride;
		}

		FConsoleVariableMulticastDelegate& GetSampleRateOverrideChangedDelegate()
		{
			return CVarMetaSoundSampleRateChanged;
		}
		
		TRange<float> GetBlockRateClampRange()
		{
			return TRange<float>(1.f,1000.f);
		}

		TRange<int32> GetSampleRateClampRange()
		{
			return TRange<int32>(8000, 96000);
		}
	} // namespace Frontend
} // namespace Metasound

const FString FMetasoundAssetBase::FileExtension(TEXT(".metasound"));

bool FMetasoundAssetBase::ConformObjectDataToInterfaces()
{
	return false;
}

void FMetasoundAssetBase::RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	// Graph registration must only happen on one thread to avoid race conditions on graph registration.
	checkf(IsInGameThread(), TEXT("MetaSound %s graph can only be registered on the GameThread"), *GetOwningAssetName());
	checkf(!IsRunningCookCommandlet(), TEXT("Cook of asset must call RegisterNode directly providing FDocumentNodeRegistryEntryForCook to avoid proxy/runtime graph generation."));

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::RegisterGraphWithFrontend);
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MetaSoundAssetBase::RegisterGraphWithFrontend asset %s"), *this->GetOwningAssetName()));
	if (!InRegistrationOptions.bForceReregister)
	{
		if (IsRegistered())
		{
			return;
		}
	}

#if WITH_EDITOR
	if (InRegistrationOptions.bRebuildReferencedAssetClasses)
	{
		RebuildReferencedAssetClasses();
	}
#endif

	if (InRegistrationOptions.bRegisterDependencies)
	{
		RegisterAssetDependencies(InRegistrationOptions);
	}

	// This should not be necessary as it should be added on asset load,
	// but currently registration is required to be called prior to adding
	// an object-defined graph class to the registry so it was placed here.
	if (UObject* OwningObject = GetOwningAsset())
	{
		IMetaSoundAssetManager::GetChecked().AddOrUpdateAsset(*OwningObject);
	}

	// Auto update must be done after all referenced asset classes are registered
	if (InRegistrationOptions.bAutoUpdate)
	{
		const bool bDidUpdate = AutoUpdate(InRegistrationOptions.bAutoUpdateLogWarningOnDroppedConnection);
#if WITH_EDITOR
		if (bDidUpdate || InRegistrationOptions.bForceViewSynchronization)
		{
			GetModifyContext().SetForceRefreshViews();
		}
#endif // WITH_EDITOR
	}
	else
	{
#if WITH_EDITOR
		if (InRegistrationOptions.bForceViewSynchronization)
		{
			GetModifyContext().SetForceRefreshViews();
		}
#endif // WITH_EDITOR
	}

#if WITH_EDITOR
	// Must be completed after auto-update to ensure all non-transient referenced dependency data is up-to-date (ex.
	// class version), which is required for most accurately caching current registry metadata.
	CacheRegistryMetadata();
#endif // WITH_EDITOR

	UObject* Owner = GetOwningAsset();
	check(Owner);
	const FString AssetName = Owner->GetName();

	GraphRegistryKey = FRegistryContainerImpl::Get().RegisterGraph(Owner);
	if (GraphRegistryKey.IsValid())
	{
#if WITH_EDITORONLY_DATA
		UpdateAssetRegistry();
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		UClass* Class = Owner->GetClass();
		check(Class);
		const FString ClassName = Class->GetName();
		UE_LOG(LogMetaSound, Error, TEXT("Registration failed for MetaSound node class '%s' of UObject class '%s'"), *AssetName, *ClassName);
	}
}

void FMetasoundAssetBase::CookMetaSound()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::CookMetaSound);
	if (IsRegistered())
	{
		return;
	}

	CookReferencedMetaSounds();
	IMetaSoundAssetManager::GetChecked().AddOrUpdateAsset(*GetOwningAsset());

	// Auto update must be done after all referenced asset classes are registered
	const bool bDidUpdate = AutoUpdate(/*bAutoUpdateLogWarningOnDroppedConnection=*/true);
#if WITH_EDITOR
	if (bDidUpdate)
	{
		GetModifyContext().SetForceRefreshViews();
	}
#endif // WITH_EDITOR

#if WITH_EDITOR
	// Must be completed after auto-update to ensure all non-transient referenced dependency data is up-to-date (ex.
	// class version), which is required for most accurately caching current registry metadata.
	CacheRegistryMetadata();
#endif // WITH_EDITOR

	UObject* Owner = GetOwningAsset();
	check(Owner);

	{
#if WITH_EDITORONLY_DATA
		// Performs document transforms on local copy, which reduces document footprint & renders transforming unnecessary at runtime
		FMetaSoundFrontendDocumentBuilder& DocBuilder = IDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(Owner);
		const bool bContainsTemplateDependency = DocBuilder.ContainsDependencyOfType(EMetasoundFrontendClassType::Template);
		if (bContainsTemplateDependency)
		{
			DocBuilder.TransformTemplateNodes();
		}
#endif // WITH_EDITORONLY_DATA

		if (GraphRegistryKey.IsValid())
		{
			FRegistryContainerImpl::Get().UnregisterNode(GraphRegistryKey.NodeKey);
			GraphRegistryKey = { };
		}

		// During cook, we need to register the node so that it is available for other graphs, but we need to avoid
		// creating proxies. To do so, we use a special node registration object which reflects the necessary information
		// for the node registry, but does not create INodes.
		TScriptInterface<IMetaSoundDocumentInterface> DocInterface(Owner);
		const FMetasoundFrontendDocument& Document = DocInterface->GetConstDocument();
		const FTopLevelAssetPath AssetPath = DocInterface->GetAssetPathChecked();
		TUniquePtr<INodeRegistryEntry> RegistryEntry = MakeUnique<AssetBasePrivate::FDocumentNodeRegistryEntryForCook>(Document, AssetPath);

		const FNodeRegistryKey NodeKey = FRegistryContainerImpl::Get().RegisterNode(MoveTemp(RegistryEntry));
		GraphRegistryKey = FGraphRegistryKey { NodeKey, AssetPath };
	}

	if (GraphRegistryKey.IsValid())
	{
#if WITH_EDITORONLY_DATA
		UpdateAssetRegistry();
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		const UClass* Class = Owner->GetClass();
		check(Class);
		const FString ClassName = Class->GetName();
		UE_LOG(LogMetaSound, Error, TEXT("Registration failed during cook for MetaSound node class '%s' of UObject class '%s'"), *GetOwningAssetName(), *ClassName);
	}
}

void FMetasoundAssetBase::OnNotifyBeginDestroy()
{
	using namespace Metasound::Frontend;

	// Unregistration of graph is not necessary when cooking as deserialized objects are not mutable and, should they be reloaded,
	// omitting unregistration avoids potentially kicking off an invalid asynchronous task to unregister a non-existent runtime graph.
	if (IsRunningCookCommandlet())
	{
		if (GraphRegistryKey.IsValid())
		{
			FRegistryContainerImpl::Get().UnregisterNode(GraphRegistryKey.NodeKey);
			GraphRegistryKey = { };
		}
	}
	else
	{
		// The editor module removal and addition of assets with the MetaSoundAssetManager directly,
		// so it can update open editors and load possible assets to reference at will, as opposed
		// to managing by direct load requests and associated asset references.
		// (Owning "asset" is a misnomer as we support dynamically generated MetaSounds at runtime now.)
#if WITH_EDITOR
		UObject* OwningAsset = GetOwningAsset();
		check(OwningAsset);
		if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
		{
			AssetManager->RemoveAsset(*OwningAsset);
		}
#endif // !WITH_EDITOR

		UnregisterGraphWithFrontend();
	}
}

void FMetasoundAssetBase::UnregisterGraphWithFrontend()
{
	using namespace Metasound::Frontend;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::UnregisterGraphWithFrontend);

	check(IsInGameThread());
	checkf(!IsRunningCookCommandlet(), TEXT("Cook of asset must call UnregisterNode directly providing FDocumentNodeRegistryEntryForCook to avoid proxy/runtime graph generation."));

	if (GraphRegistryKey.IsValid())
	{
		UObject* OwningAsset = GetOwningAsset();
		if (ensureAlways(OwningAsset))
		{
			const bool bSuccess = FRegistryContainerImpl::Get().UnregisterGraph(GraphRegistryKey, OwningAsset);
			if (!bSuccess)
			{
				UE_LOG(LogMetaSound, Verbose, TEXT("Failed to unregister node with key %s for asset %s. No registry entry exists with that key."), *GraphRegistryKey.ToString(), *GetOwningAssetName());
			}
		}

		GraphRegistryKey = { };
	}
}

bool FMetasoundAssetBase::IsInterfaceDeclared(const FMetasoundFrontendVersion& InVersion) const
{
	return GetConstDocumentChecked().Interfaces.Contains(InVersion);
}

Metasound::Frontend::FNodeClassInfo FMetasoundAssetBase::GetAssetClassInfo() const
{
	using namespace Metasound::Frontend;

	const UObject* Owner = GetOwningAsset();
	check(Owner);
	TScriptInterface<const IMetaSoundDocumentInterface> DocInterface((UObject*)Owner);
	return FNodeClassInfo { GetConstDocumentChecked().RootGraph, DocInterface->GetAssetPathChecked() };
}

void FMetasoundAssetBase::SetDocument(FMetasoundFrontendDocument InDocument, bool bMarkDirty)
{
	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	*Document = MoveTemp(InDocument);
	if (bMarkDirty)
	{
		MarkMetasoundDocumentDirty();
	}
}

bool FMetasoundAssetBase::VersionAsset()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::VersionAsset);
	constexpr bool bIsDeterministic = true;
	FDocumentIDGenerator::FScopeDeterminism DeterminismScope(bIsDeterministic);

	UObject* OwningAsset = GetOwningAsset();
	check(OwningAsset);

	bool bDidEdit = false;

#if WITH_EDITORONLY_DATA
	// Version Document Model
	if (GetConstDocumentChecked().RequiresInterfaceVersioning())
	{
		GetDocumentChecked().VersionInterfaces();
		bDidEdit = true;
	}
#endif // WITH_EDITORONLY_DATA

	bDidEdit |= Metasound::Frontend::VersionDocument(*this);

	// Version Interfaces. Has to be re-run until no pass reports an update in case
	// versions fork (ex. an interface splits into two newly named interfaces).
	// TODO: Need to add function to determine if interfaces require updating before
	// running update to avoid invalidating builder cache when not necessary. This
	// will require either re-writing old transforms with the builder API or just
	// leaving handle/controller logic and moving to deprecated implementation that
	// is bypassed on versioned documents in most cases.
	{
		bool bInterfaceUpdated = false;
		bool bPassUpdated = true;
		TScriptInterface<IMetaSoundDocumentInterface> Interface(OwningAsset);
		const IMetaSoundDocumentInterface* ConstInterface = Interface.GetInterface();
		while (bPassUpdated)
		{
			bPassUpdated = false;

			const TArray<FMetasoundFrontendVersion> Versions = ConstInterface->GetConstDocument().Interfaces.Array();
			for (const FMetasoundFrontendVersion& Version : Versions)
			{
				FUpdateRootGraphInterface UpdateTransform(Version, GetOwningAssetName());
				bPassUpdated = TryUpdateInterfaceFromVersion(Version);
			}

			bInterfaceUpdated |= bPassUpdated;
		}

		if (bInterfaceUpdated)
		{
			Interface->ConformObjectToDocument();
		}
		bDidEdit |= bInterfaceUpdated;
	}

	return bDidEdit;
}

#if WITH_EDITOR
void FMetasoundAssetBase::CacheRegistryMetadata()
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	if (!ensure(Document))
	{
		return;
	}

	using FNameDataTypePair = TPair<FName, FName>;
	const TSet<FMetasoundFrontendVersion>& InterfaceVersions = Document->Interfaces;
	FMetasoundFrontendClassInterface& RootGraphClassInterface = Document->RootGraph.Interface;

	// 1. Gather inputs/outputs managed by interfaces
	TMap<FNameDataTypePair, FMetasoundFrontendClassInput*> Inputs;
	for (FMetasoundFrontendClassInput& Input : RootGraphClassInterface.Inputs)
	{
		FNameDataTypePair NameDataTypePair = FNameDataTypePair(Input.Name, Input.TypeName);
		Inputs.Add(MoveTemp(NameDataTypePair), &Input);
	}

	TMap<FNameDataTypePair, FMetasoundFrontendClassOutput*> Outputs;
	for (FMetasoundFrontendClassOutput& Output : RootGraphClassInterface.Outputs)
	{
		FNameDataTypePair NameDataTypePair = FNameDataTypePair(Output.Name, Output.TypeName);
		Outputs.Add(MoveTemp(NameDataTypePair), &Output);
	}

	// 2. Copy metadata for inputs/outputs managed by interfaces, removing them from maps generated
	auto CacheInterfaceMetadata = [](const FMetasoundFrontendVertexMetadata & InRegistryMetadata, FMetasoundFrontendVertexMetadata& OutMetadata)
	{
		const int32 CachedSortOrderIndex = OutMetadata.SortOrderIndex;
		OutMetadata = InRegistryMetadata;
		OutMetadata.SortOrderIndex = CachedSortOrderIndex;
	};

	for (const FMetasoundFrontendVersion& Version : InterfaceVersions)
	{
		const FInterfaceRegistryKey InterfaceKey = GetInterfaceRegistryKey(Version);
		const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(InterfaceKey);

		UE_CLOG(nullptr == Entry, LogMetaSound, Error, TEXT("Failed to find interface (%s) when caching registry data for %s. MetaSound inputs and outputs for asset may not function correctly."), *Version.ToString(), *GetOwningAssetName());

		if (Entry)
		{
			for (const FMetasoundFrontendClassInput& InterfaceInput : Entry->GetInterface().Inputs)
			{
				const FNameDataTypePair NameDataTypePair = FNameDataTypePair(InterfaceInput.Name, InterfaceInput.TypeName);
				if (FMetasoundFrontendClassInput* Input = Inputs.FindRef(NameDataTypePair))
				{
					CacheInterfaceMetadata(InterfaceInput.Metadata, Input->Metadata);
					Inputs.Remove(NameDataTypePair);
				}
			}

			for (const FMetasoundFrontendClassOutput& InterfaceOutput : Entry->GetInterface().Outputs)
			{
				const FNameDataTypePair NameDataTypePair = FNameDataTypePair(InterfaceOutput.Name, InterfaceOutput.TypeName);
				if (FMetasoundFrontendClassOutput* Output = Outputs.FindRef(NameDataTypePair))
				{
					CacheInterfaceMetadata(InterfaceOutput.Metadata, Output->Metadata);
					Outputs.Remove(NameDataTypePair);
				}
			}
		}
	}

	// 3. Iterate remaining inputs/outputs not managed by interfaces and set to serialize text
	// (in case they were orphaned by an interface no longer being implemented).
	for (TPair<FNameDataTypePair, FMetasoundFrontendClassInput*>& Pair : Inputs)
	{
		Pair.Value->Metadata.SetSerializeText(true);
	}

	for (TPair<FNameDataTypePair, FMetasoundFrontendClassOutput*>& Pair : Outputs)
	{
		Pair.Value->Metadata.SetSerializeText(true);
	}

	// 4. Refresh style as order of members could've changed
	{
		FMetasoundFrontendInterfaceStyle InputStyle;
		Algo::ForEach(RootGraphClassInterface.Inputs, [&InputStyle](const FMetasoundFrontendClassInput& Input)
		{
			InputStyle.DefaultSortOrder.Add(Input.Metadata.SortOrderIndex);
		});
		RootGraphClassInterface.SetInputStyle(InputStyle);
	}

	{
		FMetasoundFrontendInterfaceStyle OutputStyle;
		Algo::ForEach(RootGraphClassInterface.Outputs, [&OutputStyle](const FMetasoundFrontendClassOutput& Output)
		{
			OutputStyle.DefaultSortOrder.Add(Output.Metadata.SortOrderIndex);
		});
		RootGraphClassInterface.SetOutputStyle(OutputStyle);
	}

	// 5. Cache registry data on document dependencies
	for (FMetasoundFrontendClass& Dependency : Document->Dependencies)
	{
		if (!FMetasoundFrontendClass::CacheGraphDependencyMetadataFromRegistry(Dependency))
		{
			UE_LOG(LogMetaSound, Warning,
				TEXT("'%s' failed to cache dependency registry data: Registry missing class with key '%s'"),
				*GetOwningAssetName(),
				*Dependency.Metadata.GetClassName().ToString());
			UE_LOG(LogMetaSound, Warning,
				TEXT("Asset '%s' may fail to build runtime graph unless re-registered after dependency with given key is loaded."),
				*GetOwningAssetName());
		}
	}
}

FMetasoundFrontendDocumentModifyContext& FMetasoundAssetBase::GetModifyContext()
{
	return GetDocumentChecked().Metadata.ModifyContext;
}

const FMetasoundFrontendDocumentModifyContext& FMetasoundAssetBase::GetConstModifyContext() const
{
	return GetConstDocumentChecked().Metadata.ModifyContext;
}

const FMetasoundFrontendDocumentModifyContext& FMetasoundAssetBase::GetModifyContext() const
{
	return GetConstDocumentChecked().Metadata.ModifyContext;
}
#endif // WITH_EDITOR

bool FMetasoundAssetBase::IsRegistered() const
{
	using namespace Metasound::Frontend;

	return GraphRegistryKey.IsValid();
}

bool FMetasoundAssetBase::IsReferencedAsset(const FMetasoundAssetBase& InAsset) const
{
	using namespace Metasound::Frontend;

	bool bIsReferenced = false;
	AssetBasePrivate::DepthFirstTraversal(*this, [&](const FMetasoundAssetBase& ChildAsset)
	{
		TSet<const FMetasoundAssetBase*> Children;
		if (&ChildAsset == &InAsset)
		{
			bIsReferenced = true;
			return Children;
		}

		TArray<FMetasoundAssetBase*> ChildRefs;
		ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(ChildAsset, ChildRefs));
		Algo::Transform(ChildRefs, Children, [](FMetasoundAssetBase* Child) { return Child; });
		return Children;

	});

	return bIsReferenced;
}

bool FMetasoundAssetBase::AddingReferenceCausesLoop(const FMetasoundAssetBase& InMetaSound) const
{
	using namespace Metasound::Frontend;

	bool bCausesLoop = false;
	const FMetasoundAssetBase* Parent = this;
	AssetBasePrivate::DepthFirstTraversal(InMetaSound, [&](const FMetasoundAssetBase& ChildAsset)
	{
		TSet<const FMetasoundAssetBase*> Children;
		if (Parent == &ChildAsset)
		{
			bCausesLoop = true;
			return Children;
		}

		TArray<FMetasoundAssetBase*> ChildRefs;
		ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(ChildAsset, ChildRefs));
		Algo::Transform(ChildRefs, Children, [](FMetasoundAssetBase* Child) { return Child; });
		return Children;
	});

	return bCausesLoop;
}

bool FMetasoundAssetBase::AddingReferenceCausesLoop(const FSoftObjectPath& InReferencePath) const
{
	using namespace Metasound::Frontend;

	const FMetasoundAssetBase* ReferenceAsset = IMetaSoundAssetManager::GetChecked().TryLoadAsset(InReferencePath);
	if (!ensureAlways(ReferenceAsset))
	{
		return false;
	}

	return AddingReferenceCausesLoop(*ReferenceAsset);
}

TArray<FMetasoundAssetBase::FSendInfoAndVertexName> FMetasoundAssetBase::GetSendInfos(uint64 InInstanceID) const
{
	return TArray<FSendInfoAndVertexName>();
}

#if WITH_EDITOR
FText FMetasoundAssetBase::GetDisplayName(FString&& InTypeName) const
{
	using namespace Metasound::Frontend;

	FConstGraphHandle GraphHandle = GetRootGraphHandle();
	const bool bIsPreset = !GraphHandle->GetGraphStyle().bIsGraphEditable;

	if (!bIsPreset)
	{
		return FText::FromString(MoveTemp(InTypeName));
	}

	return FText::Format(LOCTEXT("PresetDisplayNameFormat", "{0} (Preset)"), FText::FromString(MoveTemp(InTypeName)));
}
#endif // WITH_EDITOR

bool FMetasoundAssetBase::MarkMetasoundDocumentDirty() const
{
	if (const UObject* OwningAsset = GetOwningAsset())
	{
		return OwningAsset->MarkPackageDirty();
	}
	return false;
}

Metasound::Frontend::FDocumentHandle FMetasoundAssetBase::GetDocumentHandle()
{
	return Metasound::Frontend::IDocumentController::CreateDocumentHandle(GetDocumentAccessPtr());
}

Metasound::Frontend::FConstDocumentHandle FMetasoundAssetBase::GetDocumentHandle() const
{
	return Metasound::Frontend::IDocumentController::CreateDocumentHandle(GetDocumentConstAccessPtr());
}

Metasound::Frontend::FGraphHandle FMetasoundAssetBase::GetRootGraphHandle()
{
	return GetDocumentHandle()->GetRootGraph();
}

Metasound::Frontend::FConstGraphHandle FMetasoundAssetBase::GetRootGraphHandle() const
{
	return GetDocumentHandle()->GetRootGraph();
}

bool FMetasoundAssetBase::ImportFromJSON(const FString& InJSON)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::ImportFromJSON);

	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	if (ensure(nullptr != Document))
	{
		bool bSuccess = Metasound::Frontend::ImportJSONToMetasound(InJSON, *Document);

		if (bSuccess)
		{
			ensure(MarkMetasoundDocumentDirty());
		}

		return bSuccess;
	}
	return false;
}

bool FMetasoundAssetBase::ImportFromJSONAsset(const FString& InAbsolutePath)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::ImportFromJSONAsset);

	Metasound::Frontend::FDocumentAccessPtr DocumentPtr = GetDocumentAccessPtr();
	if (FMetasoundFrontendDocument* Document = DocumentPtr.Get())
	{
		bool bSuccess = Metasound::Frontend::ImportJSONAssetToMetasound(InAbsolutePath, *Document);

		if (bSuccess)
		{
			ensure(MarkMetasoundDocumentDirty());
		}

		return bSuccess;
	}
	return false;
}

const FMetasoundFrontendDocument& FMetasoundAssetBase::GetConstDocumentChecked() const
{
	const UObject* Owner = GetOwningAsset();
	check(Owner);

	// Annoying const cast requirement because ScriptInterface isn't ctor isn't const correct :(
	TScriptInterface<IMetaSoundDocumentInterface> DocInterface = const_cast<UObject*>(Owner);

	return DocInterface->GetConstDocument();
}

FMetasoundFrontendDocument& FMetasoundAssetBase::GetDocumentChecked()
{
	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	check(nullptr != Document);
	return *Document;
}

const FMetasoundFrontendDocument& FMetasoundAssetBase::GetDocumentChecked() const
{
	return GetConstDocumentChecked();
}

const Metasound::Frontend::FGraphRegistryKey& FMetasoundAssetBase::GetGraphRegistryKey() const
{
	return GraphRegistryKey;
}

const Metasound::Frontend::FNodeRegistryKey& FMetasoundAssetBase::GetRegistryKey() const
{
	return GraphRegistryKey.NodeKey;
}

FString FMetasoundAssetBase::GetOwningAssetName() const
{
	if (const UObject* OwningAsset = GetOwningAsset())
	{
		return OwningAsset->GetPathName();
	}
	return FString();
}

#if WITH_EDITOR
void FMetasoundAssetBase::RebuildReferencedAssetClasses()
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	AssetManager.AddAssetReferences(*this);
	TSet<IMetaSoundAssetManager::FAssetInfo> ReferencedAssetClasses = AssetManager.GetReferencedAssetClasses(*this);
	SetReferencedAssetClasses(MoveTemp(ReferencedAssetClasses));
}
#endif // WITH_EDITOR

void FMetasoundAssetBase::RegisterAssetDependencies(const Metasound::Frontend::FMetaSoundAssetRegistrationOptions& InRegistrationOptions)
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	TArray<FMetasoundAssetBase*> References = GetReferencedAssets();
	for (FMetasoundAssetBase* Reference : References)
	{
		if (InRegistrationOptions.bForceReregister || !Reference->IsRegistered())
		{
			// TODO: Check for infinite recursion and error if so
			AssetManager.AddOrUpdateAsset(*(Reference->GetOwningAsset()));
			Reference->RegisterGraphWithFrontend(InRegistrationOptions);
		}
	}
}

void FMetasoundAssetBase::CookReferencedMetaSounds()
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	TArray<FMetasoundAssetBase*> References = GetReferencedAssets();
	for (FMetasoundAssetBase* Reference : References)
	{
		if (!Reference->IsRegistered())
		{
			// TODO: Check for infinite recursion and error if so
			AssetManager.AddOrUpdateAsset(*(Reference->GetOwningAsset()));
			Reference->CookMetaSound();
		}
	}
}

bool FMetasoundAssetBase::AutoUpdate(bool bInLogWarningsOnDroppedConnection)
{
	using namespace Metasound::Frontend;

	FString OwningAssetName = GetOwningAssetName();
	const bool bAutoUpdated = FAutoUpdateRootGraph(MoveTemp(OwningAssetName), bInLogWarningsOnDroppedConnection).Transform(GetDocumentHandle());
	return bAutoUpdated;
}

#if WITH_EDITORONLY_DATA
void FMetasoundAssetBase::UpdateAssetRegistry() 
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	UObject* Owner = GetOwningAsset();
	check(Owner);
	const FMetasoundFrontendGraphClass& DocumentClassGraph = GetDocumentChecked().RootGraph;

	TScriptInterface<IMetaSoundDocumentInterface> DocInterface(Owner);
	FNodeClassInfo AssetClassInfo(DocumentClassGraph, DocInterface->GetAssetPathChecked());

	// Refresh Asset Registry Info if successfully registered with Frontend
	const FMetasoundFrontendClassMetadata& DocumentClassMetadata = DocumentClassGraph.Metadata;
	AssetClassInfo.AssetClassID = FGuid(DocumentClassMetadata.GetClassName().Name.ToString());
	FNodeClassName ClassName = DocumentClassMetadata.GetClassName().ToNodeClassName();
	FMetasoundFrontendClass GraphClass;

	AssetClassInfo.bIsPreset = DocumentClassGraph.PresetOptions.bIsPreset;
	AssetClassInfo.Version = DocumentClassMetadata.GetVersion();
	AssetClassInfo.InputTypes.Reset();
	Algo::Transform(GraphClass.Interface.Inputs, AssetClassInfo.InputTypes, [] (const FMetasoundFrontendClassInput& Input) { return Input.TypeName; });

	AssetClassInfo.OutputTypes.Reset();
	Algo::Transform(GraphClass.Interface.Outputs, AssetClassInfo.OutputTypes, [](const FMetasoundFrontendClassOutput& Output) { return Output.TypeName; });

	SetRegistryAssetClassInfo(MoveTemp(AssetClassInfo));
}
#endif

bool FMetasoundAssetBase::TryUpdateInterfaceFromVersion(const FMetasoundFrontendVersion& Version)
{
	using namespace Metasound::Frontend;
	using namespace AssetBasePrivate;

	FConstDocumentHandle ConstDoc = IDocumentController::CreateDocumentHandle(GetDocumentConstAccessPtr());
	FMetasoundFrontendInterface TargetInterface = GetInterfaceToVersion(Version, ConstDoc);

	if (TargetInterface.Version.IsValid())
	{
		TArray<const IInterfaceRegistryEntry*> UpgradePath;
		GetUpdatePathForDocument(Version, TargetInterface.Version, UpgradePath);
		const bool bUpdated = UpdateDocumentInterface(UpgradePath, Version, GetDocumentHandle());
		ensureMsgf(bUpdated, TEXT("Target interface '%s' was out-of-date but interface failed to be updated"), *TargetInterface.Version.ToString());
		return bUpdated;
	}

	return false;
}

FMetasoundFrontendInterface FMetasoundAssetBase::GetInterfaceToVersion(const FMetasoundFrontendVersion& InterfaceVersion, Metasound::Frontend::FConstDocumentHandle InDocument) const
{
	using namespace Metasound::Frontend;

	// Find registered target interface.
	FMetasoundFrontendInterface TargetInterface;
	bool bFoundTargetInterface = ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceVersion.Name, TargetInterface);
	if (!bFoundTargetInterface)
	{
		UE_LOG(LogMetaSound, Warning,
			TEXT("Could not check for interface updates. Target interface is not registered [InterfaceVersion:%s] when attempting to update root graph of asset (%s). "
				"Ensure that the module which registers the interface has been loaded before the asset is loaded."),
			*InterfaceVersion.ToString(),
			*GetOwningAssetName());
		return { };
	}

	if (TargetInterface.Version == InterfaceVersion)
	{
		return { };
	}

	return TargetInterface;
}

TSharedPtr<FMetasoundFrontendDocument> FMetasoundAssetBase::PreprocessDocument()
{
	return nullptr;
}

#if WITH_EDITORONLY_DATA
bool FMetasoundAssetBase::GetVersionedOnLoad() const
{
	return bVersionedOnLoad;
}

void FMetasoundAssetBase::ClearVersionedOnLoad()
{
	bVersionedOnLoad = false;
}

void FMetasoundAssetBase::SetVersionedOnLoad()
{
	bVersionedOnLoad = true;
}
#endif // WITH_EDITORONLY_DATA

PRAGMA_DISABLE_DEPRECATION_WARNINGS
const FMetasoundAssetBase::FRuntimeData& FMetasoundAssetBase::GetRuntimeData() const
{
	static const FRuntimeData PlaceholderForDeprecatedMethod;
	return PlaceholderForDeprecatedMethod;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE // "MetaSound"
