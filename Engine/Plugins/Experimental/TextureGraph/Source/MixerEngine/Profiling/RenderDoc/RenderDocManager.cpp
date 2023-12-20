// Copyright Epic Games, Inc. All Rights Reserved.
#include "RenderDocManager.h"

#include "Misc/AutomationTest.h"
#include "Job/Scheduler.h"
#include <IRenderDocPlugin.h>

#include "MixerEngine.h"
#include "RenderingThread.h"
//#include "RenderDocPluginModule.h"

DEFINE_LOG_CATEGORY(LogRenderDocMixer);

namespace MixerEditor
{


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Public Interface
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	RenderDocManager::RenderDocManager()
	{
		// Disabling for the time being
#if 0
		static FAutoConsoleCommand CCmdRenderDocCaptureFrame = FAutoConsoleCommand(
			TEXT("renderdoc.Mixer_CaptureNextBatch"),
			TEXT("Captures the next Job Batch and launches RenderDoc"),
			FConsoleCommandDelegate::CreateRaw(this, &RenderDocManager::CaptureNextBatch)
		);

		static FAutoConsoleCommand CCmdRenderDocCapturePIE = FAutoConsoleCommand(
			TEXT("renderdoc.Mixer_CapturePrevBatch"),
			TEXT("Captures the previous Job Batch and launches RenderDoc"),
			FConsoleCommandDelegate::CreateRaw(this, &RenderDocManager::CapturePreviousBatch)
		);
#endif 
	}

	RenderDocManager::~RenderDocManager()
	{
	}


	void RenderDocManager::CaptureNextBatch()
	{
		// Disabling for the time being
#if 0
		MixerEngine::GetScheduler()->SetCaptureRenderDocNextBatch(true);
#endif 
	}

	void RenderDocManager::CapturePreviousBatch()
	{
		// Disabling for the time being
#if 0
		MixerEngine::GetScheduler()->CaptureRenderDocLastRunBatch();
#endif 
	}

	void RenderDocManager::BeginCapture()
	{
		// Disabling for the time being
#if 0
		ENQUEUE_RENDER_COMMAND(BeginCaptureCommand)([this](FRHICommandListImmediate& RHICommandList)
			{
				IRenderDocPlugin& PluginModule = FModuleManager::GetModuleChecked<IRenderDocPlugin>("RenderDocPlugin");
				PluginModule.BeginCapture(&RHICommandList, IRenderCaptureProvider::ECaptureFlags_Launch, FString("TextureGraph"));
			});
		/*PluginModule.CaptureFrame(nullptr, IRenderCaptureProvider::ECaptureFlags_Launch, FString());*/
#endif 
	}

	void RenderDocManager::EndCapture()
	{
		// Disabling for the time being
#if 0
		ENQUEUE_RENDER_COMMAND(EndnCaptureCommand)([this](FRHICommandListImmediate& RHICommandList)
			{
				IRenderDocPlugin& PluginModule = FModuleManager::GetModuleChecked<IRenderDocPlugin>("RenderDocPlugin");
				PluginModule.EndCapture(&RHICommandList);
			});
#endif 
	}
}