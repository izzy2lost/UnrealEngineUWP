// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCamera.h"

#include "AdvancedWidgetsModule.h"

DEFINE_LOG_CATEGORY(LogVirtualCamera);

namespace UE::VirtualCamera
{
	void FVirtualCameraModuleImpl::StartupModule()
	{
		// Loads widgets (ex. RadialSlider) that are potentially referenced by assets
		FModuleManager::Get().LoadModuleChecked<FAdvancedWidgetsModule>("AdvancedWidgets");
	}

	void FVirtualCameraModuleImpl::ShutdownModule()
	{}
}

IMPLEMENT_MODULE(UE::VirtualCamera::FVirtualCameraModuleImpl, VirtualCamera)