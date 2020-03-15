// Copyright Microsoft Inc. All Rights Reserved.

#include "UWPSaveGameSystem.h"
#include "SlateApplication.h"
#include "ConfigCacheIni.h"

#include "AllowWindowsPlatformTypes.h"
#include <collection.h>
#include <robuffer.h>
#include <ppltasks.h>
#include "HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY(LogUWPSaveGame);

//! GameSaveContainer.SubmitUpdatesAsync() - Only 16MB of data may be written per call. 
enum { kMaxBlobSizeInBytes = (16 * 1024 * 1024) };

using namespace Concurrency;
using namespace Platform;
using namespace Platform::Collections;				// Map
using namespace Windows::Foundation;				// IAsyncOperation
using namespace Windows::Foundation::Collections;	// IMapView
using namespace Windows::Storage::Streams;			// IBuffer, Buffer
using namespace Windows::Gaming::XboxLive::Storage;	// GameSaveProviderGetResult, GameSaveProvider, GameSaveContainer, GameSaveOperationResult, GameSaveContainerInfoGetResult


static Platform::String ^ GetProductConfigId()
{
	FString ScidFromIni;
	if (GConfig->GetString(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), TEXT("ServiceConfigId"), ScidFromIni, GEngineIni))
	{
		return ref new Platform::String(*ScidFromIni);
	}

	return nullptr;
}

static Buffer ^ CreateStorageBuffer(const uint8 *	InBuffer,
	int32			InSize)
{
	check(InSize <= kMaxBlobSizeInBytes);

	Buffer^ OutBuffer = ref new Buffer(InSize);
	OutBuffer->Length = InSize;

	IUnknown* UnknownInterface = reinterpret_cast<IUnknown*>(OutBuffer);

	Microsoft::WRL::ComPtr< IBufferByteAccess > BufferByteAccess;
	HRESULT Result = UnknownInterface->QueryInterface(_uuidof(IBufferByteAccess), &BufferByteAccess);
	if (FAILED(Result))
	{
		return nullptr;
	}

	uint8 * DestBytes = nullptr;
	BufferByteAccess->Buffer(&DestBytes);

	memcpy(DestBytes, InBuffer, InSize);


	return OutBuffer;
}

static Buffer ^ CreateStorageBuffer(const TArray< uint8 > & Src)
{
	return CreateStorageBuffer(Src.GetData(), Src.Num());
}

static bool CopyStorageBuffer(TArray< uint8 > &	Dest,
	IBuffer ^			Src)
{
	check(nullptr != Src);


	unsigned int SrcLength = Src->Length;


	// pre-allocate empty storage for buffer
	Dest.Empty(SrcLength);

	// get source raw data
	IUnknown * UnknownInterface = reinterpret_cast< IUnknown * >(Src);

	Microsoft::WRL::ComPtr< IBufferByteAccess > BufferByteAccess;
	HRESULT Result = UnknownInterface->QueryInterface(_uuidof(IBufferByteAccess), &BufferByteAccess);
	if (FAILED(Result))
	{
		return false;
	}

	BYTE * StorageBufferBytePtr = nullptr;
	BufferByteAccess->Buffer(&StorageBufferBytePtr);

	// copy data
	Dest.AddUninitialized(SrcLength);
	memcpy(Dest.GetData(), StorageBufferBytePtr, SrcLength);


	return true;
}




static task< GameSaveProvider ^ > GetProviderAsync(Platform::String ^ ProductConfigId)
{
	IAsyncOperation< IVectorView< Windows::System::User ^ > ^ > ^ FindAllUsersOp = Windows::System::User::FindAllAsync(Windows::System::UserType::LocalUser);
	auto FindAllUsersTask = create_task(FindAllUsersOp);

	auto GetCurrentUserTask = FindAllUsersTask.then([](IVectorView< Windows::System::User ^ > ^ UserList)
	{
		Windows::System::User ^ CurrentUser = nullptr;

		if ((nullptr != UserList) &&
			(UserList->Size > 0))
		{
			CurrentUser = UserList->GetAt(0);	// UWP only has one logged in User (at time of writing)
		}
		else
		{
			UE_LOG(LogUWPSaveGame, Warning, TEXT("Local user list is NULL or Empty."));
		}

		return CurrentUser;

	});

	return GetCurrentUserTask.then([ProductConfigId](Windows::System::User ^ CurrentUser)
	{
		if (nullptr != CurrentUser)
		{
			UE_LOG(LogUWPSaveGame, Verbose, TEXT("Current user = %p"), (void *)CurrentUser);
			IAsyncOperation< GameSaveProviderGetResult ^ > ^ Op = GameSaveProvider::GetForUserAsync(CurrentUser, ProductConfigId);
			return create_task(Op).then([](GameSaveProviderGetResult ^ Result) -> GameSaveProvider ^
			{
				if (nullptr != Result)
				{
					return Result->Value;
				}

				return nullptr;
			});
		}

		UE_LOG(LogUWPSaveGame, Warning, TEXT("Current user is NULL."));
		return create_task([]() -> GameSaveProvider ^
		{
			return nullptr;
		});
	});
}

static task< GameSaveContainer ^ > GetDefaultContainerAsync(Platform::String ^ ProductConfigId)
{
	auto GetProviderTask = GetProviderAsync(ProductConfigId);

	return GetProviderTask.then([](GameSaveProvider ^ Provider)
	{
		GameSaveContainer ^ FileContainer = nullptr;

		if (nullptr != Provider)
		{
			Platform::String ^ ContainerName = ref new String(L"UE4DefaultSaveGameContainer");
			FileContainer = Provider->CreateContainer(ContainerName);
		}

		return FileContainer;
	});
}



static task< ISaveGameSystem::ESaveExistsResult > ExistsAsyncAux(Platform::String ^		ProductConfigId,
	Platform::String ^		PlatformSlotName)
{
	task< GameSaveContainer ^ > GetContainerTask = GetDefaultContainerAsync(ProductConfigId);

	return GetContainerTask.then([PlatformSlotName](GameSaveContainer ^ Container)
	{
		if (nullptr != Container)
		{
			GameSaveBlobInfoQuery ^ Query = Container->CreateBlobInfoQuery(PlatformSlotName);
			if (nullptr != Query)
			{
				IAsyncOperation< GameSaveBlobInfoGetResult ^ > ^ InfoOp = Query->GetBlobInfoAsync();
				auto InfoTask = create_task(InfoOp);

				return InfoTask.then([PlatformSlotName](GameSaveBlobInfoGetResult ^ Result)
				{
					if ((nullptr != Result) &&
						(GameSaveErrorStatus::Ok == Result->Status))
					{
						IVectorView< GameSaveBlobInfo ^ > ^ InfoList = Result->Value;
						if (nullptr != InfoList)
						{
							// check for exact slot name match (as search uses slot name as prefix)
							for (uint32 i = 0; i < InfoList->Size; i++)
							{
								GameSaveBlobInfo ^	Info = InfoList->GetAt(i);

								if (nullptr != Info)
								{
									if (Platform::String::CompareOrdinal(PlatformSlotName, Info->Name) == 0)
									{
										return ISaveGameSystem::ESaveExistsResult::OK;
									}
								}
							}

						}
					}

					return ISaveGameSystem::ESaveExistsResult::DoesNotExist;
				});
			}
		}

		return create_task([]() -> ISaveGameSystem::ESaveExistsResult
		{
			return ISaveGameSystem::ESaveExistsResult::DoesNotExist;
		});
	});
}

static task< bool > SaveAsyncAux(Platform::String ^		ProductConfigId,
	Platform::String ^		PlatformSlotName,
	const TArray< uint8 > &	SrcData)
{
	Buffer ^ StorageBuffer = CreateStorageBuffer(SrcData);

	task< GameSaveContainer ^ > GetContainerTask = GetDefaultContainerAsync(ProductConfigId);

	return GetContainerTask.then([PlatformSlotName, StorageBuffer](GameSaveContainer ^ Container)
	{
		if (nullptr != Container)
		{
			UE_LOG(LogUWPSaveGame, Log, TEXT("Attempting to save data to connected storage."));


			// submit to xblive connected storage
			Map< Platform::String ^, IBuffer ^ > ^ StorageUpdates = ref new Map< Platform::String ^, IBuffer ^ >();
			StorageUpdates->Insert(PlatformSlotName, StorageBuffer);

			IAsyncOperation< GameSaveOperationResult ^ >^ SubmitOp = Container->SubmitUpdatesAsync(StorageUpdates->GetView(), nullptr, nullptr);
			auto StartSubmitTask = create_task(SubmitOp);

			return StartSubmitTask.then([](GameSaveOperationResult ^ Result)
			{
				if (nullptr != Result)
				{
					return (GameSaveErrorStatus::Ok == Result->Status);
				}

				return false;
			});
		}

		return create_task([]() -> bool
		{
			return false;
		});
	});
}

static task< IBuffer ^ > LoadAsyncAux(Platform::String ^ ProductConfigId,
	Platform::String ^ PlatformSlotName)
{
	task< GameSaveContainer ^ > GetContainerTask = GetDefaultContainerAsync(ProductConfigId);

	return GetContainerTask.then([PlatformSlotName](GameSaveContainer ^ Container)
	{
		if (nullptr != Container)
		{
			UE_LOG(LogUWPSaveGame, Log, TEXT("Attempting to load data from connected storage."));

			// read from xblive connected storage
			Vector< Platform::String ^ > ^ BlobsToRead = ref new Vector< Platform::String ^ >();
			BlobsToRead->Append(PlatformSlotName);

			IAsyncOperation< GameSaveBlobGetResult ^ > ^ ReadOp = Container->GetAsync(BlobsToRead);
			auto ReadResultTask = create_task(ReadOp);

			return ReadResultTask.then([PlatformSlotName](GameSaveBlobGetResult ^ Result)
			{
				IBuffer ^ DataBuffer = nullptr;

				if (nullptr != Result)
				{
					auto Status = Result->Status;
					if (GameSaveErrorStatus::Ok == Status)
					{
						IMapView< Platform::String ^, IBuffer ^ > ^ DataMap = Result->Value;
						if (nullptr != DataMap)
						{
							DataBuffer = DataMap->Lookup(PlatformSlotName);
						}
					}
				}

				return DataBuffer;
			});
		}

		return create_task([]() -> IBuffer ^
		{
			return nullptr;
		});
	});
}

static task< bool > DeleteAsyncAux(Platform::String ^	ProductConfigId,
	Platform::String ^	PlatformSlotName)
{
	task< GameSaveContainer ^ > GetContainerTask = GetDefaultContainerAsync(ProductConfigId);

	return GetContainerTask.then([PlatformSlotName](GameSaveContainer ^ Container)
	{
		if (nullptr != Container)
		{
			UE_LOG(LogUWPSaveGame, Log, TEXT("Attempting to delete data from connected storage."));

			Vector< Platform::String ^ > ^ BlobsToDelete = ref new Vector< Platform::String ^ >();
			BlobsToDelete->Append(PlatformSlotName);

			IAsyncOperation< GameSaveOperationResult ^ >^ DeleteOp = Container->SubmitUpdatesAsync(nullptr, BlobsToDelete, nullptr);
			auto DeleteTask = create_task(DeleteOp);

			return DeleteTask.then([](GameSaveOperationResult ^ Result)
			{
				if (nullptr != Result)
				{
					return (GameSaveErrorStatus::Ok == Result->Status);
				}

				return false;
			});
		}

		return create_task([]() -> bool
		{
			return false;
		});
	});
}






ISaveGameSystem::ESaveExistsResult FUWPSaveGameSystem::DoesSaveGameExistWithResult(const TCHAR *	Name,
	const int32	 /* UserIndex */)
{
	ESaveExistsResult Result = ESaveExistsResult::UnspecifiedError;

	try
	{
		Platform::String ^ ProductConfigId = GetProductConfigId();
		if (nullptr != ProductConfigId)
		{
			Platform::String ^ PlatformSlotName = ref new Platform::String(Name);


			auto ExistsTask = ExistsAsyncAux(ProductConfigId, PlatformSlotName);

			while (!ExistsTask.is_done())
			{
				FSlateApplication::Get().PumpMessages();
			}

			Result = ExistsTask.get();
		}
		else
		{
			UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::DoesSaveGameExist() failed - unable to get Product Config Id"));
		}
	}
	catch (Platform::Exception ^ Ex)
	{
		UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::DoesSaveGameExist() failed (0x%08X) - \"%s\""), Ex->HResult, ((nullptr != Ex->Message) ? Ex->Message->Data() : TEXT("Unknown Error")));
	}


	return Result;
}

bool FUWPSaveGameSystem::SaveGame(bool					 /* bAttemptToUseUI */,
	const TCHAR *			Name,
	const int32			 /* UserIndex */,
	const TArray< uint8 > &	SrcData)
{
	bool Result = false;


	try
	{
		Platform::String ^ ProductConfigId = GetProductConfigId();
		if (nullptr != ProductConfigId)
		{
			Platform::String ^ PlatformSlotName = ref new Platform::String(Name);


			auto SaveDataTask = SaveAsyncAux(ProductConfigId, PlatformSlotName, SrcData);

			while (!SaveDataTask.is_done())
			{
				FSlateApplication::Get().PumpMessages();
			}

			Result = SaveDataTask.get();
		}
		else
		{
			UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::SaveGame() failed - unable to get Product Config Id"));
		}
	}
	catch (Platform::Exception ^ Ex)
	{
		UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::SaveGame() failed (0x%08X) - \"%s\""), Ex->HResult, ((nullptr != Ex->Message) ? Ex->Message->Data() : TEXT("Unknown Error")));
	}


	return Result;
}

bool FUWPSaveGameSystem::LoadGame(bool				 /* bAttemptToUseUI */,
	const TCHAR *		Name,
	const int32		 /* UserIndex */,
	TArray< uint8 > &	DestData)
{
	bool Result = false;


	try
	{
		Platform::String ^ ProductConfigId = GetProductConfigId();
		if (nullptr != ProductConfigId)
		{
			Platform::String ^ PlatformSlotName = ref new Platform::String(Name);


			auto LoadDataTask = LoadAsyncAux(ProductConfigId, PlatformSlotName);

			auto CopyBufferTask = LoadDataTask.then([&DestData](IBuffer ^ DataBuffer)
			{
				bool Copied = false;

				if ((nullptr != DataBuffer) &&
					(CopyStorageBuffer(DestData, DataBuffer)))
				{
					Copied = true;
				}

				return Copied;
			});

			while (!CopyBufferTask.is_done())
			{
				FSlateApplication::Get().PumpMessages();
			}

			Result = CopyBufferTask.get();
		}
		else
		{
			UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::LoadGame() failed - unable to get Product Config Id"));
		}
	}
	catch (Platform::Exception ^ Ex)
	{
		UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::LoadGame() failed (0x%08X) - \"%s\""), Ex->HResult, ((nullptr != Ex->Message) ? Ex->Message->Data() : TEXT("Unknown Error")));
	}


	return Result;
}

bool FUWPSaveGameSystem::DeleteGame(bool			 /* bAttemptToUseUI */,
	const TCHAR *		Name,
	const int32	 /* UserIndex */)
{
	bool Result = false;


	try
	{
		Platform::String ^ ProductConfigId = GetProductConfigId();
		if (nullptr != ProductConfigId)
		{
			Platform::String ^ PlatformSlotName = ref new Platform::String(Name);


			auto DeleteTask = DeleteAsyncAux(ProductConfigId, PlatformSlotName);

			while (!DeleteTask.is_done())
			{
				FSlateApplication::Get().PumpMessages();
			}

			Result = DeleteTask.get();
		}
		else
		{
			UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::DeleteGame() failed - unable to get Product Config Id"));
		}
	}
	catch (Platform::Exception ^ Ex)
	{
		UE_LOG(LogUWPSaveGame, Warning, TEXT("FUWPSaveGameSystem::DeleteGame() failed (0x%08X) - \"%s\""), Ex->HResult, ((nullptr != Ex->Message) ? Ex->Message->Data() : TEXT("Unknown Error")));
	}


	return Result;
}

