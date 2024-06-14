// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalDynamicRHI_Shaders.cpp: Metal Dynamic RHI Class Shader Methods.
=============================================================================*/

#include "MetalDynamicRHI.h"
#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "Shaders/MetalShaderLibrary.h"
#include "DataDrivenShaderPlatformInfo.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Shader Methods


FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalVertexShader* Shader = new FMetalVertexShader(Code);
	return Shader;
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalPixelShader* Shader = new FMetalPixelShader(Code);
	return Shader;
}


FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    FMetalGeometryShader* Shader = new FMetalGeometryShader(Code);
    return Shader;
#else
	return nullptr;
#endif
}


FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    return new FMetalComputeShader(Code, MTLLibraryPtr());
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
FMeshShaderRHIRef FMetalDynamicRHI::RHICreateMeshShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;

    return new FMetalMeshShader(Code);
}

FAmplificationShaderRHIRef FMetalDynamicRHI::RHICreateAmplificationShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
    MTL_SCOPED_AUTORELEASE_POOL;

    return new FMetalAmplificationShader(Code);
}
#endif

#if METAL_RHI_RAYTRACING
FRayTracingShaderRHIRef FMetalDynamicRHI::RHICreateRayTracingShader(TArrayView<const uint8> Code, const FSHAHash& Hash, EShaderFrequency ShaderFrequency)
{
	switch (ShaderFrequency)
    {
		default: checkNoEntry(); return nullptr;
	}
}
#endif // METAL_RHI_RAYTRACING

FRHIShaderLibraryRef FMetalDynamicRHI::RHICreateShaderLibrary(EShaderPlatform Platform, FString const& FilePath, FString const& Name)
{
	FString METAL_MAP_EXTENSION(TEXT(".metalmap"));

    MTL_SCOPED_AUTORELEASE_POOL;
    
    FRHIShaderLibraryRef Result = nullptr;

    const FName PlatformName = FDataDrivenShaderPlatformInfo::GetName(Platform);
    const FName ShaderFormatName = LegacyShaderPlatformToShaderFormat(Platform);
    FString ShaderFormatAndPlatform = ShaderFormatName.ToString() + TEXT("-") + PlatformName.ToString();

    FString LibName = FString::Printf(TEXT("%s_%s"), *Name, *ShaderFormatAndPlatform);
    LibName.ToLowerInline();

    FString BinaryShaderFile = FilePath / LibName + METAL_MAP_EXTENSION;

    if (IFileManager::Get().FileExists(*BinaryShaderFile) == false)
    {
        // the metal map files are stored in UFS file system
        // for pak files this means they might be stored in a different location as the pak files will mount them to the project content directory
        // the metal libraries are stores non UFS and could be anywhere on the file system.
        // if we don't find the metalmap file straight away try the pak file path
        BinaryShaderFile = FPaths::ProjectContentDir() / LibName + METAL_MAP_EXTENSION;
    }

    FScopeLock Lock(&FMetalShaderLibrary::LoadedShaderLibraryMutex);

    FRHIShaderLibrary** FoundShaderLibrary = FMetalShaderLibrary::LoadedShaderLibraryMap.Find(BinaryShaderFile);
    if (FoundShaderLibrary)
    {
        return *FoundShaderLibrary;
    }

    FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*BinaryShaderFile);

    if( BinaryShaderAr != NULL )
    {
        FMetalShaderLibraryHeader Header;
        FSerializedShaderArchive SerializedShaders;
        TArray<uint8> ShaderCode;

        *BinaryShaderAr << Header;
        *BinaryShaderAr << SerializedShaders;
        *BinaryShaderAr << ShaderCode;
        BinaryShaderAr->Flush();
        delete BinaryShaderAr;

        // Would be good to check the language version of the library with the archive format here.
        if (Header.Format == ShaderFormatName.GetPlainNameString())
        {
            check(((SerializedShaders.GetNumShaders() + Header.NumShadersPerLibrary - 1) / Header.NumShadersPerLibrary) == Header.NumLibraries);

            TArray<MTLLibraryPtr> Libraries;
            Libraries.Empty(Header.NumLibraries);

            for (uint32 i = 0; i < Header.NumLibraries; i++)
            {
                FString MetalLibraryFilePath = (FilePath / LibName) + FString::Printf(TEXT(".%d.metallib"), i);
                FString MetalLibraryAbsoluteFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*MetalLibraryFilePath);

                METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibraryFile: %s"), *MetalLibraryAbsoluteFilePath)));
                NS::Error* Error;
                NS::String* MetalLibraryFilePathNSString = FStringToNSString(MetalLibraryAbsoluteFilePath);
                NS::URL *metalLibraryURL = NS::URL::fileURLWithPath(MetalLibraryFilePathNSString);
                MTLLibraryPtr Library = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newLibrary(metalLibraryURL, &Error));

                if (Library.get() == nullptr)
                {
                    // Metallib not found. Is Zen server being used?
                    static const bool bRunningWithZenStore = FPlatformFileManager::Get().FindPlatformFile(TEXT("StorageServer")) != nullptr;
                    if( bRunningWithZenStore )
                    {
                        FArchive* Reader = IFileManager::Get().CreateFileReader(*MetalLibraryFilePath);
                        TArray<uint8> LibraryData;
                        bool Success = false;
                        if (Reader)
                        {
                            const int64 FileSize = Reader->TotalSize();
                            LibraryData.Reset( FileSize + 2 );
                            LibraryData.AddUninitialized( FileSize );
                            Reader->Serialize(LibraryData.GetData(), LibraryData.Num());
                            Success = Reader->Close();
                            delete Reader;
                        }
                        if (Success)
                        {
                            dispatch_data_t data = dispatch_data_create(LibraryData.GetData(), LibraryData.Num(), nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
                            Library = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newLibrary(data, &Error));
                        }
                        else
                        {
                            UE_LOG(LogMetal, Warning, TEXT("Metallib '%s' unable to be read from ZenStore."), *MetalLibraryFilePath);
                            return nullptr;
                        }
                    }
                    else
                    {
                        UE_LOG(LogMetal, Warning, TEXT("Metallib '%s' not found and ZenStore is not being used."), *MetalLibraryFilePath);
                        return nullptr;
                    }
                }

                if (Library.get() != nullptr)
                {
                    Libraries.Add(Library);
                }
                else
                {
                    UE_LOG(LogMetal, Display, TEXT("Failed to create library: %s"), *NSStringToFString(Error->description()));
                    return nullptr;
                }
            }

            Result = new FMetalShaderLibrary(Platform, Name, BinaryShaderFile, Header, SerializedShaders, ShaderCode, Libraries);
            FMetalShaderLibrary::LoadedShaderLibraryMap.Add(BinaryShaderFile, Result.GetReference());
        }
    }
    else
    {
        UE_LOG(LogMetal, Display, TEXT("No .metalmap file found for %s!"), *LibName);
    }

    return Result;
}

FBoundShaderStateRHIRef FMetalDynamicRHI::RHICreateBoundShaderState(
	FRHIVertexDeclaration* VertexDeclarationRHI,
	FRHIVertexShader* VertexShaderRHI,
	FRHIPixelShader* PixelShaderRHI,
	FRHIGeometryShader* GeometryShaderRHI)
{
	NOT_SUPPORTED("RHICreateBoundShaderState");
	return nullptr;
}

FRHIShaderLibraryRef FMetalDynamicRHI::RHICreateShaderLibrary_RenderThread(class FRHICommandListImmediate& RHICmdList, EShaderPlatform Platform, FString FilePath, FString Name)
{
	return RHICreateShaderLibrary(Platform, FilePath, Name);
}
