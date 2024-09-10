// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSubsystems.generated.h"

class IEditorDataStorageProvider;
class IEditorDataStorageUiProvider;
class IEditorDataStorageCompatibilityProvider;

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

	IEditorDataStorageProvider* Get();
	const IEditorDataStorageProvider* Get() const;

protected:
	mutable IEditorDataStorageProvider* DataStorage{ nullptr };
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

	IEditorDataStorageUiProvider* Get();
	const IEditorDataStorageUiProvider* Get() const;

protected:
	mutable IEditorDataStorageUiProvider* DataStorageUi{ nullptr };
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

	IEditorDataStorageCompatibilityProvider* Get();
	const IEditorDataStorageCompatibilityProvider* Get() const;

protected:
	mutable IEditorDataStorageCompatibilityProvider* DataStorageCompatibility{ nullptr };
};