//// Copyright Epic Games, Inc. All Rights Reserved.
//#pragma once
//
//#include "CoreMinimal.h"
//#include "Engine/GameInstance.h"
//
////Authentication system session
////#include "Network/AuthenticationSystem.h"
//#include "Model/Action/ActionCommon.h"
//#include "Data/Blob.h"
//
//#include "MixerEngineGameInstance.generated.h"
//
///**
// * 
// */
//class Tex;
//class TiledBlob;
//typedef std::shared_ptr<TiledBlob>	TiledBlobPtr;
//typedef std::shared_ptr<Tex>		TexPtr;
//UCLASS()
//class MIXERENGINE_API UMixerEngineGameInstance : public UGameInstance
//{
//	GENERATED_BODY()
//private:
//
//	//AuthenticationSystem*						_authSysHandle = nullptr; ///Authentication system that holds current user info
//	static TArray<UTexture*>					renderTextures;				//Array containing all the game textures in use 
//																			
//public:									
//												UMixerEngineGameInstance(const FObjectInitializer& objectInitializer);
//												~UMixerEngineGameInstance();
//	//AuthenticationSystem*						GetAuthenticationSystem() { return _authSysHandle ? _authSysHandle : nullptr; }
//	virtual void								DisplayError(FText title, FText message);
//	virtual void								DisplayProgress(IProgressCallbackPtr progress);
//	virtual	void								Shutdown() override;
//
//	
//
//	//////////////////////////////////////////////////////////////////////////
//	//// Static functions
//	//////////////////////////////////////////////////////////////////////////
//	static void									ReportError(FText title = FText::FromStringTable("/MixerEditor/UI/StringTables/LocalizationTable.LocalizationTable", "Error"), FText message = FText::FromStringTable("/MixerEditor/UI/StringTables/LocalizationTable.LocalizationTable", "SomethingWrongTryAgain"));
//	static void									EnableProgressCallback(IProgressCallbackPtr progress);
//	static int32								GetObjReferenceCount(UObject* Obj, TArray<UObject*>* OutReferredToObjects = nullptr);
//	static void									LogObjectReferences(UObject* Obj);
//	
//	static	void								AddToRenderList(UTexture* target);
//	static	void								AddToRenderList(TexPtr target);
//	static	void								AddToRenderList(BlobPtr target);
//
//	UFUNCTION(BlueprintCallable, Category = "Resource View")
//	static	void								ClearRenderList();
//
//	UFUNCTION(BlueprintCallable, Category = "Resource View")
//	static	TArray<UTexture*>					GetRenderTextureList();
//
//	UFUNCTION(BlueprintCallable, Category = "Resource View")
//	static UTexture*							GetFromRenderList(int index);
//};
//
