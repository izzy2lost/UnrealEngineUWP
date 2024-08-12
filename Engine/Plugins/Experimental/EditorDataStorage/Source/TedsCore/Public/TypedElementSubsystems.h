// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSubsystems.generated.h"

class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageCompatibilityInterface;

/**
 * A subsystem to provide alternative access to the Editor Data Storage. This should be used in most situations instead of 
 * directly Data Storage from the compatibility location in Typed Elements Registry.
 */
UCLASS()
class TEDSCORE_API UEditorDataStorageSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UEditorDataStorageSubsystem() override;

	ITypedElementDataStorageInterface* Get();
	const ITypedElementDataStorageInterface* Get() const;

protected:
	mutable ITypedElementDataStorageInterface* DataStorage{ nullptr };
};

/**
 * A subsystem to provide alternative access to the Editor Data Storage UI. This should be used in most situations instead of
 * directly Data Storage UI from the compatibility location in Typed Elements Registry.
 */
UCLASS()
class TEDSCORE_API UEditorDataStorageUiSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UEditorDataStorageUiSubsystem() override;

	ITypedElementDataStorageUiInterface* Get();
	const ITypedElementDataStorageUiInterface* Get() const;

protected:
	mutable ITypedElementDataStorageUiInterface* DataStorageUi{ nullptr };
};

/**
 * A subsystem to provide alternative access to the Editor Data Storage Compatibility. This should be used in most situations instead of
 * directly Data Storage Compatibility from the compatibility location in Typed Elements Registry.
 */
UCLASS()
class TEDSCORE_API UEditorDataStorageCompatibilitySubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UEditorDataStorageCompatibilitySubsystem() override;

	ITypedElementDataStorageCompatibilityInterface* Get();
	const ITypedElementDataStorageCompatibilityInterface* Get() const;

protected:
	mutable ITypedElementDataStorageCompatibilityInterface* DataStorageCompatibility{ nullptr };
};