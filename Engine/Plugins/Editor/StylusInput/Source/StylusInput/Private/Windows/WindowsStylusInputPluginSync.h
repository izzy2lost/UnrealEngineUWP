// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS

#include "WindowsStylusInputPluginBase.h"

namespace UE::StylusInput::Private::Windows
{
	class FWindowsStylusInputPluginSync final : public IStylusSyncPlugin, public FWindowsStylusInputPluginBase
	{
	public:
		explicit FWindowsStylusInputPluginSync(FGetWindowContextCallback&& GetWindowContext, FUpdateTabletContextsCallback&& UpdateTabletContextsCallback);
		~FWindowsStylusInputPluginSync();
		
		HRESULT CreateFreeThreadMarshaler();

		// IUnknown
		virtual HRESULT QueryInterface(REFIID InterfaceID, void** InterfaceObject) override
		{
			if (InterfaceID == IID_IStylusSyncPlugin || InterfaceID == IID_IUnknown)
			{
				*InterfaceObject = this;
				AddRef();
				return S_OK;
			}

			if (InterfaceID == IID_IMarshal && FreeThreadedMarshaler)
			{
				return FreeThreadedMarshaler->QueryInterface(InterfaceID, InterfaceObject);
			}

			*InterfaceObject = nullptr;
			return E_NOINTERFACE;
		}

		virtual ULONG AddRef() override
		{
			return ++RefCount;
		}

		virtual ULONG Release() override
		{
			const int32 NewRefCount = --RefCount;
			if (NewRefCount == 0)
				delete this;

			return NewRefCount;
		}

		// IStylusPlugin
		virtual HRESULT RealTimeStylusEnabled(IRealTimeStylus* RealTimeStylus, ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs) override;
		virtual HRESULT RealTimeStylusDisabled(IRealTimeStylus* RealTimeStylus, ULONG TabletContextIDsCount, const TABLET_CONTEXT_ID* TabletContextIDs) override;
		virtual HRESULT StylusDown(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PropertyCount, LONG* PacketBuffer, LONG** ppInOutPkt) override;
		virtual HRESULT StylusUp(IRealTimeStylus* piRtsSrc, const StylusInfo* pStylusInfo, ULONG cPropCountPerPkt, LONG* pPacket, LONG** ppInOutPkt) override;
		virtual HRESULT TabletAdded(IRealTimeStylus* RealTimeStylus, IInkTablet* Tablet) override;
		virtual HRESULT TabletRemoved(IRealTimeStylus* RealTimeStylus, LONG TabletIndex) override;
		virtual HRESULT InAirPackets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketCount, ULONG PacketBufferLength, LONG* PacketBuffer, ULONG* InOutPacketCount, LONG** InOutPacketBuffer) override;
		virtual HRESULT Packets(IRealTimeStylus* RealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketCount, ULONG PacketBufferLength, LONG* PacketBuffer, ULONG* InOutPacketCount, LONG** InOutPacketBuffer) override;
		virtual HRESULT StylusInRange(IRealTimeStylus* piRtsSrc, TABLET_CONTEXT_ID tcid, STYLUS_ID sid) override;
		virtual HRESULT StylusOutOfRange(IRealTimeStylus* piRtsSrc, TABLET_CONTEXT_ID tcid, STYLUS_ID sid) override;
		virtual HRESULT StylusButtonDown(IRealTimeStylus* piRtsSrc, STYLUS_ID sid, const GUID* pGuidStylusButton, POINT* pStylusPos) override;
		virtual HRESULT StylusButtonUp(IRealTimeStylus* piRtsSrc, STYLUS_ID sid, const GUID* pGuidStylusButton, POINT* pStylusPos) override;
		virtual HRESULT CustomStylusDataAdded(IRealTimeStylus* piRtsSrc, const GUID* pGuidId, ULONG cbData, const BYTE* pbData) override;
		virtual HRESULT SystemEvent(IRealTimeStylus* piRtsSrc, TABLET_CONTEXT_ID tcid, STYLUS_ID sid, SYSTEM_EVENT event, SYSTEM_EVENT_DATA eventdata) override;
		virtual HRESULT Error(IRealTimeStylus* RealTimeStylus, IStylusPlugin* Plugin, RealTimeStylusDataInterest DataInterest, HRESULT ErrorCode, LONG_PTR* InternalKey) override;
		virtual HRESULT UpdateMapping(IRealTimeStylus* piRtsSrc) override;
		virtual HRESULT DataInterest(RealTimeStylusDataInterest* pDataInterest) override;

	protected:
		// FWindowsStylusInputPluginBase
		virtual FString GetName() const override { return "Asynchronous"; }
		
	private:
		int32 RefCount = 1;

		IUnknown* FreeThreadedMarshaler = nullptr;
	};
}

#endif
