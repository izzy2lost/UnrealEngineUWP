// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundOperatorInterface.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"
#include "Sound/SoundGenerator.h"
#include "Subsystems/AudioEngineSubsystem.h"

#include "MetasoundSource.h"

#include "MetasoundOperatorCacheSubsystem.generated.h"


/**
* 	UMetaSoundCacheSubsystem
*/
UCLASS()
class METASOUNDENGINE_API UMetaSoundCacheSubsystem : public UAudioEngineSubsystem
{
	GENERATED_BODY()

public: 
	//~ Begin USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Update() override; // todo: compile out
	//~ End USubsystem interface
	 
	/* Builds the requested number of MetaSound operators (asynchronously) and puts them in the pool for playback.
	(If these operators are not yet available when the MetaSound attepmts to play, one will be created Independent of this request.) */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PrecacheMetaSound(UMetaSoundSource* InMetaSound, int32 InNumInstances = 1);

private:
	FSoundGeneratorInitParams BuildParams;
}; //UMetaSoundCacheSubsystem

