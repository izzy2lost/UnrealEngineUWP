// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DMXControlConsoleMatrixCellController.h"

#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleMatrixCellController"

UDMXControlConsoleFaderGroup& UDMXControlConsoleMatrixCellController::GetOwnerFaderGroupChecked() const
{
	const UDMXControlConsoleFixturePatchMatrixCell& OwnerMatrixCell = GetOwnerMatrixCellChecked();
	return OwnerMatrixCell.GetOwnerFaderGroupChecked();
}

int32 UDMXControlConsoleMatrixCellController::GetIndex() const
{
	const UDMXControlConsoleFixturePatchMatrixCell& OwnerMatrixCell = GetOwnerMatrixCellChecked();

	const TArray<UDMXControlConsoleMatrixCellController*> Controllers = OwnerMatrixCell.GetMatrixCellControllers();
	const int32 Index = Controllers.IndexOfByKey(this);
	return Index;
}

void UDMXControlConsoleMatrixCellController::Destroy()
{
	UDMXControlConsoleFixturePatchMatrixCell& OwnerMatrixCell = GetOwnerMatrixCellChecked();

#if WITH_EDITOR
	OwnerMatrixCell.PreEditChange(UDMXControlConsoleFixturePatchMatrixCell::StaticClass()->FindPropertyByName(UDMXControlConsoleFixturePatchMatrixCell::GetMatrixCellControllersPropertyName()));
#endif // WITH_EDITOR

	OwnerMatrixCell.DeleteMatrixCellController(this);

#if WITH_EDITOR
	OwnerMatrixCell.PostEditChange();
#endif // WITH_EDITOR
}

UDMXControlConsoleFixturePatchMatrixCell& UDMXControlConsoleMatrixCellController::GetOwnerMatrixCellChecked() const
{
	UDMXControlConsoleFixturePatchMatrixCell* OwnerMatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(GetOuter());
	checkf(OwnerMatrixCell, TEXT("Invalid outer for '%s', cannot get controller owner correctly."), *GetName());

	return *OwnerMatrixCell;
}

#undef LOCTEXT_NAMESPACE
