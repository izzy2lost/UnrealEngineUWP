//// Copyright Epic Games, Inc. All Rights Reserved.
//#include "MixerEngineGameInstance.h"
//#include "MixerEngine/MixerEngine.h"
//#include "RenderCommandFence.h"
//#include "Kismet/GameplayStatics.h" 
//#include "2D/Tex.h"
//#include "Device/FX/DeviceBuffer_FX.h"
//#include "Data/TiledBlob.h" 
//#include "Data/Blobber.h"
//#include "Templates/Casts.h"
//#include "Helper/Util.h"
//#include "Engine/World.h"
//
//
//TArray<UTexture*> UMixerEngineGameInstance::renderTextures;
//void UMixerEngineGameInstance::AddToRenderList(UTexture* target)
//{
//#if !UE_BUILD_SHIPPING
//	if (!renderTextures.Contains(target))
//		renderTextures.Add(target);
//	else
//		UE_LOG(LogTemp, Warning, TEXT("Already in rt list %s"), *target->GetName());
//#endif
//}
//
//void UMixerEngineGameInstance::AddToRenderList(TexPtr target)
//{
//#if !UE_BUILD_SHIPPING
//	UTexture* rtTexture = target->Texture();
//	if (IsValid(rtTexture))
//	{
//		AddToRenderList(rtTexture);
//	}
//#endif
//}
//
//void UMixerEngineGameInstance::AddToRenderList(BlobPtr target)
//{
//#if !UE_BUILD_SHIPPING
//	if (!target)
//		return;
//	DeviceBuffer_FX* fxBuffer;
//	if(target->Buffer())
//		fxBuffer = static_cast<DeviceBuffer_FX*>(target->Buffer().Get());
//	else
//	{
//		TiledBlobPtr tiledBlob = std::static_pointer_cast<TiledBlob>(target);
//		fxBuffer = static_cast<DeviceBuffer_FX*>(tiledBlob->Tile(0,0)->Buffer().Get());
//	}
//	if (fxBuffer)
//	{
//		TexPtr dstTex = fxBuffer->Texture();
//		if (dstTex)
//		{
//			AddToRenderList(dstTex);
//		}
//	}
//#endif
//}
//
//UTexture* UMixerEngineGameInstance::GetFromRenderList(int index)
//{
//#if !UE_BUILD_SHIPPING
//	if (index >= renderTextures.Num() || renderTextures.Num()==0)
//		return nullptr;
//
//	return renderTextures[index];
//#endif
//	return nullptr;
//}
//
//TArray<UTexture*> UMixerEngineGameInstance::GetRenderTextureList()
//{
//	return renderTextures;
//}
//
//
//UMixerEngineGameInstance::UMixerEngineGameInstance(const FObjectInitializer& objectInitializer)
//{
//}
//
//UMixerEngineGameInstance::~UMixerEngineGameInstance()
//{
//	/*if (_authSysHandle)
//	{
//		delete _authSysHandle;
//		_authSysHandle = nullptr;
//	}*/
//
//	UE_LOG(LogTemp, Log, TEXT("Mixer Engine game instance destroyed"))
//}
//
//void UMixerEngineGameInstance::DisplayError(FText title, FText message)
//{
//	
//	UE_LOG(LogTemp, Log, TEXT("Error : %s : %s"),*title.ToString(), *message.ToString());
//}
//
//void UMixerEngineGameInstance::DisplayProgress(IProgressCallbackPtr progress)
//{
//	UE_LOG(LogTemp, Log, TEXT("Progress displayed"));
//}
//
//void UMixerEngineGameInstance::Shutdown()
//{
//	Super::Shutdown();
//	ClearRenderList();
//}
//
//void UMixerEngineGameInstance::ClearRenderList()
//{
//	renderTextures.Empty();
//}
//
//void UMixerEngineGameInstance::ReportError(FText title, FText message)
//{
//	UMixerEngineGameInstance* gameInstance = Cast<UMixerEngineGameInstance>(Util::GetGameWorld()->GetGameInstance());
//	if(gameInstance)
//		gameInstance->DisplayError(title, message);
//}
//
//void UMixerEngineGameInstance::EnableProgressCallback(IProgressCallbackPtr progress)
//{
//	UMixerEngineGameInstance* gameInstance = Cast<UMixerEngineGameInstance>(Util::GetGameWorld()->GetGameInstance());
//	if(gameInstance)
//		gameInstance->DisplayProgress(progress);
//}
//
//int32 UMixerEngineGameInstance::GetObjReferenceCount(UObject* Obj, TArray<UObject*>* OutReferredToObjects /*= nullptr*/)
//{
//	if (!Obj || !Obj->IsValidLowLevelFast())
//	{
//		return -1;
//	}
//
//	TArray<UObject*> ReferredToObjects;		
//	FReferenceFinder ObjectReferenceCollector(ReferredToObjects, Obj, false, true, true, false);
//	ObjectReferenceCollector.FindReferences(Obj);
//
//	int32 retval = 0;
//	if (OutReferredToObjects)
//	{
//		OutReferredToObjects->Append(ReferredToObjects);
//		retval = OutReferredToObjects->Num();
//	}
//	return retval;
//}
//
//void UMixerEngineGameInstance::LogObjectReferences(UObject* Obj)
//{
//	TArray<UObject*> ObjectArray;
//	GetObjReferenceCount(Obj, &ObjectArray);
//	for (UObject* Each : ObjectArray)
//	{
//		if (Each)
//		{
//			UE_LOG(LogTemp, Warning, TEXT("%s"), *Each->GetName());
//		}
//	}
//}
//
