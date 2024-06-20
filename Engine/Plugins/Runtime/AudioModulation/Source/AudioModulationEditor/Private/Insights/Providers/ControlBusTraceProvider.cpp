// Copyright Epic Games, Inc. All Rights Reserved.
#include "Insights/Providers/ControlBusTraceProvider.h"

#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ModuleService.h"


namespace AudioModulationEditor
{
	FName FControlBusTraceProvider::GetName_Static()
	{
		return "ControlBusProvider";
	}

	bool FControlBusTraceProvider::ProcessMessages()
	{
		auto BumpEntryFunc = [this](const FControlBusMessageBase& Msg)
		{
			TSharedPtr<FControlBusDashboardEntry>* ToReturn = nullptr;
			UpdateDeviceEntry(Msg.DeviceId, Msg.ControlBusId, [&ToReturn, &Msg](TSharedPtr<FControlBusDashboardEntry>& Entry)
				{
					if (!Entry.IsValid())
					{
						Entry = MakeShared<FControlBusDashboardEntry>();
						Entry->DeviceId = Msg.DeviceId;
						Entry->ControlBusId = Msg.ControlBusId;
					}
					ToReturn = &Entry;
				});

			return ToReturn;
		};

		ProcessMessageQueue<FControlBusActivateMessage>(TraceMessages.ActivateMessages, BumpEntryFunc,
		[](const FControlBusActivateMessage& Msg, TSharedPtr<FControlBusDashboardEntry>* OutEntry)
		{
			FControlBusDashboardEntry& EntryRef = *OutEntry->Get();
			EntryRef.Name = *Msg.BusName;
			EntryRef.ControlBusId = Msg.ControlBusId;
			EntryRef.ParamName = Msg.ParamName;
		});

		ProcessMessageQueue<FControlBusUpdateMessage>(TraceMessages.UpdateMessages, BumpEntryFunc,
		[](const FControlBusUpdateMessage& Msg, TSharedPtr<FControlBusDashboardEntry>* OutEntry)
		{
			FControlBusDashboardEntry& EntryRef = *OutEntry->Get();
			EntryRef.Value = Msg.Value;
		});

		auto GetEntry = [this](const FControlBusMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.ControlBusId);
		};

		ProcessMessageQueue<FControlBusDeactivateMessage>(TraceMessages.DeactivateMessages, GetEntry,
		[this](const FControlBusDeactivateMessage& Msg, TSharedPtr<FControlBusDashboardEntry>* OutEntry)
		{
			if (OutEntry && (*OutEntry)->Timestamp < Msg.Timestamp)
			{
				RemoveDeviceEntry(Msg.DeviceId, Msg.ControlBusId);
			}
		});

		return true;
	}

	UE::Trace::IAnalyzer* FControlBusTraceProvider::ConstructAnalyzer()
	{
		class FControlBusTraceAnalyzer : public UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase
		{
		public:
			FControlBusTraceAnalyzer(TSharedRef<FControlBusTraceProvider> InProvider)
				: UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase(InProvider)
			{
			}

			virtual void OnAnalysisBegin(const UE::Trace::IAnalyzer::FOnAnalysisContext& Context) override
			{
				UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase::OnAnalysisBegin(Context);

				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;
				Builder.RouteEvent(RouteId_Activate, "Audio", "ControlBusActivate");
				Builder.RouteEvent(RouteId_Deactivate, "Audio", "ControlBusDeactivate");
				Builder.RouteEvent(RouteId_Update, "Audio", "ControlBusUpdate");
			}

			virtual bool OnEvent(uint16 RouteId, UE::Trace::IAnalyzer::EStyle Style, const UE::Trace::IAnalyzer::FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FControlBusTraceAnalyzer"));

				FControlBusMessages& Messages = GetProvider<FControlBusTraceProvider>().TraceMessages;
				switch (RouteId)
				{
					case RouteId_Activate:
					{
						Messages.ActivateMessages.Enqueue(FControlBusActivateMessage { Context });
						break;
					}

					case RouteId_Deactivate:
					{
						Messages.DeactivateMessages.Enqueue(FControlBusDeactivateMessage{ Context });
						break;
					}

					case RouteId_Update:
					{
						Messages.UpdateMessages.Enqueue(FControlBusUpdateMessage { Context });
						break;
					}

					default:
					{
						return OnEventFailure(RouteId, Style, Context);
					}
				}

				return OnEventSuccess(RouteId, Style, Context);
			}

		private:
			enum : uint16
			{
				RouteId_Activate,
				RouteId_Deactivate,
				RouteId_Update
			};
		};

		return new FControlBusTraceAnalyzer(AsShared());
	}
} // namespace AudioModulationEditor
