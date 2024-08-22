// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsStylusInputPluginAsync.h"

#if PLATFORM_WINDOWS

namespace UE::StylusInput::Private::Windows
{
	FWindowsStylusInputPluginAsync::FWindowsStylusInputPluginAsync(FGetWindowContextCallback&& GetWindowContext,
	                                                     FUpdateTabletContextsCallback&& UpdateTabletContextsCallback)
		: FWindowsStylusInputPluginBase(MoveTemp(GetWindowContext), MoveTemp(UpdateTabletContextsCallback))
	{
	}

	FWindowsStylusInputPluginAsync::~FWindowsStylusInputPluginAsync()
	{
	}

	HRESULT FWindowsStylusInputPluginAsync::RealTimeStylusEnabled(IRealTimeStylus* RealTimeStylus, const ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs)
	{
		return ProcessRealTimeStylusEnabled(RealTimeStylus, TabletContextIDsCount, TabletContextIDs);
	}

	HRESULT FWindowsStylusInputPluginAsync::RealTimeStylusDisabled(IRealTimeStylus* RealTimeStylus, const ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs)
	{
		return ProcessRealTimeStylusDisabled(RealTimeStylus, TabletContextIDsCount, TabletContextIDs);
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusDown(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, const ULONG PropertyCount, LONG* PacketBuffer, LONG**)
	{
		return ProcessPackets(StylusInfo, 1, PropertyCount, EPacketType::StylusDown, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusUp(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, const ULONG PropertyCount, LONG* PacketBuffer, LONG**)
	{
		return ProcessPackets(StylusInfo, 1, PropertyCount, EPacketType::StylusUp, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginAsync::TabletAdded(IRealTimeStylus* RealTimeStylus, IInkTablet* Tablet)
	{
		return ProcessTabletAdded(Tablet);
	}

	HRESULT FWindowsStylusInputPluginAsync::TabletRemoved(IRealTimeStylus* RealTimeStylus, LONG TabletIndex)
	{
		return ProcessTabletRemoved(TabletIndex);
	}

	HRESULT FWindowsStylusInputPluginAsync::InAirPackets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, const ULONG PacketCount,
	                                                const ULONG PacketBufferLength, LONG* PacketBuffer, ULONG*, LONG**)
	{
		return ProcessPackets(StylusInfo, PacketCount, PacketBufferLength, EPacketType::AboveDigitizer, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginAsync::Packets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, const ULONG PacketCount,
													const ULONG PacketBufferLength, LONG* PacketBuffer, ULONG*, LONG**)
	{
		return ProcessPackets(StylusInfo, PacketCount, PacketBufferLength, EPacketType::OnDigitizer, reinterpret_cast<int32*>(PacketBuffer));
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusInRange(IRealTimeStylus* RealTimeStylus, TABLET_CONTEXT_ID TabletContextID, STYLUS_ID StylusID)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusOutOfRange(IRealTimeStylus* RealTimeStylus, TABLET_CONTEXT_ID TabletContextID, STYLUS_ID StylusID)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusButtonDown(IRealTimeStylus* RealTimeStylus, STYLUS_ID StylusID, const GUID* pGuidStylusButton, POINT* pStylusPos)
	{		
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::StylusButtonUp(IRealTimeStylus* RealTimeStylus, STYLUS_ID StylusID, const GUID* pGuidStylusButton, POINT* pStylusPos)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::CustomStylusDataAdded(IRealTimeStylus* piRtsSrc, const GUID* pGuidId, ULONG cbData, const BYTE* pbData)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::SystemEvent(IRealTimeStylus* piRtsSrc, TABLET_CONTEXT_ID tcid, STYLUS_ID sid, SYSTEM_EVENT event, SYSTEM_EVENT_DATA eventdata)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::Error(IRealTimeStylus*, IStylusPlugin* Plugin, const RealTimeStylusDataInterest DataInterest, const HRESULT ErrorCode, LONG_PTR*)
	{
#if STYLUSINPUT_SHOW_NOTIMPL_ERRORS
		const FString ErrorMessage = ProcessError(DataInterest, ErrorCode);
		DebugEvent(ErrorMessage);
#endif

		return S_OK;
	}

	HRESULT FWindowsStylusInputPluginAsync::UpdateMapping(IRealTimeStylus* piRtsSrc)
	{
		return E_NOTIMPL;
	}

	HRESULT FWindowsStylusInputPluginAsync::DataInterest(RealTimeStylusDataInterest* pDataInterest)
	{
		*pDataInterest = RTSDI_AllData;
		return S_OK;
	}
}
#endif
