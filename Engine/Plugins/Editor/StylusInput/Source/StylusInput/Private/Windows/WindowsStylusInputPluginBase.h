// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <HAL/Platform.h>

#if PLATFORM_WINDOWS

#include <StylusInputPacket.h>
#include <Delegates/Delegate.h>

#include "WindowsStylusInputStats.h"
#include "WindowsStylusInputTabletContext.h"

#include <Windows/AllowWindowsPlatformTypes.h>
	#include <Microsoft/COMPointer.h>
	#include <RTSCom.h>
#include <Windows/HideWindowsPlatformTypes.h>

namespace UE::StylusInput
{
	class IStylusInputEventHandler;
}

namespace UE::StylusInput::Private::Windows
{
	class FWindowsStylusInputPlatformAPI;

	DECLARE_DELEGATE_RetVal(const FWindowContext&, FGetWindowContextCallback);
	DECLARE_DELEGATE_OneParam(FUpdateTabletContextsCallback, const FTabletContextContainer&);

	class FWindowsStylusInputPluginBase
	{
	public:
		bool AddEventHandler(IStylusInputEventHandler* EventHandler);
		bool RemoveEventHandler(IStylusInputEventHandler* EventHandler);
		int32 NumEventHandlers() const { return EventHandlers.Num(); }

		float GetPacketsPerSecond() const { return PacketStats.GetPacketsPerSecond(); }

	protected:
		FWindowsStylusInputPluginBase(FGetWindowContextCallback&& GetWindowContextCallback, FUpdateTabletContextsCallback&& UpdateTabletContextsCallback);
		~FWindowsStylusInputPluginBase() = default;

		virtual FString GetName() const = 0;

		void DebugEvent(const FString& Message) const;

		HRESULT ProcessRealTimeStylusEnabled(IRealTimeStylus* RealTimeStylus, uint32 TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs);
		HRESULT ProcessRealTimeStylusDisabled(IRealTimeStylus* RealTimeStylus, uint32 TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs);
		HRESULT ProcessPackets(const StylusInfo* StylusInfo, uint32 PacketCount, uint32 PacketBufferLength, EPacketType Type, const int32* PacketBuffer);
		FString ProcessError(RealTimeStylusDataInterest DataInterest, HRESULT ErrorCode);
		HRESULT ProcessTabletAdded(IInkTablet* Tablet);
		HRESULT ProcessTabletRemoved(LONG TabletIndex);

	private:
		const FPacketProperty* GetPacketDescriptions(uint32 TabletContextId) const;
		HRESULT UpdateTabletContexts(IRealTimeStylus* RealTimeStylus, uint32 TabletContextIDsNum, const TABLET_CONTEXT_ID* TabletContextIDs);

		FPacketStats PacketStats;

		FGetWindowContextCallback GetWindowContextCallback;
		FUpdateTabletContextsCallback UpdateTabletContextsCallback;

		FTabletContextContainer TabletContexts;

		TArray<IStylusInputEventHandler*> EventHandlers;
	};
}

#define STYLUSINPUT_SHOW_NOTIMPL_ERRORS 0

#endif
