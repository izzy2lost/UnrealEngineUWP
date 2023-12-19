// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Compiler.h"

#include "MuT/AST.h"
#include "MuT/ErrorLogPrivate.h"

#include "MuR/Operations.h"
#include "MuT/NodeObjectPrivate.h"


namespace mu
{
	
    //!
    struct FStateOptimizationOptions
    {
		uint8 FirstLOD = 0;
		uint8 NumExtraLODsToBuildAfterFirstLOD = 0;
		bool bOnlyFirstLOD = false;
		ETextureCompressionStrategy TextureCompressionStrategy = ETextureCompressionStrategy::None;

        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 4;
            arch << ver;

			arch << FirstLOD;
			arch << bOnlyFirstLOD;
            arch << TextureCompressionStrategy;
			arch << NumExtraLODsToBuildAfterFirstLOD;
        }


        void Unserialise( InputArchive& arch )
        {
            int32_t ver = 0;
            arch >> ver;
			check(ver <= 4);

			if (ver >= 2)
			{
				arch >> FirstLOD;
			}
			else
			{
				FirstLOD = 0;
			}

            arch >> bOnlyFirstLOD;

			if (ver >= 4)
			{
				arch >> TextureCompressionStrategy;
			}
			else
			{
				bool bAvoidRuntimeCompression;
				arch >> bAvoidRuntimeCompression;
				TextureCompressionStrategy = bAvoidRuntimeCompression ? ETextureCompressionStrategy::DontCompressRuntime : ETextureCompressionStrategy::None;
			}

			if (ver == 3)
			{
				int32 OldNumExtraLODsToBuildAfterFirstLOD;
				arch >> OldNumExtraLODsToBuildAfterFirstLOD;
				NumExtraLODsToBuildAfterFirstLOD = OldNumExtraLODsToBuildAfterFirstLOD;
			}
			else if (ver >= 4)
			{
				arch >> NumExtraLODsToBuildAfterFirstLOD;
			}
			else
			{
				NumExtraLODsToBuildAfterFirstLOD = 0;
			}

		}
    };


    //!
    class CompilerOptions::Private
    {
    public:

        //! Detailed optimization options
        FModelOptimizationOptions OptimisationOptions;

        bool bIgnoreStates = false;
		int MinRomSize = 3;
		int MinTextureResidentMipCount = 3;

        int ImageCompressionQuality = 0;
		int32 ImageTiling=0 ;

        bool bLog = false;

		FImageOperator::FImagePixelFormatFunc ImageFormatFunc;
    };


    //! Information about an object state in the source data
    struct FObjectState
    {
        //! Name used to identify the state from the code and user interface.
        FString m_name;

        //! GPU Optimisation options
		FStateOptimizationOptions m_optimisation;

        //! List of names of the runtime parameters in this state
        TArray<FString> m_runtimeParams;

        void Serialise( OutputArchive& arch ) const
        {
            const int32 ver = 6;
            arch << ver;

            arch << m_name;
            arch << m_optimisation;
            arch << m_runtimeParams;
        }


        void Unserialise( InputArchive& arch )
        {
            int32 ver = 0;
            arch >> ver;
            check( ver>=5 && ver<=6 );

			if (ver <= 5)
			{
				std::string Temp;
				arch >> Temp;
				m_name = Temp.c_str();
			}
			else
			{
				arch >> m_name;
			}
            arch >> m_optimisation;

			if (ver <= 5)
			{
				TArray<std::string> Temp;
				arch >> Temp;
				m_runtimeParams.SetNum(Temp.Num());
				for ( int32 i=0; i<Temp.Num(); ++i)
				{
					m_runtimeParams[i] = Temp[i].c_str();
				}
			}
			else
			{
				arch >> m_runtimeParams;
			}
        }
    };


    //!
    struct FStateCompilationData
    {
        FObjectState nodeState;
        Ptr<ASTOp> root;
        FProgram::FState state;
		//FStateOptimizationOptions optimisationFlags;

        //! List of instructions that need to be cached to efficiently update this state
        TArray<Ptr<ASTOp>> m_updateCache;

        //! List of root instructions for the dynamic resources that depend on the runtime
        //! parameters of this state.
		TArray< TPair<Ptr<ASTOp>, TArray<FString> > > m_dynamicResources;
    };


    //!
    class Compiler::Private
    {
    public:

        Private()
        {
            m_pErrorLog = new ErrorLog();
        }

        Ptr<ErrorLog> m_pErrorLog;

        //! Detailed options
        Ptr<CompilerOptions> m_options;


		//!
		void GenerateRoms(Model* p, int32 MinRomSize);

    };

}
