// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertReplicationScriptingEditorModule.h"

#include "Modules/ModuleManager.h"

namespace UE::ConcertReplicationScriptingEditor
{
	void FConcertReplicationScriptingEditorModule::StartupModule()
	{}

	void FConcertReplicationScriptingEditorModule::ShutdownModule()
	{}
}

IMPLEMENT_MODULE(UE::ConcertReplicationScriptingEditor::FConcertReplicationScriptingEditorModule, ConcertReplicationScriptingEditor);