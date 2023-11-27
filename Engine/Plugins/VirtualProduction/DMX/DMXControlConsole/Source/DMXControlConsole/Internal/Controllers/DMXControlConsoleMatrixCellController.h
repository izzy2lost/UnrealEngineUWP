// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleElementController.h"

#include "DMXControlConsoleMatrixCellController.generated.h"

class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFixturePatchMatrixCell;


/** A controller for handling inputs for more than one fader at once in a matrix cell */
UCLASS(AutoExpandCategories = ("DMX Element Controller", "DMX Element Controller|Oscillator"))
class DMXCONTROLCONSOLE_API UDMXControlConsoleMatrixCellController
	: public UDMXControlConsoleElementController
{
	GENERATED_BODY()

public:
	//~ Being UDMXControlConsoleElementController interface
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const override;
	virtual int32 GetIndex() const override;
	virtual void Destroy() override;
	//~ End UDMXControlConsoleElementController interface

	/** Returns the Matrix Cell this Controller resides in */
	UDMXControlConsoleFixturePatchMatrixCell& GetOwnerMatrixCellChecked() const;
};
