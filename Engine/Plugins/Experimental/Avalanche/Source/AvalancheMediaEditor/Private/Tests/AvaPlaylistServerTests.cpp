// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheBroadcast.h"
#include "Broadcast/OutputDevices/AvaOutputRootItem.h"
#include "Broadcast/OutputDevices/AvaOutputTreeItem.h"
#include "IAvaMediaEditorModule.h"
#include "MediaOutput.h"
#include "MessageEndpointBuilder.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Playlist/AvaPlaylistServer.h"
#include "Playlist/MediaOutputEditorUtils/MediaOutputEditorUtils.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaPlaylistServerTests, Log, All);

namespace UE::AvaPlaylistServerTests
{
	class FPlaylistServerTestBase : public FAutomationTestBase
	{
	public:
		FPlaylistServerTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
		{
			// UE-185435 - Blackmagic lib generates an error if the driver is not installed.
			LocalSuppressedLogCategories.Add(TEXT("LogBlackmagicCore"));
		}

		//~ Begin FAutomationTestBase
		virtual bool ShouldCaptureLogCategory(const FName& Category) const override
		{
			if (LocalSuppressedLogCategories.Contains(Category))
			{
				return false;
			}
			return FAutomationTestBase::ShouldCaptureLogCategory(Category);
		}
		//~ End FAutomationTestBase

	protected:
		TSet<FName> LocalSuppressedLogCategories;
	};

	class FTestClient
	{
	public:
		TSharedPtr<FMessageEndpoint> MessageEndpoint;
		FAutomationTestBase* Test = nullptr;
		TSet<int32> ReceivedRequestIds;

		bool HasReceivedResponse(int32 InRequestId) const
		{
			return ReceivedRequestIds.Contains(InRequestId);
		}
		
		void Init(FAutomationTestBase* InTest)
		{
			Test = InTest;
			
			MessageEndpoint = FMessageEndpoint::Builder("AvaMediaPlaylistTestClient")
				.Handling<FAvaPlaylistChannelImage>(this, &FTestClient::HandlePlaylistChannelImage)
				.Handling<FAvaPlaylistServerMsg>(this, &FTestClient::HandleServerMessage);
		}
		
		void HandlePlaylistChannelImage(const FAvaPlaylistChannelImage& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
		{
			ReceivedRequestIds.Add(InMessage.RequestId);
			
			UE_LOG(LogAvaPlaylistServerTests, Log, TEXT("Received Playlist Channel Image successfully."));

			const FString SaveName = FString::Printf(TEXT("%s%s"), *FPaths::ProjectSavedDir(), TEXT("ChannelImageTest.jpeg"));
			UE_LOG(LogAvaPlaylistServerTests, Display, TEXT("Saving Image to %s"), *SaveName);

			FFileHelper::SaveArrayToFile(InMessage.ImageData, *SaveName);
		}

		void HandleServerMessage(const FAvaPlaylistServerMsg& InMessage, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
		{
			ReceivedRequestIds.Add(InMessage.RequestId);

			const ELogVerbosity::Type Verbosity = ParseLogVerbosityFromString(InMessage.Verbosity);
				
			if (Verbosity <= ELogVerbosity::Error && Test)
			{
				Test->AddError(InMessage.Text, 1);
			}
		}
	};

	TSharedPtr<FAvaPlaylistServer> GetOrCreatePlaylistServer()
	{
		TSharedPtr<FAvaPlaylistServer> PlaylistServer = IAvaMediaEditorModule::Get().GetPlaylistServer();
		if (!PlaylistServer)
		{
			UE_LOG(LogAvaPlaylistServerTests, Log, TEXT("Playlist Server not started. Starting one temporarily for the test."));

			// Start a temporary server. It will be deleted when the last latent command is finished.
			PlaylistServer = MakeShared<FAvaPlaylistServer>();
			PlaylistServer->Init(TEXT(""));
		}
		return PlaylistServer;
	}

	void BackupBroadcastConfig()
	{
		const UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
		const FString SaveFilepath = Broadcast.GetBroadcastSaveFilepath();
		const FString BackupFilepath = SaveFilepath + TEXT(".tests.backup");
		IFileManager::Get().Copy(*BackupFilepath, *SaveFilepath);
		UE_LOG(LogAvaPlaylistServerTests, Log, TEXT("Broadcast config backed up."));
	}

	void RestoreBroadcastConfig()
	{
		UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
		const FString SaveFilepath = Broadcast.GetBroadcastSaveFilepath();
		const FString BackupFilepath = SaveFilepath + TEXT(".tests.backup");
		if (!IFileManager::Get().FileExists(*BackupFilepath))
		{
			UE_LOG(LogAvaPlaylistServerTests, Error, TEXT("Failed to restore broadcast config: backup file \"%s\" doesn't exist."), *BackupFilepath);
			return;
		}

		IFileManager::Get().Copy(*SaveFilepath, *BackupFilepath);
		IFileManager::Get().Delete(*BackupFilepath);

		// Reload broadcast from backup file.
		Broadcast.StopBroadcast();
		Broadcast.LoadBroadcast();

		UE_LOG(LogAvaPlaylistServerTests, Log, TEXT("Broadcast config restored."));
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FPlaylistServerWaitForResponse, int32, RequestId, TSharedPtr<UE::AvaPlaylistServerTests::FTestClient>, TestClient, TSharedPtr<FAvaPlaylistServer>, PlaylistServer);
bool FPlaylistServerWaitForResponse::Update()
{
	if (!TestClient.IsValid())
	{
		UE_LOG(LogAvaPlaylistServerTests, Error, TEXT("Test Client is not valid."));
		return true;
	}
	
	return TestClient->HasReceivedResponse(RequestId);
}

DEFINE_LATENT_AUTOMATION_COMMAND(FPlaylistServerRestoreBroadcastConfig);
bool FPlaylistServerRestoreBroadcastConfig::Update()
{
	using namespace UE::AvaPlaylistServerTests;
	RestoreBroadcastConfig();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaylistServerGetChannelImage, "Avalanche.PlaylistServer.GetChannelImage", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
bool FPlaylistServerGetChannelImage::RunTest(const FString& Parameters)
{
	using namespace UE::AvaPlaylistServerTests;
	
	const TSharedPtr<FAvaPlaylistServer> PlaylistServer = GetOrCreatePlaylistServer();
	
	// Build a test client for this request.
	const TSharedPtr<FTestClient> TestClient = MakeShared<FTestClient>();
	TestClient->Init(this);

	// Send the request message.
	constexpr int32 RequestId = 1000;
	FAvaPlaylistGetChannelImage* Request = FMessageEndpoint::MakeMessage<FAvaPlaylistGetChannelImage>();
	Request->RequestId = RequestId;
	Request->ChannelName = UAvalancheBroadcast::Get().GetChannelName(0).ToString();
	
	TestClient->MessageEndpoint->Send(Request, PlaylistServer->GetMessageAddress());
	
	ADD_LATENT_AUTOMATION_COMMAND(FPlaylistServerWaitForResponse(RequestId, TestClient, PlaylistServer));
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPlaylistServerAddChannelDevice, UE::AvaPlaylistServerTests::FPlaylistServerTestBase, "Avalanche.PlaylistServer.AddChannelDevice", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
bool FPlaylistServerAddChannelDevice::RunTest(const FString& Parameters)
{
	using namespace UE::AvaPlaylistServerTests;

	// Need to backup the broadcast config because we are about to make changes.
	BackupBroadcastConfig();
	
	const TSharedPtr<FAvaPlaylistServer> PlaylistServer = GetOrCreatePlaylistServer();

	// Build a test client for this request.
	const TSharedPtr<FTestClient> TestClient = MakeShared<FTestClient>();
	TestClient->Init(this);

	// Send the request message.
	FAvaPlaylistAddChannelDevice* Request = FMessageEndpoint::MakeMessage<FAvaPlaylistAddChannelDevice>();
	constexpr int32 RequestId = 1000;
	Request->RequestId = RequestId;
	Request->ChannelName = UAvalancheBroadcast::Get().GetChannelName(0).ToString();
	Request->MediaOutputName = TEXT("InvalidDevice");

	// Find an existing output so the request succeeds.
	const FAvaOutputTreeItemPtr OutputDevices = MakeShared<FAvaOutputRootItem>();
	FAvaOutputTreeItem::RefreshTree(OutputDevices);
	
	if (OutputDevices->GetChildren().Num() > 0)
	{
		FAvaOutputTreeItemPtr ExistingOutput = OutputDevices->GetChildren()[0];
		while (ExistingOutput.IsValid() && ExistingOutput->GetChildren().Num() > 0)
		{
			ExistingOutput = ExistingOutput->GetChildren()[0];
		}
		if (ExistingOutput.IsValid())
		{
			Request->MediaOutputName = ExistingOutput->GetDisplayName().ToString();
			UE_LOG(LogAvaPlaylistServerTests, Log, TEXT("Selected media output for test: \"%s\"."), *Request->MediaOutputName);
		}
	}
	
	TestClient->MessageEndpoint->Send(Request, PlaylistServer->GetMessageAddress());
	
	ADD_LATENT_AUTOMATION_COMMAND(FPlaylistServerWaitForResponse(RequestId, TestClient, PlaylistServer));
	ADD_LATENT_AUTOMATION_COMMAND(FPlaylistServerRestoreBroadcastConfig());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaylistServerMediaOutputSerialization, "Avalanche.PlaylistServer.MediaOutputSerialization", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
bool FPlaylistServerMediaOutputSerialization::RunTest(const FString& Parameters)
{

	for (UClass* const Class : TObjectRange<UClass>())
	{
		// For now, we test all the media output classes, technically, this should work with all of them.
		// The result of this test depend on the plugins that are enabled. 
		// The blackmagic plugin should be enabled at least.
		const bool bIsMediaOutputClass = Class->IsChildOf(UMediaOutput::StaticClass()) && Class != UMediaOutput::StaticClass();
		if (bIsMediaOutputClass)
		{
			UMediaOutput* const MediaOutput = NewObject<UMediaOutput>(GetTransientPackage(), Class, NAME_None, RF_Transactional);
			FString MediaOutputJson = FAvaMediaOutputEditorUtils::SerializeMediaOutput(MediaOutput);

			const FString SaveName = FString::Printf(TEXT("%sSerializationTest_%s.json"), *FPaths::ProjectSavedDir(), *Class->GetName());
			UE_LOG(LogAvaPlaylistServerTests, Display, TEXT("Serializing Media Output \"%s\" to \"%s\""), *Class->GetName(), *SaveName);

			TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*SaveName));
			*Archive << MediaOutputJson;
			
			Archive->Close();
		}
	}
	return true;
}