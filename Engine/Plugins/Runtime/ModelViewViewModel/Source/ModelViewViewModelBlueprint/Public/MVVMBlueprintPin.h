// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMPropertyPath.h"

#include "MVVMBlueprintPin.generated.h"

class UWidgetBlueprint;
class UEdGraphNode;
class UEdGraphPin;

/**
*
*/
UENUM()
enum class EMVVMBlueprintPinStatus : uint8
{
	Valid,
	Orphaned,
};

/**
*
*/
USTRUCT()
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPin
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FGuid PinId;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FName PinName;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FMVVMBlueprintPropertyPath Path;

	/** Default value for this pin (used if the pin has no connections), stored as a string */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FString DefaultString;

	/** If the default value for this pin should be an FText, it is stored here. */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FText DefaultText;

	/** If the default value for this pin should be an object, we store a pointer to it */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	TObjectPtr<class UObject> DefaultObject;

	/** The pin is split. */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	bool bSplit = false;

	/** The pin could not be set. */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	mutable EMVVMBlueprintPinStatus Status = EMVVMBlueprintPinStatus::Valid;

public:
	FMVVMBlueprintPin() = default;
	FMVVMBlueprintPin(FName PinName);

	FName GetName() const
	{
		return PinName;
	}

	/** The pin is split into its different components. */
	bool IsSplit() const
	{
		return bSplit;
	}

	/** The pin could not be assigned to the graph pin. */
	EMVVMBlueprintPinStatus GetStatus() const
	{
		return Status;
	}

	/** Are we using the path. */
	bool UsedPathAsValue() const
	{
		return !bSplit && Path.IsValid();
	}

	const FMVVMBlueprintPropertyPath& GetPath() const
	{
		return Path;
	}

	FString GetValueAsString(const UClass* SelfContext) const;

	void SetDefaultValue(UObject* Value);
	void SetDefaultValue(const FText& Value);
	void SetDefaultValue(const FString& Value);
	void SetPath(const FMVVMBlueprintPropertyPath& Value);

	static bool IsInputPin(const UEdGraphPin* Pin);
	static TArray<FMVVMBlueprintPin> CopyAndReturnMissingPins(UBlueprint* Blueprint, UEdGraphNode* GraphNode, const TArray<FMVVMBlueprintPin>& Pins);
	static TArray<FMVVMBlueprintPin> CreateFromNode(UBlueprint* Blueprint, UEdGraphNode* GraphNode);
	static FMVVMBlueprintPin CreateFromPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin);

	void CopyTo(const UBlueprint* WidgetBlueprint, UEdGraphNode* Node) const;
	UEdGraphPin* FindGraphPin(UEdGraphNode* Node) const;

private:
	void Reset();
};
