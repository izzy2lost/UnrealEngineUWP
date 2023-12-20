// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <memory>
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMixerInsight, Log, All);

class MixerInsightSchedulerObserver;
class MixerInsightSession;
using MixerInsightSessionPtr = std::shared_ptr<MixerInsightSession>;

class FMixerInsightModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

struct RecordID;

class MIXERINSIGHT_API MixerInsight
{
private:
	static MixerInsight* GInstance;					/// Static instance. Use Create to create a new instance. Destroy the current instance first
	MixerInsight();
	~MixerInsight();

	MixerInsightSessionPtr					Session;
public:
	static bool								Create();  /// Create the Insight Instance ONLY if no other currently created AND if a MixerEngine is already created

	/// Destroy the current instance
	static bool								Destroy(); /// Destroy the instance of Insight if it exists.

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE static MixerInsight*		Instance() { return GInstance; }

	FORCEINLINE MixerInsightSessionPtr		GetSession() const { return Session; }
};

// Macro to control in one place how we display hash value
//#define HashToFString(h) FString::Printf(TEXT("%20llu"), (h))
#define HashToFString(h) FString::Printf(TEXT("%llX"), (h))
