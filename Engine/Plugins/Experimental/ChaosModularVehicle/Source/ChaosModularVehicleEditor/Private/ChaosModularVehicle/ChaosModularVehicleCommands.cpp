// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ChaosModularVehicleCommands.h"

#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleActor.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Internationalization/Regex.h"

#include "Logging/LogMacros.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "SceneOutlinerDelegates.h"


DEFINE_LOG_CATEGORY_STATIC(UChaosModularVehicleCommandsLogging, NoLogging, All);



void FChaosModularVehicleCommands::ImportFile(const FString& File)
{
}
