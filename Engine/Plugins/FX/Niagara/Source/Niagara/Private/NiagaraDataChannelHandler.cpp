// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelCommon.h"

void UNiagaraDataChannelHandler::BeginDestroy()
{
	Super::BeginDestroy();
	Cleanup();
}

void UNiagaraDataChannelHandler::Init(const UNiagaraDataChannel* InChannel)
{
	DataChannel = InChannel;
}

void UNiagaraDataChannelHandler::Cleanup()
{
	if(Reader)
	{
		Reader->Cleanup();
		Reader = nullptr;
	}
	
	if(Writer)
	{
		Writer->Cleanup();
		Writer = nullptr;
	}
}

void UNiagaraDataChannelHandler::BeginFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	CurrentTG = TG_PrePhysics;
}

void UNiagaraDataChannelHandler::EndFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{

}

void UNiagaraDataChannelHandler::Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
	CurrentTG = TickGroup;
}

UNiagaraDataChannelWriter* UNiagaraDataChannelHandler::GetDataChannelWriter()
{
	if(Writer == nullptr)
	{
		Writer =  NewObject<UNiagaraDataChannelWriter>();
		Writer->Owner = this;
	}
	return Writer;
}

UNiagaraDataChannelReader* UNiagaraDataChannelHandler::GetDataChannelReader()
{
	if (Reader == nullptr)
	{
		Reader = NewObject<UNiagaraDataChannelReader>();
		Reader->Owner = this;
	}
	return Reader;
}

FNiagaraDataChannelDataPtr UNiagaraDataChannelHandler::CreateData()
{
	FNiagaraDataChannelDataPtr Ret = MakeShared<FNiagaraDataChannelData>();
	Ret->Init(this);
	return Ret;
}