// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Http/AvaMediaHttpServer.h"
#include "IAvaMediaModule.h"
#include "IAvalancheBroadcastSettings.h"
#include "ModularFeature/AvaMediaSync.h"
#include "ModuleDescriptor.h"
#include "OutputDevices/AvaDeviceProviderProxy.h"
#include "OutputDevices/AvaDisplayDeviceProvider.h"
#include "Playback/AvaMediaPlaybackClient.h"
#include "Playback/AvaMediaPlaybackClientDelegates.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvaMediaPlaybackServer.h"
#include "Playback/AvaMediaPlaybackServerProcess.h"
#include "Playlist/AvalancheManagedInstanceCache.h"

class FAvaMediaModule : public IAvaMediaModule
{
public:
	FAvaMediaModule();

	//~ Begin IAvaMediaModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool IsMediaPlaybackClientStarted() const override { return AvaMediaPlaybackClient.IsValid();}
	virtual void StartMediaPlaybackClient() override;
	virtual void StopMediaPlaybackClient() override;
	virtual bool IsMediaPlaybackServerStarted() const override { return AvaMediaPlaybackServer.IsValid();}
	virtual void StartMediaPlaybackServer(const FString& InPlaybackServerName) override;
	virtual void StopMediaPlaybackServer() override;
	
	virtual IAvaMediaPlaybackClient& GetMediaPlaybackClient() override;
	virtual TSharedPtr<FAvaMediaPlaybackServer> GetMediaPlaybackServer() const override { return AvaMediaPlaybackServer; }
	virtual const IMediaIOCoreDeviceProvider* GetDeviceProvider(FName InProviderName, const FMediaIOOutputConfiguration* InMediaIOOutputConfiguration) const override;
	virtual TArray<const IMediaIOCoreDeviceProvider*> GetDeviceProvidersForServer(const FString& InServerName) const override;
	virtual FString GetServerNameForDevice(const FName& InDeviceProviderName, const FName& InDeviceName) const override;
	virtual bool IsLocalDevice(const FName& InDeviceProviderName, const FName& InDeviceName) const override;
	virtual void LaunchGameModeLocalPlaybackServer() override;
	virtual void StopGameModeLocalPlaybackServer() override;
	virtual bool IsGameModeLocalPlaybackServerLaunched() const override;
	virtual const IAvalancheBroadcastSettings& GetBroadcastSettings() const override;
	virtual const FAvaInstanceSettings& GetAvaInstanceSettings() const override;
	virtual FAvaMediaPlaybackManager& GetLocalPlaybackManager() const override;
	virtual FAvalancheManagedInstanceCache& GetManagedInstanceCache() const override;
	virtual bool IsAvaMediaSyncProviderFeatureAvailable() const override;
	virtual IAvaMediaSyncProvider* GetAvaMediaSyncProvider() const override;
	virtual void NotifyMapChangedEvent(UWorld* InWorld, EAvaMediaMapChangeType InEventType) override;
	virtual FOnAvaMediaSyncProviderChanged& GetOnAvaMediaSyncProviderChanged() override { return OnAvaMediaSyncProviderChanged; }
	virtual FOnAvaMediaSyncPackageModified& GetOnAvaMediaSyncPackageModified() override { return OnAvaMediaSyncPackageModified; }
	virtual FOnMapChangedEvent& GetOnMapChangedEvent() override { return OnMapChangedEvent; }
	virtual FOnAvaMediaPlaybackClientStarted& GetOnAvaMediaPlaybackClientStarted() override { return OnAvaMediaPlaybackClientStarted; }
	virtual FOnAvaMediaPlaybackClientStopped& GetAvaMediaPlaybackClientStopped() override { return OnAvaMediaPlaybackClientStopped; }
	virtual FOnAvaMediaPlaybackServerStarted& GetOnAvaMediaPlaybackServerStarted() override { return OnAvaMediaPlaybackServerStarted; }
	virtual FOnAvaMediaPlaybackServerStopped& GetAvaMediaPlaybackServerStopped() override { return OnAvaMediaPlaybackServerStopped; }
	virtual FGetEditorViewportClient& GetEditorViewportClientDelegate() override { return GetEditorViewportClient; }
	virtual IAvaDeviceProviderProxyManager& GetDeviceProviderProxyManager() override;
	//~ End IAvaMediaModule
	
private:
	void PostEngineInit();
	void EnginePreExit();
	void StopAllServices();
	
	// Command handlers
	void StartPlaybackServerCommand(const TArray<FString>& InArgs);
	void StopPlaybackServerCommand(const TArray<FString>& InArgs);
	void StartPlaybackClientCommand(const TArray<FString>& InArgs);
	void StopPlaybackClientCommand(const TArray<FString>& InArgs);
	void LaunchLocalPlaybackServerCommand(const TArray<FString>& InArgs) { LaunchGameModeLocalPlaybackServer(); }
	void StopLocalPlaybackServerCommand(const TArray<FString>& InArgs) { StopGameModeLocalPlaybackServer(); }

	void StartHttpPlaybackServerCommand(const TArray<FString>& InArgs);
	void StopHttpPlaybackServerCommand(const TArray<FString>& InArgs);
	
	void SaveDeviceProvidersCommand(const TArray<FString>& InArgs);
	void LoadDeviceProvidersCommand(const TArray<FString>& InArgs);
	void UnloadDeviceProvidersCommand(const TArray<FString>& InArgs);
	void ListDeviceProvidersCommand(const TArray<FString>& InArgs);
	void HandleStatCommand(const TArray<FString>& InArgs);

	// Event Handlers
	void OnAvaMediaPlaybackClientConnectionEvent(IAvaMediaPlaybackClient& InPlaybackClient,
		const UE::AvaMediaPlaybackClient::Delegates::FConnectionEventArgs& InArgs);

private:
	FAvaDisplayDeviceProvider AvaDisplayDeviceProvider;
	FAvaDeviceProviderProxyManager DeviceProviderProxyManager;
	TUniquePtr<FAvaMediaSync> AvaMediaSync;

	TArray<IConsoleObject*> ConsoleCmds;
	
	/**
	 *	Wraps the local default UAvalancheMediaSettings.
	 */
	class FLocalBroadcastSettings : public IAvalancheBroadcastSettings
	{
	public:
		virtual ~FLocalBroadcastSettings() override = default;

		//~ Begin IAvalancheBroadcastSettings
		virtual const FLinearColor& GetChannelClearColor() const override;
		virtual EPixelFormat GetDefaultPixelFormat() const override;
		virtual const FIntPoint& GetDefaultResolution() const override;
		virtual bool IsDrawPlaceholderWidget() const override;
		virtual const FSoftObjectPath& GetPlaceholderWidgetClass() const override;
		//~ End IAvalancheBroadcastSettings
	};
	FLocalBroadcastSettings LocalBroadcastSettings;

	/**
	 * The purpose of the settings bridge is to provide a valid object
	 * even if a client disconnects, it will either switch to another client
	 * or use the local settings.
	 */
	class FBroadcastSettingsBridge final : public IAvalancheBroadcastSettings
	{
	public:
		explicit FBroadcastSettingsBridge(FAvaMediaModule* Module) : ParentModule(Module) {}
		virtual ~FBroadcastSettingsBridge() override = default;

		//~ Begin IAvalancheBroadcastSettings
		virtual const FLinearColor& GetChannelClearColor() const override;
		virtual EPixelFormat GetDefaultPixelFormat() const override;
		virtual const FIntPoint& GetDefaultResolution() const override;
		virtual bool IsDrawPlaceholderWidget() const override;
		virtual const FSoftObjectPath& GetPlaceholderWidgetClass() const override;
		//~ End IAvalancheBroadcastSettings

	private:
		const IAvalancheBroadcastSettings& GetSettings() const;
		
		FAvaMediaModule* ParentModule = nullptr;
	};
	FBroadcastSettingsBridge BroadcastSettingsBridge;
	
	TSharedPtr<FAvaMediaPlaybackServer> AvaMediaPlaybackServer;	
	TSharedPtr<FAvaMediaPlaybackClient> AvaMediaPlaybackClient;
	TSharedPtr<FAvaMediaPlaybackServerProcess> LocalPlaybackServerProcess;
	TSharedPtr<FAvaMediaPlaybackManager> LocalPlaybackManager;
	TSharedPtr<FAvalancheManagedInstanceCache> ManagedInstanceCache;

	TSharedPtr<FAvaMediaHttpServer> AvaMediaHttpPlaybackServer;

	FOnAvaMediaSyncProviderChanged OnAvaMediaSyncProviderChanged;
	FOnAvaMediaSyncPackageModified OnAvaMediaSyncPackageModified;
	FOnMapChangedEvent OnMapChangedEvent;
	FOnAvaMediaPlaybackClientStarted OnAvaMediaPlaybackClientStarted;
	FOnAvaMediaPlaybackClientStopped OnAvaMediaPlaybackClientStopped;
	FOnAvaMediaPlaybackServerStarted OnAvaMediaPlaybackServerStarted;
	FOnAvaMediaPlaybackServerStopped OnAvaMediaPlaybackServerStopped;
	FGetEditorViewportClient GetEditorViewportClient;
};
