// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWISettings.h"
#include "HAL/IConsoleManager.h"


namespace UE::Mass::LWI::Private
{
	bool bMassLWIForceEnable = false;

	FAutoConsoleVariableRef CVarMassLWIEnable(TEXT("mass.lwi.ForceEnable"), bMassLWIForceEnable, TEXT("Use this to temporarily enable MassLWI without affecting the project wide UMassLWISettings::bMassLWIEnabled setting"));
} // UE::MassLOD::Debug

bool UMassLWISettings::IsMassLWIEnabled() const
{
	return bMassLWIEnabled || UE::Mass::LWI::Private::bMassLWIForceEnable;
}
