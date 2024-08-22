// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <HAL/Platform.h>

#if PLATFORM_WINDOWS

#include "IStylusInputModule.h"
#include "StylusInput.h"
#include "StylusInputPacket.h"

class SWindow;

/**
 * An implementation of the deprecated @see UStylusInputSubsystem interface by means of using the new interface and Windows implementation.
 */
class FDeprecatedWindowsStylusInputInterface : public IStylusInputInterfaceInternal, public TSharedFromThis<FDeprecatedWindowsStylusInputInterface>
{
public:
	FDeprecatedWindowsStylusInputInterface();
	virtual ~FDeprecatedWindowsStylusInputInterface();

	virtual void Tick() override;
	virtual int32 NumInputDevices() const override;
	virtual IStylusInputDevice* GetInputDevice(int32 Index) const override;

private:
	void CreateStylusInputInstance(SWindow* Window);
	void RemoveStylusInputInstance(const TSharedRef<SWindow>& Window);

	struct FDeprecatedStylusInputDevice : IStylusInputDevice
	{
		explicit FDeprecatedStylusInputDevice(uint32 TabletContextId, const TSharedPtr<UE::StylusInput::IStylusInputTabletContext>& TabletContext);

		void SetDirty() { Dirty = true; }
		virtual void Tick() override;

		uint32 TabletContextId;

		UE::StylusInput::FStylusInputPacket LastPacket;
	};

	class FStylusInputEventHandler : public UE::StylusInput::IStylusInputEventHandler
	{
	public:
		explicit FStylusInputEventHandler(UE::StylusInput::IStylusInputInstance* Instance, TArray<FDeprecatedStylusInputDevice>& TabletContexts);
		virtual ~FStylusInputEventHandler() override = default;
		virtual FString GetName() override { return "DeprecatedWindowsStylusInputInterfaceEventHandler"; }
		virtual void OnPacket(const UE::StylusInput::FStylusInputPacket& Packet) override;
		virtual void OnDebugEvent(const FString& Message) override;

	private:
		FDeprecatedStylusInputDevice* GetTabletContext(uint32 TabletContextId);

		UE::StylusInput::IStylusInputInstance* const Instance;
		TArray<FDeprecatedStylusInputDevice>& TabletContexts;
	};

	class FStylusInputInstanceWrapper
	{
	public:
		explicit FStylusInputInstanceWrapper(SWindow* Window, TArray<FDeprecatedStylusInputDevice>& TabletContexts);
		~FStylusInputInstanceWrapper();

		UE::StylusInput::IStylusInputInstance* Instance;
		FStylusInputEventHandler EventHandler;
	};

	TMap<SWindow*, TUniquePtr<FStylusInputInstanceWrapper>> StylusInputInstances;
	TUniquePtr<TArray<FDeprecatedStylusInputDevice>> TabletContexts;
};

#endif
