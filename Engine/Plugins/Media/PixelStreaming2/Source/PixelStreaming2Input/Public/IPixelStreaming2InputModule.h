// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"
#include "IPixelStreaming2InputHandler.h"

/**
 * The public interface of the Pixel Streaming Input module.
 */
class PIXELSTREAMING2INPUT_API IPixelStreaming2InputModule : public IInputDeviceModule
{
public:
	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IPixelStreaming2InputModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IPixelStreaming2InputModule>("PixelStreaming2Input");
	}

	/**
	 * Checks to see if this module is loaded.
	 *
	 * @return True if the module is loaded.
	 */
	static inline bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded("PixelStreaming2Input"); }

	/**
	 * @brief Create a Input Handler object
	 *
	 * @return TSharedPtr<IPixelStreaming2InputHandler> the input handler for this streamer
	 */
	virtual TSharedPtr<IPixelStreaming2InputHandler> CreateInputHandler() = 0;

	/**
	 * Attempts to create a new input device interface
	 *
	 * @return	Interface to the new input device, if we were able to successfully create one
	 */
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override = 0;
};
