// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IVCamCoreModule.h"
#include "Modules/ModuleManager.h"

namespace UE::VCamCore::WidgetSnapshotUtils
{
	struct FWidgetSnapshotSettings;
}

namespace UE::VCamCore
{
	class FVCamCoreModule : public IVCamCoreModule
	{
	public:

		static FVCamCoreModule& Get()
		{
			return FModuleManager::Get().GetModuleChecked<FVCamCoreModule>("VCamCore");
		}

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		/** Register the module's settings object. */
		void RegisterSettings();

		/** Unregister the module's settings object. */
		void UnregisterSettings();

		WidgetSnapshotUtils::FWidgetSnapshotSettings GetSnapshotSettings() const;
	};
}

