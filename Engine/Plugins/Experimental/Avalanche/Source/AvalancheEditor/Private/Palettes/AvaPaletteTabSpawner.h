// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPaletteExtension.h"
#include "AvaTabSpawner.h"

class FAvaPaletteTabSpawner : public FAvaTabSpawner
{
public:
	explicit FAvaPaletteTabSpawner(const TSharedRef<IAvaEditor>& InEditor, const FAvaPaletteTabInfo& InPaletteTabInfo);

	FName GetTabID();

	//~ Begin IAvaTabSpawner
	virtual TSharedRef<SWidget> CreateTabBody() override;
	virtual FTabSpawnerEntry& RegisterTabSpawner(const TSharedRef<FTabManager>& InTabManager, 
		const TSharedPtr<FWorkspaceItem>& InWorkspaceMenu) override;
	//~ End IAvaTabSpawner

protected:
	const FAvaPaletteTabInfo PaletteTabInfo;
};
