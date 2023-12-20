// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <memory>
/**
 * 
 */

DECLARE_LOG_CATEGORY_EXTERN(LogRenderDocMixer, Log, All);

namespace MixerEditor
{
	class MIXERENGINE_API RenderDocManager
	{

	public:
		RenderDocManager();
		~RenderDocManager();
		void												Initialize();

		void												CaptureNextBatch();
		void												CapturePreviousBatch();
		void												BeginCapture();
		void												EndCapture();
};
	typedef std::unique_ptr<RenderDocManager>				RenderDocManagerPtr;
}