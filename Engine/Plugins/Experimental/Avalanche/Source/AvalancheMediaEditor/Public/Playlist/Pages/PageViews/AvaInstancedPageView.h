// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPageView.h"

enum class ECheckBoxState : uint8;
struct FAvalanchePage;

class IAvaInstancedPageView
{
public:
	UE_AVA_TYPE(IAvaInstancedPageView);

	virtual ~IAvaInstancedPageView() = default;

	virtual ECheckBoxState IsEnabled() const = 0;
	virtual void SetEnabled(ECheckBoxState InState) = 0;

	virtual FName GetChannelName() const = 0;
	virtual bool SetChannel(FName InChannel) = 0;

	virtual const FAvalanchePage& GetTemplate() const = 0;
	virtual FText GetTemplateDescription() const = 0;
};
