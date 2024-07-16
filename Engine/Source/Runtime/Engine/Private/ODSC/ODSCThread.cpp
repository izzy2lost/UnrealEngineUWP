// Copyright Epic Games, Inc. All Rights Reserved.

#include "ODSCThread.h"
#include "CookOnTheFly.h"
#include "ODSCLog.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

FODSCRequestPayload::FODSCRequestPayload(
	EShaderPlatform InShaderPlatform,
	ERHIFeatureLevel::Type InFeatureLevel,
	EMaterialQualityLevel::Type InQualityLevel,
	const FString& InMaterialName,
	const FString& InVertexFactoryName,
	const FString& InPipelineName,
	const TArray<FString>& InShaderTypeNames,
	int32 InPermutationId,
	const FString& InRequestHash
)
: ShaderPlatform(InShaderPlatform)
, FeatureLevel(InFeatureLevel)
, QualityLevel(InQualityLevel)
, MaterialName(InMaterialName)
, VertexFactoryName(InVertexFactoryName)
, PipelineName(InPipelineName)
, ShaderTypeNames(std::move(InShaderTypeNames))
, PermutationId(InPermutationId)
, RequestHash(InRequestHash)
{

}

FArchive& operator<<(FArchive& Ar, FODSCRequestPayload& Payload)
{
	int32 iShaderPlatform = static_cast<int32>(Payload.ShaderPlatform);
	int32 iFeatureLevel = static_cast<int32>(Payload.FeatureLevel);
	int32 iQualityLevel = static_cast<int32>(Payload.QualityLevel);

	Ar << iShaderPlatform;
	Ar << iFeatureLevel;
	Ar << iQualityLevel;
	Ar << Payload.MaterialName;
	Ar << Payload.VertexFactoryName;
	Ar << Payload.PipelineName;
	Ar << Payload.ShaderTypeNames;
	Ar << Payload.PermutationId;
	Ar << Payload.RequestHash;

	if (Ar.IsLoading())
	{
		Payload.ShaderPlatform = static_cast<EShaderPlatform>(iShaderPlatform);
		Payload.FeatureLevel = static_cast<ERHIFeatureLevel::Type>(iFeatureLevel);
		Payload.QualityLevel = static_cast<EMaterialQualityLevel::Type>(iQualityLevel);
	}

	return Ar;
}

FODSCMessageHandler::FODSCMessageHandler(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type InQualityLevel, ODSCRecompileCommand InRecompileCommandType)
:	ShaderPlatform(InShaderPlatform),
	FeatureLevel(InFeatureLevel),
	QualityLevel(InQualityLevel),
	RecompileCommandType(InRecompileCommandType)
{
}

FODSCMessageHandler::FODSCMessageHandler(const TArray<FString>& InMaterials, const FString& ShaderTypesToLoad, EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type InQualityLevel, ODSCRecompileCommand InRecompileCommandType) :
	MaterialsToLoad(std::move(InMaterials)),
	ShaderTypesToLoad(ShaderTypesToLoad),
	ShaderPlatform(InShaderPlatform),
	FeatureLevel(InFeatureLevel),
	QualityLevel(InQualityLevel),
	RecompileCommandType(InRecompileCommandType)
{
}

void FODSCMessageHandler::FillPayload(FArchive& Payload)
{
	// When did we start this request?
	RequestStartTime = FPlatformTime::Seconds();

	int32 ConvertedShaderPlatform = static_cast<int32>(ShaderPlatform);
	int32 ConvertedFeatureLevel = static_cast<int32>(FeatureLevel);
	int32 ConvertedQualityLevel = static_cast<int32>(QualityLevel);

	Payload << MaterialsToLoad;
	Payload << ShaderTypesToLoad;
	Payload << ConvertedShaderPlatform;
	Payload << ConvertedFeatureLevel;
	Payload << ConvertedQualityLevel;
	Payload << RecompileCommandType;
	Payload << RequestBatch;
}

void FODSCMessageHandler::ProcessResponse(FArchive& Response)
{
	UE_LOG(LogODSC, Display, TEXT("Received response in %lf seconds."), FPlatformTime::Seconds() - RequestStartTime);

	// pull back the compiled mesh material data (if any)
	Response << OutMeshMaterialMaps;
	Response << OutGlobalShaderMap;
}

void FODSCMessageHandler::AddPayload(const FODSCRequestPayload& Payload)
{
	RequestBatch.Add(Payload);
}

const TArray<FString>& FODSCMessageHandler::GetMaterialsToLoad() const
{
	return MaterialsToLoad;
}

const TArray<uint8>& FODSCMessageHandler::GetMeshMaterialMaps() const
{
	return OutMeshMaterialMaps;
}

const TArray<uint8>& FODSCMessageHandler::GetGlobalShaderMap() const
{
	return OutGlobalShaderMap;
}

bool FODSCMessageHandler::ReloadGlobalShaders() const
{
	return RecompileCommandType == ODSCRecompileCommand::Global;
}

FODSCThread::FODSCThread(const FString& HostIP)
	: Thread(nullptr)
	, WakeupEvent(FPlatformProcess::GetSynchEventFromPool(true))
	, AllRequestsDoneEvent(FPlatformProcess::GetSynchEventFromPool(true))
	, ODSCHostIP(HostIP)
{
	UE_LOG(LogODSC, Log, TEXT("ODSC Thread active."));

	bHasDefaultConnection = (FModuleManager::LoadModuleChecked<UE::Cook::ICookOnTheFlyModule>(TEXT("CookOnTheFly")).GetDefaultServerConnection() != nullptr);
	if (!bHasDefaultConnection)
	{
		ConnectToODSCHost();
	}
}

FODSCThread::~FODSCThread()
{
	StopThread();

	FPlatformProcess::ReturnSynchEventToPool(AllRequestsDoneEvent);
	AllRequestsDoneEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(WakeupEvent);
	WakeupEvent = nullptr;
}

bool FODSCThread::ConnectToODSCHost()
{
	// If we don't have a default connection make a specific connection to the HostIP provided.
	UE::Cook::FCookOnTheFlyHostOptions CookOnTheFlyHostOptions;
	CookOnTheFlyHostOptions.Hosts.Add(ODSCHostIP);
	CookOnTheFlyServerConnection = FModuleManager::LoadModuleChecked<UE::Cook::ICookOnTheFlyModule>(TEXT("CookOnTheFly")).ConnectToServer(CookOnTheFlyHostOptions);
	if (!CookOnTheFlyServerConnection)
	{
		UE_LOG(LogODSC, Warning, TEXT("Failed to connect to cook on the fly server."));
		return false;
	}
	return CookOnTheFlyServerConnection != nullptr && CookOnTheFlyServerConnection->IsConnected();
}

bool FODSCThread::CheckODSCConnection()
{
	// If we have a default connection that already exists, send directly to that.
	if ((CookOnTheFlyServerConnection == nullptr) || (!CookOnTheFlyServerConnection->IsConnected()))
	{
		// Losing connection when exit is requested is expected, do not try to reconnect
		if (ExitRequest.GetValue())
		{
			return false;
		}

		UE_LOG(LogODSC, Display, TEXT("Detected that CookOnTheFlyServerConnection has been lost, trying again"));
		if (!ConnectToODSCHost())
		{
			return false;
		}
	}
	return CookOnTheFlyServerConnection != nullptr && CookOnTheFlyServerConnection->IsConnected();
}

void FODSCThread::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("ODSCThread"), 128 * 1024, TPri_Normal);
}

void FODSCThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

void FODSCThread::Tick()
{
	Process();
}

void FODSCThread::ResetMaterialsODSCData(ERHIFeatureLevel::Type FeatureLevel)
{
#if WITH_ODSC
	FlushRenderingCommands();

	{
		FWriteScopeLock WriteLock(RequestHashesRWLock);

		// this will stop the rendering thread, and reattach components, in the destructor
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::Default);
		RequestHashes.Empty();

		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{ 
			UMaterialInterface* Material = *It;
			if (Material)
			{
				const FMaterialResource* MaterialResource = Material->GetMaterialResource((ERHIFeatureLevel::Type)FeatureLevel);
				if (MaterialResource && MaterialResource->GetGameThreadShaderMap())
				{
					MaterialResource->GetGameThreadShaderMap()->SetIsFromODSC(false);
#if WITH_ODSC
					MaterialResource->SetODSCMetaData((uint8)0);
#endif
				}
				UpdateContext.AddMaterialInterface(Material);
			}
		}
	}
#endif
}

FODSCThread::FODSCShaderId::FODSCShaderId(const FShaderId& ShaderId)
: MaterialShaderMapHash(ShaderId.MaterialShaderMapHash)
, ShaderTypeHashedName(ShaderId.Type ? ShaderId.Type->GetHashedName() : 0)
, VFTypeHashedName(ShaderId.VFType ? ShaderId.VFType->GetHashedName() : 0)
, ShaderPipelineName(ShaderId.ShaderPipelineName)
, PermutationId(ShaderId.PermutationId)
, Platform(ShaderId.Platform)
{}

bool FODSCThread::CheckIfRequestAlreadySent(const TArray<FShaderId>& RequestShaderIds, const FString& MaterialName) const
{
	FReadScopeLock ReadLock(RequestHashesRWLock);
	for (const FShaderId& ShaderId : RequestShaderIds)
	{
		const FMaterialRequestsHashes* MaterialRequestHashes = RequestHashes.Find(ShaderId);
		if (MaterialRequestHashes == nullptr)
		{
			return false;
		}
			
		if (!MaterialRequestHashes->RequestStrings.Contains(MaterialName))
		{
			return false;
		}
	}

	return true;
}

void FODSCThread::AddRequest(const TArray<FString>& MaterialsToCompile, const FString& ShaderTypesToLoad, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel, EMaterialQualityLevel::Type QualityLevel, ODSCRecompileCommand RecompileCommandType)
{
	PendingMaterialThreadedRequests.Enqueue(new FODSCMessageHandler(MaterialsToCompile, ShaderTypesToLoad, ShaderPlatform, FeatureLevel, QualityLevel, RecompileCommandType));
}

void FODSCThread::AddShaderPipelineRequest(
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	const FString& MaterialName,
	const FString& VertexFactoryName,
	const FString& PipelineName,
	const TArray<FString>& ShaderTypeNames,
	int32 PermutationId,
	const TArray<FShaderId>& RequestShaderIds
)
{
	bool bShouldAddRequest = false;
	{
		FWriteScopeLock WriteLock(RequestHashesRWLock);

		for (const FShaderId& ShaderId : RequestShaderIds)
		{
			FMaterialRequestsHashes& MaterialRequestHashes = RequestHashes.FindOrAdd(ShaderId);
			bool bAlreadyInSet = false;
			MaterialRequestHashes.RequestStrings.Add(MaterialName, &bAlreadyInSet);
			if (!bAlreadyInSet)
			{
				bShouldAddRequest = true;
			}
		}
	}

	if (bShouldAddRequest)
	{
		SCOPED_NAMED_EVENT(AddShaderPipelineRequest_AddRequest, FColor::Emerald);

		FString RequestString = (MaterialName + VertexFactoryName + PipelineName);
		for (const auto& ShaderTypeName : ShaderTypeNames)
		{
			RequestString += ShaderTypeName;
		}
		const FString RequestHash = FMD5::HashAnsiString(*RequestString);
		PendingMeshMaterialThreadedRequests.Enqueue(FODSCRequestPayload(ShaderPlatform, FeatureLevel, QualityLevel, MaterialName, VertexFactoryName, PipelineName, ShaderTypeNames, PermutationId, RequestHash));
	}
}

void FODSCThread::RegisterMaterialShaderMap(const FMaterialShaderMap& MaterialShaderMap)
{
	FWriteScopeLock WriteLock(RequestHashesRWLock);

	TMap<FShaderId, TShaderRef<FShader>> ShadersInMap;
	MaterialShaderMap.GetShaderList(ShadersInMap);
	FSHAHash CookedShaderMapIdHash = MaterialShaderMap.GetShaderMapId().CookedShaderMapIdHash;
	for (auto Iter : ShadersInMap)
	{
		// GetShaderList doesn't use the Cookedshadermap id
		const FShaderId& ShaderIdSrc = Iter.Key;
		FShaderId ShaderIdCopy(ShaderIdSrc.Type, CookedShaderMapIdHash, ShaderIdSrc.ShaderPipelineName, ShaderIdSrc.VFType, ShaderIdSrc.PermutationId, (EShaderPlatform)ShaderIdSrc.Platform);

		// The shadermap we receive contains all the requests the client sent until now, so it's possible they got removed from RequestHashes already
		if (RequestHashes.Find(ShaderIdCopy))
		{
			RequestHashes.Remove(ShaderIdCopy);
		}
    }
}

void FODSCThread::GetCompletedRequests(TArray<FODSCMessageHandler*>& OutCompletedRequests)
{
	check(IsInGameThread());
	FODSCMessageHandler* Request = nullptr;
	while (CompletedThreadedRequests.Dequeue(Request))
	{
		OutCompletedRequests.Add(Request);
	}
}

void FODSCThread::Wakeup()
{
	AllRequestsDoneEvent->Reset();
	WakeupEvent->Trigger();
}

void FODSCThread::WaitUntilAllRequestsDone()
{
	AllRequestsDoneEvent->Wait();
}

bool FODSCThread::Init()
{
	return true;
}

uint32 FODSCThread::Run()
{
	while (!ExitRequest.GetValue())
	{
		if (WakeupEvent->Wait())
		{
			Process();
		}
	}
	return 0;
}

void FODSCThread::Stop()
{
	ExitRequest.Set(true);
	WakeupEvent->Trigger();
}

void FODSCThread::Exit()
{

}

void FODSCThread::Process()
{
	// cache all pending pipeline requests
	{
		TArray<FODSCRequestPayload> PayloadsToAggregate;
		FODSCRequestPayload Payload;
		while (PendingMeshMaterialThreadedRequests.Dequeue(Payload))
		{
			PayloadsToAggregate.Add(Payload);
		}

		if (PayloadsToAggregate.Num())
		{
			FODSCMessageHandler* RequestHandler = new FODSCMessageHandler(PayloadsToAggregate[0].ShaderPlatform, PayloadsToAggregate[0].FeatureLevel, PayloadsToAggregate[0].QualityLevel, ODSCRecompileCommand::Material);
			for (const FODSCRequestPayload& payload : PayloadsToAggregate)
			{
				RequestHandler->AddPayload(payload);
			}
			PendingRequestsPipeline.Add(RequestHandler);
		}
	}

	// cache all pending material/global requests
	{
		FODSCMessageHandler* Request = nullptr;
		while (PendingMaterialThreadedRequests.Dequeue(Request))
		{
			PendingRequestsMaterialAndGlobal.Add(Request);
		}
	}

	bIsConnectedToODSCServer = CheckODSCConnection();

	ON_SCOPE_EXIT
	{
		// SendMessageToServer is synchronous, so when we're here, we know we've processed all the requests
		WakeupEvent->Reset();
		AllRequestsDoneEvent->Trigger();
	};

	// Early out to avoid trying to connect (and most likely fail) for every compilation request
	if (!bIsConnectedToODSCServer)
	{
		return;
	}

	// cache material requests.
	TArray<FODSCMessageHandler*> RequestsToStart = MoveTemp(PendingRequestsMaterialAndGlobal);
	bool bHasGlobalShaders = false;
	uint32 NumMaterials = 0;

	for (FODSCMessageHandler* NextRequest : RequestsToStart)
	{
		if (NextRequest->GetRecompileCommandType() != ODSCRecompileCommand::Material)
		{
			bHasGlobalShaders = true;
		}
		else
		{
			NumMaterials += NextRequest->GetMaterialsToLoad().Num();
		}
	}

	bHasPendingGlobalShaders.store(bHasGlobalShaders, std::memory_order_release);
	NumPendingMaterialsRecompile.store(NumMaterials, std::memory_order_release);

	// process any material or recompile change shader requests or global shader compile requests.
	for (FODSCMessageHandler* NextRequest : RequestsToStart)
	{
		// send the info, the handler will process the response (and update shaders, etc)
		if (SendMessageToServer(NextRequest))
		{
			CompletedThreadedRequests.Enqueue(NextRequest);
		}
		else
		{
			PendingRequestsMaterialAndGlobal.Add(NextRequest);
		}

	}

	bHasPendingGlobalShaders.store(false, std::memory_order_release);
	NumPendingMaterialsRecompile.store(0, std::memory_order_release);

	RequestsToStart = MoveTemp(PendingRequestsPipeline);

	uint32 NumPipelines = 0;
	for (FODSCMessageHandler* NextRequest : RequestsToStart)
	{
		NumPipelines += NextRequest->NumPayloads();
	}

	NumPendingMaterialsShaders.store(NumPipelines, std::memory_order_release);

	// process any specific mesh material shader requests.
	for (FODSCMessageHandler* NextRequest : RequestsToStart)
	{
		if (SendMessageToServer(NextRequest))
		{
			CompletedThreadedRequests.Enqueue(NextRequest);
		}
		else
		{
			PendingRequestsPipeline.Add(NextRequest);
		}
	}

	NumPendingMaterialsShaders.store(0, std::memory_order_release);
}

bool FODSCThread::SendMessageToServer(IPlatformFile::IFileServerMessageHandler* Handler)
{
	if (bHasDefaultConnection)
	{
		IFileManager::Get().SendMessageToServer(TEXT("RecompileShaders"), Handler);
		return true;
	}

	if (!CheckODSCConnection())
	{
		return false;
	}

	// We don't have a default COTF connection so use our specific connection to send our command.
	UE::Cook::FCookOnTheFlyRequest Request(UE::Cook::ECookOnTheFlyMessage::RecompileShaders);
	{
		TUniquePtr<FArchive> Ar = Request.WriteBody();
		Handler->FillPayload(*Ar);
	}

	UE::Cook::FCookOnTheFlyResponse Response = CookOnTheFlyServerConnection->SendRequest(Request).Get();
	if (Response.IsOk())
	{
		TUniquePtr<FArchive> Ar = Response.ReadBody();
		Handler->ProcessResponse(*Ar);
		return true;
	}
	else
	{
		UE_LOG(LogODSC, Display, TEXT("Received error response from CookOnTheFlyServerConnection; disconnecting"));
		CookOnTheFlyServerConnection.Reset();
		return false;
	}
}

bool FODSCThread::GetPendingShaderData(bool& bOutIsConnectedToODSCServer, bool& bOutHasPendingGlobalShaders, uint32& OutNumPendingMaterialsRecompile, uint32& OutNumPendingMaterialsShaders) const
{
	bOutIsConnectedToODSCServer = bIsConnectedToODSCServer.load(std::memory_order_acquire);
	bOutHasPendingGlobalShaders = bHasPendingGlobalShaders.load(std::memory_order_acquire);
	OutNumPendingMaterialsRecompile = NumPendingMaterialsRecompile.load(std::memory_order_acquire);
	OutNumPendingMaterialsShaders = NumPendingMaterialsShaders.load(std::memory_order_acquire);
	return bOutHasPendingGlobalShaders || OutNumPendingMaterialsRecompile > 0 || OutNumPendingMaterialsShaders > 0;
}
