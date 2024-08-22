// Copyright Epic Games, Inc. All Rights Reserved.

#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>
#include <Modules/ModuleManager.h>
#include <Widgets/Docking/SDockTab.h>

#include "StylusInputDebugWidget.h"

#define LOCTEXT_NAMESPACE "StylusInputDebugWidgetModule"

namespace UE::StylusInput::DebugWidget
{
	class FStylusInputDebugWidgetModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		void RegisterTabSpawners(const TSharedPtr<FWorkspaceItem>& WorkspaceGroup);
		void UnregisterTabSpawners();

	private:
		TSharedRef<SDockTab> MakeDebugWidgetTab(const FSpawnTabArgs&);
		TSharedRef<SWidget> GetDebugWidget();

		bool bHasRegisteredTabSpawners = false;
		TWeakPtr<SStylusInputDebugWidget> DebugWidgetPtr;
	};

	void FStylusInputDebugWidgetModule::StartupModule()
	{
		RegisterTabSpawners(nullptr);
	}

	void FStylusInputDebugWidgetModule::ShutdownModule()
	{
		UnregisterTabSpawners();
	}

	void FStylusInputDebugWidgetModule::RegisterTabSpawners(const TSharedPtr<FWorkspaceItem>& WorkspaceGroup)
	{
		if (bHasRegisteredTabSpawners)
		{
			UnregisterTabSpawners();
		}

		FTabSpawnerEntry& DebugWidgetSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner("StylusInputDebugWidget",
			FOnSpawnTab::CreateRaw(this, &FStylusInputDebugWidgetModule::MakeDebugWidgetTab))
				.SetDisplayName(LOCTEXT("DebugWidgetTitle", "Stylus Input Debug"))
				.SetTooltipText(LOCTEXT("DebugWidgetTooltip", "Open a debug widget to verify stylus input event handling."))
				.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
				.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "StylusInputDebug.TabIcon"));

		if (WorkspaceGroup.IsValid())
		{
			DebugWidgetSpawnerEntry.SetGroup(WorkspaceGroup.ToSharedRef());
		}

		bHasRegisteredTabSpawners = true;
	}

	void FStylusInputDebugWidgetModule::UnregisterTabSpawners()
	{
		bHasRegisteredTabSpawners = false;

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("StylusInputDebugWidget");
	}

	TSharedRef<SDockTab> FStylusInputDebugWidgetModule::MakeDebugWidgetTab(const FSpawnTabArgs&)
	{
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(NomadTab);
		Tab->SetContent(GetDebugWidget());
		return Tab;
	}

	TSharedRef<SWidget> FStylusInputDebugWidgetModule::GetDebugWidget()
	{
		TSharedPtr<SStylusInputDebugWidget> DebugWidget = DebugWidgetPtr.Pin();

		if (!DebugWidget.IsValid())
		{
			DebugWidget = SNew(SStylusInputDebugWidget);
			DebugWidgetPtr = DebugWidget;
		}

		return DebugWidget.ToSharedRef();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::StylusInput::DebugWidget::FStylusInputDebugWidgetModule, StylusInputDebugWidget);
