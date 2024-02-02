// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovementModes/AdvancedWalkingMode.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "Kinematic/Settings/CommonLegacyMovementSettings.h"
#include "MoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AdvancedWalkingMode)

UAdvancedWalkingMode::UAdvancedWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default turning rate: turn slower at higher speeds
	MaxTurningRateBySpeedPct.GetRichCurve()->Reset();
	MaxTurningRateBySpeedPct.GetRichCurve()->AddKey(0.f, 500.f);
	MaxTurningRateBySpeedPct.GetRichCurve()->AddKey(1.f, 100.f);
	MaxTurningRateBySpeedPct.GetRichCurve()->SetDefaultValue(500.f);
}


void UAdvancedWalkingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	UMoverComponent* MoverComp = GetMoverComponent();

	// TODO: this was an experiment in dynamically modifying the turning rate. It should be removed once we support a modular turning mechanism.
	FMoverTickStartData StartStateCopy = StartState;
	const FMoverDefaultSyncState* StartingSyncState = StartStateCopy.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	// Map current speed to turning rate
	if (UCommonLegacyMovementSettings* LegacySettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		const float PctToMaxSpeed = FMath::Clamp(StartingSyncState->GetVelocity_WorldSpace().Length() / LegacySettings->MaxSpeed, 0.f, 1.f);
		const float TurningRate = MaxTurningRateBySpeedPct.GetRichCurveConst()->Eval(PctToMaxSpeed);
		LegacySettings->TurningRate = TurningRate;
	}

	Super::OnGenerateMove(StartStateCopy, TimeStep, OutProposedMove);
}
