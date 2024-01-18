// Copyright Epic Games, Inc. All Rights Reserved.
#include "MixerInsight.h"
#include "Modules/ModuleManager.h"
#include "TextureGraphEngine.h"

#include "Model/MixerInsightObserver.h"
#include "Model/MixerInsightSession.h"
#include "Shader.h"

IMPLEMENT_MODULE(FMixerInsightModule, MixerInsight);
DEFINE_LOG_CATEGORY(LogMixerInsight);

void FMixerInsightModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();
}


void FMixerInsightModule::ShutdownModule()
{
	FShaderType::Uninitialize();
}
//////////////////////////////////////////////////////////////////////////

MixerInsight* MixerInsight::GInstance = nullptr;

MixerInsight::MixerInsight()
{
	UE_LOG(LogMixerInsight, Log, TEXT("Initialising the MixerInsight!"));

	Session = std::make_shared<MixerInsightSession>();
}

MixerInsight::~MixerInsight()
{
	UE_LOG(LogMixerInsight, Log, TEXT("Destroying the MixerInsight!"));
}

bool MixerInsight::Create()
{
	/// Cannot create a new instance before destroying the old one
	if (!GInstance)
	{
		GInstance = new MixerInsight();

		/// Need the engine observer to watch what is happening
		auto EngineObserver = std::make_shared<MixerInsightEngineObserver>();
		TextureGraphEngine::RegisterObserverSource(EngineObserver); // will also install other observers

		return true;
	}
	return false;
}

bool MixerInsight::Destroy()
{
	/// Destroy an existing instance, no op otherwise
	if (GInstance)
	{
		/// Remove Engine observer
		TextureGraphEngine::RegisterObserverSource(nullptr);

		delete GInstance;
		GInstance = nullptr;

		return true;
	}

	return false;
}
