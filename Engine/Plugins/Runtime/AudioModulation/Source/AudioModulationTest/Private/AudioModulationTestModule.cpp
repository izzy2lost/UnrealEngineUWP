// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace AudioModulation 
{
	class AUDIOMODULATIONTEST_API FTestModule : public IModuleInterface
	{
	};
} // namespace AudioModulation

IMPLEMENT_MODULE(AudioModulation::FTestModule, AudioModulationTest);
