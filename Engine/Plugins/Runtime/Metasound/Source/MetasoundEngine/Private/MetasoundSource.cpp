// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundSource.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDeviceManager.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundAudioFormats.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundEngineAsset.h"
#include "MetasoundEngineEnvironment.h"
#include "MetasoundEnvironment.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGenerator.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorBuilderSettings.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundPrimitives.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundSettings.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundSource)

#if WITH_EDITORONLY_DATA
#include "EdGraph/EdGraph.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "MetaSound"

namespace Metasound
{
	namespace SourcePrivate
	{
		Frontend::FMetaSoundAssetRegistrationOptions GetInitRegistrationOptions()
		{
			Frontend::FMetaSoundAssetRegistrationOptions RegOptions;
			RegOptions.bForceReregister = false;
#if !WITH_EDITOR 
			if (Frontend::MetaSoundEnableCookDeterministicIDGeneration != 0)
			{
				// When without editor, don't AutoUpdate or ResolveDocument at runtime. This only happens at cook or save.
				// When with editor, those are needed because sounds are not necessarily saved before previewing.
				RegOptions.bAutoUpdate = false;
			}
#endif // !WITH_EDITOR
			if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
			{
				RegOptions.bAutoUpdateLogWarningOnDroppedConnection = Settings->bAutoUpdateLogWarningOnDroppedConnection;
			}

			return RegOptions;
		}

		class FParameterRouter
		{
			struct FQueueState
			{
				TWeakPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel;
				bool bWriterAvailable = true;
			};

		public:

			using FAudioDeviceIDAndInstanceID = TTuple<Audio::DeviceID, uint64>;

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannelForReader(Audio::DeviceID InDeviceID, uint64 InstanceID)
			{
				constexpr bool bIsForWriter = false;
				return FindOrCreateDataChannel(InDeviceID, InstanceID, bIsForWriter);
			}

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannelForWriter(Audio::DeviceID InDeviceID, uint64 InstanceID)
			{
				constexpr bool bIsForWriter = true;
				return FindOrCreateDataChannel(InDeviceID, InstanceID, bIsForWriter);
			}

		private:

			TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> FindOrCreateDataChannel(Audio::DeviceID InDeviceID, uint64 InstanceID, bool bIsForWriter)
			{
				FScopeLock Lock(&DataChannelMapCS);

				FAudioDeviceIDAndInstanceID Key = {InDeviceID, InstanceID};
				const bool bIsForReader = !bIsForWriter;

				if (FQueueState* State = DataChannels.Find(Key))
				{
					// Allow multiple readers to be returned because FMetaSoundGenerators are recreated when they come out of virtualization.
					// Only allow a single writer to be returned because FMetaSoundParameterTransmitters are only created once
					const bool bIsAvailable = bIsForReader || (State->bWriterAvailable && bIsForWriter);
					if (bIsAvailable)
					{
						TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> Channel = State->DataChannel.Pin();
						if (Channel.IsValid())
						{
							if (bIsForWriter)
							{
								State->bWriterAvailable = false;
							}
							return Channel;
						}
					}
				}

				TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> NewChannel = MakeShared<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>>();

				FQueueState NewState;
				NewState.DataChannel = NewChannel;
				if (bIsForWriter)
				{
					NewState.bWriterAvailable = false;
				}

				DataChannels.Add(Key, NewState);
				return NewChannel;
			}

			FCriticalSection DataChannelMapCS;
			TSortedMap<FAudioDeviceIDAndInstanceID, FQueueState> DataChannels;
		};
	} // namespace SourcePrivate
} // namespace Metasound


UMetaSoundSource::UMetaSoundSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FMetasoundAssetBase()
{
	bProcedural = true;
	bRequiresStopFade = true;
	NumChannels = 1;
}

const UClass& UMetaSoundSource::GetBaseMetaSoundUClass() const
{
	return *UMetaSoundSource::StaticClass();
}

const FMetasoundFrontendDocument& UMetaSoundSource::GetDocument() const
{
	return RootMetasoundDocument;
}

#if WITH_EDITOR
void UMetaSoundSource::PostEditUndo()
{
	Super::PostEditUndo();
	Metasound::FMetaSoundEngineAssetHelper::PostEditUndo(*this);
}

void UMetaSoundSource::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	// Guid is reset as asset may share implementation from
	// asset duplicated from but should not be registered as such.
	if (InDuplicateMode == EDuplicateMode::Normal)
	{
		AssetClassID = FGuid::NewGuid();
		Metasound::Frontend::FRenameRootGraphClass::Generate(GetDocumentHandle(), AssetClassID);
	}
}

void UMetaSoundSource::PostEditChangeProperty(FPropertyChangedEvent& InEvent)
{
	Super::PostEditChangeProperty(InEvent);

	if (InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, OutputFormat))
	{
		PostEditChangeOutputFormat();
	}
	if (InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, SampleRateOverride) ||
		InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, BlockRateOverride) ||
		InEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, QualitySetting) )
	{
		PostEditChangeQualitySettings();
	}
}

bool UMetaSoundSource::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Allow changes to quality if we don't have any overrides.
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSource, QualitySetting))
	{
		const bool bBlockRateIsZero = BlockRateOverride.GetValue() == 0;
		const bool bSampleRateIsZero = SampleRateOverride.GetValue() == 0;

		return bBlockRateIsZero && bSampleRateIsZero;
	}
		
	return true;
}

void UMetaSoundSource::PostEditChangeOutputFormat()
{
	using namespace Metasound::Frontend;

	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	{
		const UMetaSoundBuilderSubsystem& BuilderSubsystem = UMetaSoundBuilderSubsystem::GetConstChecked();

		UMetaSoundSourceBuilder* SourceBuilder = BuilderSubsystem.AttachSourceBuilderToAsset(this);
		check(SourceBuilder);
		SourceBuilder->SetFormat(OutputFormat, Result);

		// TODO: Once builders are notified of controller changes and can be safely persistent, this
		// can be removed so builders can be shared and not have to be created for each change output
		// format mutation transaction.
		BuilderSubsystem.DetachBuilderFromAsset(GetDocument().RootGraph.Metadata.GetClassName());
	}

	if (Result == EMetaSoundBuilderResult::Succeeded)
	{
		// Update the data in this UMetaSoundSource to reflect what is in the metasound document.
		ConformObjectDataToInterfaces();

		// Use the editor form of register to ensure other editors'
		// MetaSounds are auto-updated if they are referencing this graph.
		if (Graph)
		{
			Graph->RegisterGraphWithFrontend();
		}
		MarkMetasoundDocumentDirty();
	}
}

void UMetaSoundSource::PostEditChangeQualitySettings()
{
	// Re-cache Operator settings by clearing the Optional.
	OperatorSettings.Reset();

	// Refresh the SampleRate (which is what the engine sees from the operator settings).
	SampleRate = GetOperatorSettings(SampleRate).GetSampleRate();

	// Always refresh the GUID with the selection.
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())	
	{
		auto FindByName = [&Name = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == Name; };
		if (const FMetaSoundQualitySettings* Found = Settings->QualitySettings.FindByPredicate(FindByName))
		{
			QualitySettingGuid = Found->UniqueId;
		}
	}
}
#endif // WITH_EDITOR

bool UMetaSoundSource::ConformObjectDataToInterfaces()
{
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	bool bDidAlterObjectData = false;

	// Update the OutputFormat and NumChannels to match the audio format interface
	// on the root document.
	const FOutputAudioFormatInfoMap& FormatInfo = GetOutputAudioFormatInfo();
	for (const FOutputAudioFormatInfoPair& Pair : FormatInfo)
	{
		if (RootMetasoundDocument.Interfaces.Contains(Pair.Value.InterfaceVersion))
		{
			if ((OutputFormat != Pair.Key) || (NumChannels != Pair.Value.OutputVertexChannelOrder.Num()))
			{
				OutputFormat = Pair.Key;
				NumChannels = Pair.Value.OutputVertexChannelOrder.Num();
				bDidAlterObjectData = true;
			}

			break;
		}
	}

	return bDidAlterObjectData;
}

void UMetaSoundSource::BeginDestroy()
{
	UnregisterGraphWithFrontend();
	Super::BeginDestroy();
}

void UMetaSoundSource::PreSave(FObjectPreSaveContext InSaveContext)
{
	Super::PreSave(InSaveContext);
	Metasound::FMetaSoundEngineAssetHelper::PreSaveAsset(*this, InSaveContext);
}

void UMetaSoundSource::Serialize(FArchive& InArchive)
{
	Super::Serialize(InArchive);
	Metasound::FMetaSoundEngineAssetHelper::SerializeToArchive(*this, InArchive);
}

#if WITH_EDITOR
void UMetaSoundSource::SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses)
{
	Metasound::FMetaSoundEngineAssetHelper::SetReferencedAssetClasses(*this, MoveTemp(InAssetClasses));
}
#endif // WITH_EDITOR

TArray<FMetasoundAssetBase*> UMetaSoundSource::GetReferencedAssets()
{
	return Metasound::FMetaSoundEngineAssetHelper::GetReferencedAssets(*this);
}

const TSet<FSoftObjectPath>& UMetaSoundSource::GetAsyncReferencedAssetClassPaths() const 
{
	return ReferenceAssetClassCache;
}

void UMetaSoundSource::OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences)
{
	Metasound::FMetaSoundEngineAssetHelper::OnAsyncReferencedAssetsLoaded(*this, InAsyncReferences);
}

#if WITH_EDITORONLY_DATA

UEdGraph* UMetaSoundSource::GetGraph()
{
	return Graph;
}

const UEdGraph* UMetaSoundSource::GetGraph() const
{
	return Graph;
}

UEdGraph& UMetaSoundSource::GetGraphChecked()
{
	check(Graph);
	return *Graph;
}

const UEdGraph& UMetaSoundSource::GetGraphChecked() const
{
	check(Graph);
	return *Graph;
}

FText UMetaSoundSource::GetDisplayName() const
{
	FString TypeName = UMetaSoundSource::StaticClass()->GetName();
	return FMetasoundAssetBase::GetDisplayName(MoveTemp(TypeName));
}


void UMetaSoundSource::SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InNodeInfo)
{
	Metasound::FMetaSoundEngineAssetHelper::SetMetaSoundRegistryAssetClassInfo(*this, InNodeInfo);
}
#endif // WITH_EDITORONLY_DATA

void UMetaSoundSource::PostLoad()
{
	Super::PostLoad();
	Metasound::FMetaSoundEngineAssetHelper::PostLoad(*this);

	Duration = GetDuration();
	bLooping = IsLooping();

	PostLoadQualitySettings();
}

void UMetaSoundSource::PostLoadQualitySettings()
{
#if WITH_EDITORONLY_DATA
	
	// Ensure that our Quality settings resolve. 
	if (UMetaSoundSettings* Settings = GetMutableDefault<UMetaSoundSettings>())
	{
		ResolveQualitySettings(Settings);
		
		// Register for any changes to the settings while we're open in the editor.
		Settings->OnSettingChanged().AddWeakLambda(this, [WeakSource = MakeWeakObjectPtr(this)](UObject* InObj, struct FPropertyChangedEvent& InEvent)
		{
			if (
				WeakSource.IsValid() &&
				InEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, QualitySettings)
			)
			{
				WeakSource->ResolveQualitySettings(CastChecked<UMetaSoundSettings>(InObj));
			}
		});
	}
#endif //WITH_EDITORONLY_DATA

	// Override SampleRate with the Operator settings version which uses our Quality settings.
	SampleRate = GetOperatorSettings(SampleRate).GetSampleRate();
}

void UMetaSoundSource::ResolveQualitySettings(const UMetaSoundSettings* Settings)
{
	const FMetaSoundQualitySettings* Resolved = nullptr;

	// 1. Try and resolve by name. (most should resolve unless its been renamed, deleted).
	auto FindByName = [&Name = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == Name; };
	Resolved = Settings->QualitySettings.FindByPredicate(FindByName);

#if WITH_EDITORONLY_DATA

	// 2. If that failed, try by guid (if its been renamed in the settings, we can still find it).
	if (!Resolved && QualitySettingGuid.IsValid())
	{
		auto FindByGuid = [&Guid = QualitySettingGuid](const FMetaSoundQualitySettings& Q) -> bool { return Q.UniqueId == Guid; };
		Resolved = Settings->QualitySettings.FindByPredicate(FindByName);
	}

	// 3. If still failed to resolve, use defaults and warn.
	if (!Resolved)
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Failed to resolve Quality '%s', resetting to the default."), *QualitySetting.ToString());

		// Reset to defaults. (and make sure they are sane)
		QualitySetting = GetDefault<UMetaSoundSource>()->QualitySetting;
		QualitySettingGuid = GetDefault<UMetaSoundSource>()->QualitySettingGuid;
		if (!Settings->QualitySettings.FindByPredicate(FindByName) && !Settings->QualitySettings.IsEmpty())
		{
			// Default doesn't point to anything, use first one in the list.
			QualitySetting = Settings->QualitySettings[0].Name;
			QualitySettingGuid = Settings->QualitySettings[0].UniqueId;
		}				
	}

	// Refresh the guid/name now we've resolved to correctly reflect.
	if (Resolved)
	{
		QualitySetting = Resolved->Name;
		QualitySettingGuid = Resolved->UniqueId;
	}

#endif //WITH_EDITORONLY_DATA
}

void UMetaSoundSource::InitParameters(TArray<FAudioParameter>& ParametersToInit, FName InFeatureName)
{
	using namespace Metasound::SourcePrivate;
	using namespace Metasound::Frontend;
	using FRuntimeInput = FMetasoundAssetBase::FRuntimeInput;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitParameters);

	// Have to call register vs a simple get as the source may have yet to start playing/has not been registered
	// via InitResources. If it has, this call is fast and returns the already cached RuntimeData.
	RegisterGraphWithFrontend(GetInitRegistrationOptions());
	const FRuntimeData& RuntimeData = GetRuntimeData();

	IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();
	const Metasound::TSortedVertexNameMap<FRuntimeInput>& PublicInputMap = RuntimeData.PublicInputMap;

	// Removes values that are not explicitly defined by the ParamType
	auto Sanitize = [&PublicInputMap](FAudioParameter& Parameter)
	{
		switch (Parameter.ParamType)
		{
			case EAudioParameterType::Trigger:
			{
				Parameter = FAudioParameter(Parameter.ParamName, EAudioParameterType::Trigger);
			}
			break;
			
			case EAudioParameterType::Boolean:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.BoolParam);
			}
			break;

			case EAudioParameterType::BooleanArray:
			{
				TArray<bool> TempArray = Parameter.ArrayBoolParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Float:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.FloatParam);
			}
			break;

			case EAudioParameterType::FloatArray:
			{
				TArray<float> TempArray = Parameter.ArrayFloatParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Integer:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.IntParam);
			}
			break;

			case EAudioParameterType::IntegerArray:
			{
				TArray<int32> TempArray = Parameter.ArrayIntParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::Object:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.ObjectParam);
			}
			break;

			case EAudioParameterType::ObjectArray:
			{
				TArray<UObject*> TempArray = Parameter.ArrayObjectParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;


			case EAudioParameterType::String:
			{
				Parameter = FAudioParameter(Parameter.ParamName, Parameter.StringParam);
			}
			break;

			case EAudioParameterType::StringArray:
			{
				TArray<FString> TempArray = Parameter.ArrayStringParam;
				Parameter = FAudioParameter(Parameter.ParamName, MoveTemp(TempArray));
			}
			break;

			case EAudioParameterType::None:
			default:
			break;
		}
	};

	auto ConstructProxies = [&DataTypeRegistry](FAudioParameter& OutParamToInit, FName VertexTypeName)
	{
		using namespace Metasound;

		switch (OutParamToInit.ParamType)
		{
			case EAudioParameterType::Object:
			{
				TSharedPtr<Audio::IProxyData> ProxyPtr = DataTypeRegistry.CreateProxyFromUObject(VertexTypeName, OutParamToInit.ObjectParam);
				OutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));

				// Null out param as it is no longer needed (nor desired to be accessed once passed to the Audio Thread)
				OutParamToInit.ObjectParam = nullptr;
			}
			break;

			case EAudioParameterType::ObjectArray:
			{
				const FName ElementTypeName = CreateElementTypeNameFromArrayTypeName(VertexTypeName);
				for (TObjectPtr<UObject>& Object : OutParamToInit.ArrayObjectParam)
				{
					TSharedPtr<Audio::IProxyData> ProxyPtr = DataTypeRegistry.CreateProxyFromUObject(ElementTypeName, Object);
					OutParamToInit.ObjectProxies.Emplace(MoveTemp(ProxyPtr));
				}
				// Reset param array as it is no longer needed (nor desired to be accessed once passed to the Audio Thread).
				// All object manipulation hereafter should be done via proxies
				OutParamToInit.ArrayObjectParam.Reset();
			}
			break;

			default:
				break;
		}
	};


	for (int32 i = ParametersToInit.Num() - 1; i >= 0; --i)
	{
		bool bIsParameterValid = false;

		FAudioParameter& Parameter = ParametersToInit[i];
		if (const FRuntimeInput* Input = PublicInputMap.Find(Parameter.ParamName))
		{
			if (IsParameterValid(Parameter, Input->TypeName, DataTypeRegistry))
			{
				Sanitize(Parameter);
				ConstructProxies(Parameter, Input->TypeName);
				bIsParameterValid = true;
			}
		}

		if (!bIsParameterValid)
		{
			constexpr bool bAllowShrinking = false;
			ParametersToInit.RemoveAtSwap(i, 1, bAllowShrinking);

#if !NO_LOGGING
			if (::Metasound::MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
			{
				const FString AssetName = GetName();
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to set parameter '%s' in asset '%s': No name specified, no transmittable input found, or type mismatch."), *Parameter.ParamName.ToString(), *AssetName);
			}
#endif // !NO_LOGGING
		}
	}
}

void UMetaSoundSource::InitResources()
{
	using namespace Metasound::Frontend;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::InitResources);

	RegisterGraphWithFrontend(GetInitRegistrationOptions());
}

bool UMetaSoundSource::IsPlayable() const
{
	// todo: cache off whether this metasound is buildable to an operator.
	return true;
}

bool UMetaSoundSource::SupportsSubtitles() const
{
	return Super::SupportsSubtitles();
}

float UMetaSoundSource::GetDuration() const
{
	// This is an unfortunate function required by logic in determining what sounds can be potentially
	// culled (in this case prematurally). MetaSound OneShots are stopped either by internally logic that
	// triggers OnFinished, or if an external system requests the sound to be stopped. Setting the duration
	// as a "close to" maximum length without being considered looping avoids the MetaSound from being
	// culled inappropriately.
	return IsOneShot() ? INDEFINITELY_LOOPING_DURATION - 1.0f : INDEFINITELY_LOOPING_DURATION;
}

Metasound::Frontend::FDocumentAccessPtr UMetaSoundSource::GetDocumentAccessPtr()
{
	using namespace Metasound::Frontend;

	// Mutation of a document via the soft deprecated access ptr/controller system is not tracked by
	// the builder registry, so the document cache is invalidated here. It is discouraged to mutate
	// documents using both systems at the same time as it can corrupt a builder document's cache.
	if (UMetaSoundBuilderSubsystem* BuilderSubsystem = UMetaSoundBuilderSubsystem::Get())
	{
		const FMetasoundFrontendClassName& Name = RootMetasoundDocument.RootGraph.Metadata.GetClassName();
		BuilderSubsystem->InvalidateDocumentCache(Name);
	}

	// Return document using FAccessPoint to inform the TAccessPtr when the 
	// object is no longer valid.
	return MakeAccessPtr<FDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
}

Metasound::Frontend::FConstDocumentAccessPtr UMetaSoundSource::GetDocumentConstAccessPtr() const
{
	using namespace Metasound::Frontend;

	// Return document using FAccessPoint to inform the TAccessPtr when the 
	// object is no longer valid.
	return MakeAccessPtr<FConstDocumentAccessPtr>(RootMetasoundDocument.AccessPoint, RootMetasoundDocument);
}

bool UMetaSoundSource::ImplementsParameterInterface(Audio::FParameterInterfacePtr InInterface) const
{
	const FMetasoundFrontendVersion Version { InInterface->GetName(), { InInterface->GetVersion().Major, InInterface->GetVersion().Minor } };
	return GetDocumentChecked().Interfaces.Contains(Version);
}

ISoundGeneratorPtr UMetaSoundSource::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams, TArray<FAudioParameter>&& InDefaultParameters)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSource::CreateSoundGenerator);

	FOperatorSettings InSettings = GetOperatorSettings(static_cast<FSampleRate>(InParams.SampleRate));

	SampleRate = InSettings.GetSampleRate();

	FMetasoundEnvironment Environment = CreateEnvironment(InParams);
	FParameterRouter& Router = GetParameterRouter();
	TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel = Router.FindOrCreateDataChannelForReader(InParams.AudioDeviceID, InParams.InstanceID);

	FOperatorBuilderSettings BuilderSettings = FOperatorBuilderSettings::GetDefaultSettings();
	// Graph analyzer currently only enabled for preview sounds (but can theoretically be supported for all sounds)
	BuilderSettings.bPopulateInternalDataReferences = InParams.bIsPreviewSound;

	constexpr bool bBuildSynchronous = false;

	const bool bIsDynamic = DynamicTransactor.IsValid();
	TSharedPtr<FMetasoundGenerator> Generator;

	if (bIsDynamic)
	{
		// In order to ensure synchronization and avoid race conditions the current state
		// of the graph is copied and transform queue created here. This ensures that:
		//
		// 1. Modifications to the underlying FGraph in the FDynamicOperatorTransactor can continue 
		// while the generator is being constructed on an async task.  If this were not ensured, 
		// a race condition would be introduced wherein the FGraph could be manipulated while the
		// graph is being read while building the generator.
		//
		// 2. The state of the FGraph and TransformQueue are synchronized so that any additional
		// changes applied to the FDynamicOperatorTransactor will be placed in the TransformQueue.
		// The dynamic operator & generator will then consume these transforms after it has finished 
		// being built.

		BuilderSettings.bEnableOperatorRebind = true;

		FMetasoundDynamicGraphGeneratorInitParams InitParams
		{
			{
				InSettings,
				MoveTemp(BuilderSettings),
				MakeShared<FGraph>(DynamicTransactor->GetGraph()), // Make a copy of the graph.
				Environment,
				GetName(),
				GetOutputAudioChannelOrder(),
				MoveTemp(InDefaultParameters),
				bBuildSynchronous,
				DataChannel
			},
			DynamicTransactor->CreateTransformQueue(InSettings, Environment) // Create transaction queue
		};
		TSharedPtr<FMetasoundDynamicGraphGenerator> DynamicGenerator = MakeShared<FMetasoundDynamicGraphGenerator>(InSettings);
		DynamicGenerator->Init(MoveTemp(InitParams));

		Generator = MoveTemp(DynamicGenerator);
	}
	else
	{
		TSharedPtr<const IGraph, ESPMode::ThreadSafe> MetasoundGraph = GetRuntimeData().Graph;
		if (!MetasoundGraph.IsValid())
		{
			return ISoundGeneratorPtr(nullptr);
		}

		FMetasoundGeneratorInitParams InitParams
		{
			InSettings,
			MoveTemp(BuilderSettings),
			MetasoundGraph,
			Environment,
			GetName(),
			GetOutputAudioChannelOrder(),
			MoveTemp(InDefaultParameters),
			bBuildSynchronous,
			DataChannel
		};

		Generator = MakeShared<FMetasoundConstGraphGenerator>(MoveTemp(InitParams));
	}

	if (Generator.IsValid())
	{
		TrackGenerator(InParams.AudioComponentId, Generator);
	}

	return ISoundGeneratorPtr(Generator);
}

void UMetaSoundSource::OnEndGenerate(ISoundGeneratorPtr Generator)
{
	using namespace Metasound;
	ForgetGenerator(Generator);
}

bool UMetaSoundSource::GetAllDefaultParameters(TArray<FAudioParameter>& OutParameters) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;

	for(const TPair<FVertexName, FMetasoundAssetBase::FRuntimeInput>& Pair : GetRuntimeData().PublicInputMap)
	{
		const FMetasoundAssetBase::FRuntimeInput& Input = Pair.Value;
		FAudioParameter Params;
		Params.ParamName = Input.Name;
		Params.TypeName = Input.TypeName;

		switch (Input.DefaultLiteral.GetType())
		{
			case EMetasoundFrontendLiteralType::Boolean:
			{
				static const FName TriggerName = "Trigger";
				if (Params.TypeName == TriggerName)
				{
					Params.ParamType = EAudioParameterType::Trigger;
				}
				else
				{
					Params.ParamType = EAudioParameterType::Boolean;
				}
					
				ensure(Input.DefaultLiteral.TryGet(Params.BoolParam));
			}
			break;

			case EMetasoundFrontendLiteralType::BooleanArray:
			{
				Params.ParamType = EAudioParameterType::BooleanArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayBoolParam));
			}
			break;

			case EMetasoundFrontendLiteralType::Integer:
			{
				Params.ParamType = EAudioParameterType::Integer;
				ensure(Input.DefaultLiteral.TryGet(Params.IntParam));
			}
			break;

			case EMetasoundFrontendLiteralType::IntegerArray:
			{
				Params.ParamType = EAudioParameterType::IntegerArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayIntParam));
			}
			break;

			case EMetasoundFrontendLiteralType::Float:
			{
				Params.ParamType = EAudioParameterType::Float;
				ensure(Input.DefaultLiteral.TryGet(Params.FloatParam));
			}
			break;

			case EMetasoundFrontendLiteralType::FloatArray:
			{
				Params.ParamType = EAudioParameterType::FloatArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayFloatParam));
			}
			break;

			case EMetasoundFrontendLiteralType::String:
			{
				Params.ParamType = EAudioParameterType::String;
				ensure(Input.DefaultLiteral.TryGet(Params.StringParam));
			}
			break;

			case EMetasoundFrontendLiteralType::StringArray:
			{
				Params.ParamType = EAudioParameterType::StringArray;
				ensure(Input.DefaultLiteral.TryGet(Params.ArrayStringParam));
			}
			break;

			case EMetasoundFrontendLiteralType::UObject:
			{
				Params.ParamType = EAudioParameterType::Object;
				UObject* Object = nullptr;
				ensure(Input.DefaultLiteral.TryGet(Object));
				Params.ObjectParam = Object;
			}
			break;

			case EMetasoundFrontendLiteralType::UObjectArray:
			{
				Params.ParamType = EAudioParameterType::ObjectArray;
				ensure(Input.DefaultLiteral.TryGet(MutableView(Params.ArrayObjectParam)));
			}
			break;

			default:
			break;
		}

		if (Params.ParamType != EAudioParameterType::None)
		{
			OutParameters.Add(Params);
		}
	}
	return true;
}

bool UMetaSoundSource::IsParameterValid(const FAudioParameter& InParameter) const
{
	const TArray<FMetasoundFrontendClassInput>& Inputs = GetDocumentChecked().RootGraph.Interface.Inputs;
	const FMetasoundFrontendVertex* Vertex = Algo::FindByPredicate(Inputs, [&InParameter] (const FMetasoundFrontendClassInput& Input)
	{
		return Input.Name == InParameter.ParamName;
	});
	
	if (Vertex)
	{
		return IsParameterValid(InParameter, Vertex->TypeName, Metasound::Frontend::IDataTypeRegistry::Get());
	}
	else
	{
		return false;
	}
}

bool UMetaSoundSource::IsParameterValid(const FAudioParameter& InParameter, const FName& InTypeName, Metasound::Frontend::IDataTypeRegistry& InDataTypeRegistry) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	if (InParameter.ParamName.IsNone())
	{
		// Invalid parameter name
		return false;
	}

	if (!InParameter.TypeName.IsNone() && InParameter.TypeName != InTypeName)
	{
		// Mismatched parameter type and vertex data type
		return false;
	}

	// Special handling for UObject proxies
	if (InParameter.ParamType == EAudioParameterType::Object)
	{
		return InDataTypeRegistry.IsValidUObjectForDataType(InTypeName, InParameter.ObjectParam);
	}
	else if (InParameter.ParamType == EAudioParameterType::ObjectArray)
	{
		bool bIsValid = true;

		const FName ElementTypeName = CreateElementTypeNameFromArrayTypeName(InTypeName);
		for (const UObject* Object : InParameter.ArrayObjectParam)
		{
			bIsValid = InDataTypeRegistry.IsValidUObjectForDataType(ElementTypeName, Object);
			if (!bIsValid)
			{
				break;
			}
		}
		return bIsValid;
	}

	const IDataTypeRegistryEntry* RegistryEntry = InDataTypeRegistry.FindDataTypeRegistryEntry(InTypeName);
	if (!RegistryEntry)
	{
		// Unregistered MetaSound data type
		return false;
	}

	switch (InParameter.ParamType)
	{
		case EAudioParameterType::Trigger:
		case EAudioParameterType::Boolean:
		{
			return RegistryEntry->GetDataTypeInfo().bIsBoolParsable;
		}
		break;

		case EAudioParameterType::BooleanArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsBoolArrayParsable;
		}
		break;

		case EAudioParameterType::Float:
		{
			return RegistryEntry->GetDataTypeInfo().bIsFloatParsable;
		}
		break;

		case EAudioParameterType::FloatArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsFloatArrayParsable;
		}
		break;

		case EAudioParameterType::Integer:
		{
			return RegistryEntry->GetDataTypeInfo().bIsIntParsable;
		}
		break;

		case EAudioParameterType::IntegerArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsIntArrayParsable;
		}
		break;

		case EAudioParameterType::String:
		{
			return RegistryEntry->GetDataTypeInfo().bIsStringParsable;
		}
		break;

		case EAudioParameterType::StringArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsStringArrayParsable;
		}
		break;

		case EAudioParameterType::NoneArray:
		{
			return RegistryEntry->GetDataTypeInfo().bIsDefaultArrayParsable;
		}

		case EAudioParameterType::None:
		{
			return RegistryEntry->GetDataTypeInfo().bIsDefaultParsable;
		}
		break;

		default:
		{
			// All parameter types should be covered.
			static_assert(static_cast<uint8>(EAudioParameterType::COUNT) == 13, "Possible unhandled EAudioParameterType");
			checkNoEntry();
			// Unhandled parameter type
			return false;
		}
	}

}

bool UMetaSoundSource::IsLooping() const
{
	return !IsOneShot();
}

bool UMetaSoundSource::IsOneShot() const
{
	using namespace Metasound::Frontend;

	// If the metasound source implements the one-shot interface, then it's a one-shot metasound
	return IsInterfaceDeclared(SourceOneShotInterface::GetVersion());
}

TSharedPtr<Audio::IParameterTransmitter> UMetaSoundSource::CreateParameterTransmitter(Audio::FParameterTransmitterInitParams&& InParams) const
{
	using namespace Metasound;
	using namespace Metasound::SourcePrivate;

	METASOUND_LLM_SCOPE;

	const FRuntimeData& RuntimeData = GetRuntimeData();

	// Build list of parameters that can be set at runtime.
	TArray<FName> ValidParameters;
	for (const TPair<FVertexName, FMetasoundAssetBase::FRuntimeInput>& Pair : RuntimeData.PublicInputMap)
	{
		if (Pair.Value.bIsTransmittable && (Pair.Value.AccessType == EMetasoundFrontendVertexAccessType::Reference))
		{
			ValidParameters.Add(Pair.Value.Name);
		}
	}

	FParameterRouter& Router = GetParameterRouter();
	TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel = Router.FindOrCreateDataChannelForWriter(InParams.AudioDeviceID, InParams.InstanceID);

	Metasound::FMetaSoundParameterTransmitter::FInitParams InitParams(GetOperatorSettings(InParams.SampleRate), InParams.InstanceID, MoveTemp(InParams.DefaultParams), MoveTemp(ValidParameters), DataChannel);
	InitParams.DebugMetaSoundName = GetFName();

	return MakeShared<Metasound::FMetaSoundParameterTransmitter>(MoveTemp(InitParams));
}

Metasound::FOperatorSettings UMetaSoundSource::GetOperatorSettings(Metasound::FSampleRate InSampleRate) const
{	
	if (!OperatorSettings)
	{
		using namespace Metasound;
		using namespace Metasound::SourcePrivate;

		// Lazy Query and cache on the optional.
		auto QueryQualitySettings = [&](Metasound::FSampleRate InSampleRate) -> Metasound::FOperatorSettings
		{
			static const int32 DefaultSampleRateConstant = 48000;
			static const float DefaultBlockRateConstant = 100.f;
			
			// 1. Sensible defaults.
			FSampleRate SampleRate = DefaultSampleRateConstant;
			float BlockRate = DefaultBlockRateConstant;

			// 2. Query CVars. (Override with CVars if they are > 0)
			const float BlockRateCVar = Metasound::Frontend::GetDefaultBlockRate();
			const int32 SampleRateCvar = Metasound::Frontend::GetDefaultSampleRate();

			if (SampleRateCvar != INDEX_NONE)
			{
				SampleRate = SampleRateCvar;
			}
			if (BlockRateCVar > 0)
			{
				BlockRate = BlockRateCVar;
			}

			// 3. Query our quality settings.
			if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
			{
				if (const FMetaSoundQualitySettings* Found = Settings->QualitySettings.FindByPredicate([&QT = QualitySetting](const FMetaSoundQualitySettings& Q) -> bool { return Q.Name == QT; }))
				{
					// Allow partial applications of settings, if some are non-zero.
					if (Found->BlockRate > 0.f)
					{
						BlockRate = Found->BlockRate;
					}
					if (Found->SampleRate > 0.f)
					{
						SampleRate = Found->SampleRate;
					}
				}
			}

			// 4. Do per asset overrides.
			if (const float SerializedBlockRate = BlockRateOverride.GetValue(); SerializedBlockRate > 0.0f)
			{
				BlockRate = SerializedBlockRate;
			}
			if (const int32 SerializedSampleRate = SampleRateOverride.GetValue(); SerializedSampleRate > 0)
			{
				SampleRate = SerializedSampleRate;
			}

			return Metasound::FOperatorSettings(SampleRate, BlockRate);
		};
		OperatorSettings = QueryQualitySettings(InSampleRate);
	}
	return *OperatorSettings;
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment() const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment;
	Environment.SetValue<uint32>(SourceInterface::Environment::SoundUniqueID, GetUniqueID());

	return Environment;
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment(const FSoundGeneratorInitParams& InParams) const
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment = CreateEnvironment();
	Environment.SetValue<bool>(SourceInterface::Environment::IsPreview, InParams.bIsPreviewSound);
	Environment.SetValue<uint64>(SourceInterface::Environment::TransmitterID, InParams.InstanceID);
	Environment.SetValue<Audio::FDeviceId>(SourceInterface::Environment::DeviceID, InParams.AudioDeviceID);
	Environment.SetValue<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames, InParams.AudioMixerNumOutputFrames);

#if WITH_METASOUND_DEBUG_ENVIRONMENT
	Environment.SetValue<FString>(SourceInterface::Environment::GraphName, GetFullName());
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

	return Environment;
}

Metasound::FMetasoundEnvironment UMetaSoundSource::CreateEnvironment(const Audio::FParameterTransmitterInitParams& InParams) const
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	FMetasoundEnvironment Environment = CreateEnvironment();
	Environment.SetValue<uint64>(SourceInterface::Environment::TransmitterID, InParams.InstanceID);

	return Environment;
}

const TArray<Metasound::FVertexName>& UMetaSoundSource::GetOutputAudioChannelOrder() const
{
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	if (const FOutputAudioFormatInfo* FormatInfo = GetOutputAudioFormatInfo().Find(OutputFormat))
	{
		return FormatInfo->OutputVertexChannelOrder;
	}
	else
	{
		// Unhandled audio format. Need to update audio output format vertex key map.
		checkNoEntry();
		static const TArray<Metasound::FVertexName> Empty;
		return Empty;
	}
}

void UMetaSoundSource::TrackGenerator(uint64 Id, TSharedPtr<Metasound::FMetasoundGenerator> Generator)
{
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	Generators.Add(Id, Generator);
	OnGeneratorInstanceCreated.Broadcast(Id, Generator);
}

void UMetaSoundSource::ForgetGenerator(ISoundGeneratorPtr Generator)
{
	using namespace Metasound;
	FMetasoundGenerator* AsMetasoundGenerator = static_cast<FMetasoundGenerator*>(Generator.Get());
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	for (auto It = Generators.begin(); It != Generators.end(); ++It)
	{
		if ((*It).Value.HasSameObject(AsMetasoundGenerator))
		{
			OnGeneratorInstanceDestroyed.Broadcast((*It).Key, StaticCastSharedPtr<Metasound::FMetasoundGenerator>(Generator));
			Generators.Remove((*It).Key);
			return;
		}
	}
}

TWeakPtr<Metasound::FMetasoundGenerator> UMetaSoundSource::GetGeneratorForAudioComponent(uint64 ComponentId) const
{
	using namespace Metasound;
	FScopeLock GeneratorMapLock(&GeneratorMapCriticalSection);
	const TWeakPtr<FMetasoundGenerator>* Result = Generators.Find(ComponentId);
	if (!Result)
	{
		return TWeakPtr<FMetasoundGenerator>(nullptr);
	}
	return *Result;
}

Metasound::SourcePrivate::FParameterRouter& UMetaSoundSource::GetParameterRouter()
{
	using namespace Metasound::SourcePrivate;

	static FParameterRouter Router;
	return Router;
}

TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> UMetaSoundSource::SetDynamicGeneratorEnabled(bool bInIsEnabled)
{
	using namespace Metasound;
	using namespace Metasound::DynamicGraph;

	if (bInIsEnabled)
	{
		if (!DynamicTransactor.IsValid())
		{
			TSharedPtr<FGraph> CurrentGraph = GetRuntimeData().Graph;
			if (CurrentGraph.IsValid())
			{
				DynamicTransactor = MakeShared<FDynamicOperatorTransactor>(*CurrentGraph);
			}
			else
			{
				DynamicTransactor = MakeShared<FDynamicOperatorTransactor>();
			}
		}
	}
	else
	{
		DynamicTransactor.Reset();
	}

	return DynamicTransactor;
}

TSharedPtr<Metasound::DynamicGraph::FDynamicOperatorTransactor> UMetaSoundSource::GetDynamicGeneratorTransactor() const
{
	return DynamicTransactor;
}
#undef LOCTEXT_NAMESPACE // MetaSound
