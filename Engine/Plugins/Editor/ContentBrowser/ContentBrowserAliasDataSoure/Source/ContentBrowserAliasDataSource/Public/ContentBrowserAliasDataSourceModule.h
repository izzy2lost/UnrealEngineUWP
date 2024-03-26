// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserAliasDataSource.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

class FContentBrowserAliasDataSourceModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void PreUnloadCallback() override;

	CONTENTBROWSERALIASDATASOURCE_API UContentBrowserAliasDataSource* GetAliasDataSource();

private:
	TStrongObjectPtr<UContentBrowserAliasDataSource> AliasDataSource;
};
