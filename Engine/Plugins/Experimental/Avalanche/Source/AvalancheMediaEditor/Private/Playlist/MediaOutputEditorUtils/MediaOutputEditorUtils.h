// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"
#include "Templates/SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMediaOutputEditorUtils, Warning, All);

class FJsonObject;

/*
 *	Class designed to deal with Serialization and Editing
 *	of Avalanche Media Output device data coming from AvalanchePlaylistServer
 *
 *	It is meant to give full control and provide all information to Playlist server
 *	in a form that is usable outside of unreal engine
 */
class FAvaMediaOutputEditorUtils
{
public:
	FAvaMediaOutputEditorUtils();
	virtual ~FAvaMediaOutputEditorUtils();

	static FString SerializeMediaOutput(const UMediaOutput* InMediaOutput);
	static void EditMediaOutput(UMediaOutput* InMediaOutput, const FString& InDeviceData);

private:
	//Serialization helper functions
	static TSharedPtr<FJsonObject> ParsePropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseElementaryPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseEnumPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);
	static TSharedPtr<FJsonObject> ParseStructPropertyInfo(TFieldIterator<FProperty> InProperty, const void* InOwnerObject);

	//Helper functions for editing Avalanche Media Output Device
	static void SetProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, const TFieldIterator<FProperty>& InProperty);
	static void SetElementaryProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
	static void SetEnumProperty(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
	static void SetStructProperties(void* InOwnerObject, const TSharedPtr<FJsonObject>& InPropertyObject, TFieldIterator<FProperty> InProperty);
};
