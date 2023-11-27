// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFixturePatchMatrixCell.generated.h"

struct FDMXCell;
struct FDMXFixtureCellAttribute;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFixturePatchCellAttributeFader;
class UDMXControlConsoleMatrixCellController;
class UDMXEntityFixturePatch;


/** A fader matching a Fixture Patch Matrix Cell in the DMX Control Console. */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleFixturePatchMatrixCell
	: public UObject
	, public IDMXControlConsoleFaderGroupElement
{
	GENERATED_BODY()

public:
	//~ Being IDMXControlConsoleFaderGroupElementInterface
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const override;
	virtual UDMXControlConsoleElementController* GetElementController() override;
	virtual int32 GetIndex() const override;
	virtual const TArray<UDMXControlConsoleFaderBase*>& GetFaders() const override { return CellAttributeFaders; }
	virtual int32 GetUniverseID() const override;
	virtual int32 GetStartingAddress() const override;
	virtual int32 GetEndingAddress() const override;
#if WITH_EDITOR
	virtual void SetIsMatchingFilter(bool bMatches) override;
#endif // WITH_EDITOR
	virtual void Destroy() override;
	//~ End IDMXControlConsoleFaderGroupElementInterface

	/** Adds a Matrix Cell Fader to this Matrix Cell */
	UDMXControlConsoleFixturePatchCellAttributeFader* AddFixturePatchCellAttributeFader(const FDMXFixtureCellAttribute& CellAttribute, const int32 InUniverseID, const int32 StartingChannel);

	/** Deletes a Matrix Cell Fader from this Matrix Cell */
	void DeleteCellAttributeFader(UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader);

	/** Sets Fader's properties values from the given AttributeToChannel map */
	void SetPropertiesFromCell(const FDMXCell& Cell, const int32 InUniverseID, const int32 InStartingChannel);

	/** Creates a Matrix Controller for the given Element */
	UDMXControlConsoleMatrixCellController* CreateMatrixCellController(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement, const FString& ControllerName = "");

	/** Creates a Matrix Controller for the given array of Elements */
	UDMXControlConsoleMatrixCellController* CreateMatrixCellController(const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements, const FString& ControllerName = "");

	/** Deletes the given Matrix Controller */
	void DeleteMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController);

	/** Gets the array of Matrix Controllers for this Matrix Cell */
	TArray<UDMXControlConsoleMatrixCellController*> GetMatrixCellControllers() const { return MatrixCellControllers; }

	/** Gets the Matrix Controller for the given Element, if valid */
	UDMXControlConsoleMatrixCellController* GetControllerByElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element) const;

	/** Sorts the array of Elements by their starting address */
	void SortElementsByStartingAddress() const;

	/** Gets ID index of this Matrix Cell */
	int32 GetCellID() const { return CellID; }

	/** Gets X coordinate of this Matrix Cell */
	int32 GetCellX() const { return CellX; }

	/** Gets Y coordinate of this Matrix Cell */
	int32 GetCellY() const { return CellY; }

#if WITH_EDITOR
	/** True if there's at least one CellAttributeFader visible in Editor */
	bool HasVisibleInEditorCellAttributeFaders() const;

	/** Sets in Editor visibility state of all elements to true */
	void ShowAllFadersInEditor();
#endif // WITH_EDITOR

	// Property Name getters
	FORCEINLINE static FName GetCellXPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchMatrixCell, CellX); }
	FORCEINLINE static FName GetCellYPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchMatrixCell, CellY); }
	FORCEINLINE static FName GetMatrixCellControllersPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchMatrixCell, MatrixCellControllers); }
	FORCEINLINE static FName GetCellAttributeFadersPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchMatrixCell, CellAttributeFaders); }

protected:
	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

private:
	/** Called when the Fixture Patch is changed inside its DMX Library */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* InFixturePatch);

	/** Updates CellAttributeFaders properties according to the given FixturePatch */
	void UpdateFixturePatchCellAttributeFaders(UDMXEntityFixturePatch* InFixturePatch);

	/** Updates the Matrix Cell Controllers array to ensure that each Element has its own Controller */
	void UpdateMatrixCellControllers();

	/** Cell Index ID */
	UPROPERTY(VisibleAnywhere, Category = "DMX Matrix Cell")
	int32 CellID = 0;

	/** Cell X coordinate */
	UPROPERTY(VisibleAnywhere, Category = "DMX Matrix Cell")
	int32 CellX = 0;

	/** Cell Y coordinate */
	UPROPERTY(VisibleAnywhere, Category = "DMX Matrix Cell")
	int32 CellY = 0;

	/** The array of Controllers for the Elements in this Matrix Cell */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleMatrixCellController>> MatrixCellControllers;

	/** Faders array of this Matrix Cell */
	UPROPERTY(VisibleAnywhere, Category = "DMX Matrix Cell")
	TArray<TObjectPtr<UDMXControlConsoleFaderBase>> CellAttributeFaders;
};
