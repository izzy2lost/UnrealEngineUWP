// Copyright Epic Games, Inc. All Rights Reserved.

#include "CVarMultiUserReplicationEditor.h"

namespace UE::MultiUserReplicationEditor::ConsoleVariables
{
	TAutoConsoleVariable<bool> CVarEnableReplication(
		TEXT("Concert.EnableReplication"),
		false,
		TEXT("Controls whether the replication feature is enabled."),
		ECVF_Default
		);
}