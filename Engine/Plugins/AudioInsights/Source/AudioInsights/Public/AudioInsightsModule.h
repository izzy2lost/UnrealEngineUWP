// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "AudioInsightsDashboardFactory.h"
#include "AudioInsightsTraceModule.h"
#include "Framework/Docking/TabManager.h"
#include "IAudioInsightsModule.h"
#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"
#include "Widgets/Docking/SDockTab.h"


namespace UE::Audio::Insights
{
	class FAudioInsightsModule final : public IAudioInsightsModule
	{
	public:
		FAudioInsightsModule() = default;

		virtual ~FAudioInsightsModule() = default;

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		virtual void RegisterDashboardViewFactory(TSharedRef<IDashboardViewFactory> InDashboardFactory) override;
		virtual void UnregisterDashboardViewFactory(FName InName) override;
		virtual ::Audio::FDeviceId GetDeviceId() const override;

		AUDIOINSIGHTS_API static FAudioInsightsModule& GetChecked();
		AUDIOINSIGHTS_API FTraceModule& GetTraceModule();

		TSharedRef<FDashboardFactory> GetDashboardFactory();
		const TSharedRef<FDashboardFactory> GetDashboardFactory() const;

		TSharedRef<SDockTab> CreateDashboardTabWidget(const FSpawnTabArgs& Args);

	private:
		TSharedPtr<FDashboardFactory> DashboardFactory;
		FTraceModule TraceModule;
	};
} // namespace UE::Audio::Insights
