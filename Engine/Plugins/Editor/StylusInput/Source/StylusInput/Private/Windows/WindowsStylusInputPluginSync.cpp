// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WindowsStylusInputPluginSync.h"

#if PLATFORM_WINDOWS

#include "WindowsStylusInputPlatformAPI.h"

namespace UE::StylusInput::Private::Windows
{
	FWindowsStylusInputPluginSync::FWindowsStylusInputPluginSync(FGetWindowContextCallback&& GetWindowContext,
	                                                   FUpdateTabletContextsCallback&& UpdateTabletContextsCallback)
		: FWindowsStylusInputPluginBase(MoveTemp(GetWindowContext), MoveTemp(UpdateTabletContextsCallback))
	{
	}

	FWindowsStylusInputPluginSync::~FWindowsStylusInputPluginSync()
	{
		if (FreeThreadedMarshaler)
		{
			FreeThreadedMarshaler->Release();
		}
	}

	HRESULT FWindowsStylusInputPluginSync::CreateFreeThreadMarshaler()
	{
		check(FreeThreadedMarshaler == nullptr);

		const FWindowsStylusInputPlatformAPI& WindowsAPI = FWindowsStylusInputPlatformAPI::GetInstance();
		return WindowsAPI.CoCreateFreeThreadedMarshaler(this, &FreeThreadedMarshaler);
	}

	HRESULT FWindowsStylusInputPluginSync::RealTimeStylusEnabled(IRealTimeStylus* RealTimeStylus, ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs)
	{
		return ProcessRealTimeStylusEnabled(RealTimeStylus, TabletContextIDsCount, TabletContextIDs);
	}

	HRESULT FWindowsStylusInputPluginSync::RealTimeStylusDisabled(IRealTimeStylus* RealTimeStylus, ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs)
	{
		return ProcessRealTimeStylusDisabled(RealTimeStylus, TabletContextIDsCount, TabletContextIDs);
	}

	HRESULT FWindowsStylusInputPluginSync::StylusDown(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PropertyCount, LONG* PacketBuffer, LONG**)
	{
		return ProcessPackets(StylusInfo, 1, PropertyCount, EPacketType::StylusDown, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginSync::StylusUp(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PropertyCount, LONG* PacketBuffer, LONG**)
	{
		return ProcessPackets(StylusInfo, 1, PropertyCount, EPacketType::StylusUp, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginSync::TabletAdded(IRealTimeStylus* RealTimeStylus, IInkTablet* Tablet)
	{
		return ProcessTabletAdded(Tablet);
	}

	HRESULT FWindowsStylusInputPluginSync::TabletRemoved(IRealTimeStylus* RealTimeStylus, LONG TabletIndex)
	{
		return ProcessTabletRemoved(TabletIndex);
	}

	HRESULT FWindowsStylusInputPluginSync::InAirPackets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketCount, ULONG PacketBufferLength,
													LONG* PacketBuffer, ULONG*, LONG**)
	{
		return ProcessPackets(StylusInfo, PacketCount, PacketBufferLength, EPacketType::AboveDigitizer, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginSync::Packets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, const ULONG PacketCount,
	                                          const ULONG PacketBufferLength, LONG* PacketBuffer, ULONG*, LONG**)
	{
		return ProcessPackets(StylusInfo, PacketCount, PacketBufferLength, EPacketType::OnDigitizer, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginSync::StylusInRange(IRealTimeStylus* RealTimeStylus, TABLET_CONTEXT_ID TabletContextID, STYLUS_ID StylusID)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::StylusOutOfRange(IRealTimeStylus* RealTimeStylus, TABLET_CONTEXT_ID TabletContextID, STYLUS_ID StylusID)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::StylusButtonDown(IRealTimeStylus* piRtsSrc, STYLUS_ID sid, const GUID* pGuidStylusButton, POINT* pStylusPos)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::StylusButtonUp(IRealTimeStylus* piRtsSrc, STYLUS_ID sid, const GUID* pGuidStylusButton, POINT* pStylusPos)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::CustomStylusDataAdded(IRealTimeStylus* piRtsSrc, const GUID* pGuidId, ULONG cbData, const BYTE* pbData)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::SystemEvent(IRealTimeStylus* piRtsSrc, TABLET_CONTEXT_ID tcid, STYLUS_ID sid, SYSTEM_EVENT event, SYSTEM_EVENT_DATA eventdata)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::Error(IRealTimeStylus*, IStylusPlugin* Plugin, const RealTimeStylusDataInterest DataInterest, const HRESULT ErrorCode, LONG_PTR*)
	{
#if STYLUSINPUT_SHOW_NOTIMPL_ERRORS
		const FString ErrorMessage = ProcessError(DataInterest, ErrorCode);
		DebugEvent(ErrorMessage);
#endif

		return S_OK;
	}

	HRESULT FWindowsStylusInputPluginSync::UpdateMapping(IRealTimeStylus* piRtsSrc)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginSync::DataInterest(RealTimeStylusDataInterest* pDataInterest)
	{
		*pDataInterest = RTSDI_AllData;
		return S_OK;
	}
}
#endif
