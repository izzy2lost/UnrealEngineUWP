// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class FAvaPlaylistEditor;
class FExtender;
class FExtensibilityManager;
class FUICommandList;
class UAvalanchePlaylist;
class UObject;

struct FAvaMRQPlaylistContext
{
    TArray<TWeakPtr<const FAvaPlaylistEditor>, TInlineAllocator<1>> PlaylistEditors;
};

class FAvaMRQEditorModule : public IModuleInterface
{
    //~ Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    //~ End IModuleInterface

    static TSharedRef<FExtender> ExtendPlaylistToolbar(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> InObjects);

    static TSharedRef<FUICommandList> CreatePlaylistActions(TSharedRef<FAvaMRQPlaylistContext> InContext);

    TWeakPtr<FExtensibilityManager> PlaylistToolbarExtensibilityWeak;

    FDelegateHandle PlaylistToolbarExtenderHandle;
};
