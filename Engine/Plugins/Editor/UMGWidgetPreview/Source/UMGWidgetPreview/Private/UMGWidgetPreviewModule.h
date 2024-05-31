// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

namespace UE::UMGWidgetPreview::Private
{
	class FUMGWidgetPreviewModule
		: public IModuleInterface
	{
	public:
		//~ Begin IModuleInterface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface

	private:
		void RegisterMenus();
	};
}
