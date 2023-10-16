// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkHubModule.h"

class FLiveLinkHubModule : public ILiveLinkHubModule
{
public:
	//~ Begin ILiveLinkHubModule interface
	virtual void StartLiveLinkHub() override;
	//~ End ILiveLinkHubModule interface
};
