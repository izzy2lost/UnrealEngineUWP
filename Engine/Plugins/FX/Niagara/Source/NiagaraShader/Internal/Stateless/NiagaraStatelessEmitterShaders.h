// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraStatelessSimulationShader.h"

#include "NiagaraStatelessModuleShaderParameters.h"

#include "RenderGraphFwd.h"
#include "ShaderParameterStruct.h"

namespace NiagaraStateless
{
	class FSimulationShaderDefaultCS : public FSimulationShader
	{
	public:
		DECLARE_EXPORTED_GLOBAL_SHADER(FSimulationShaderDefaultCS, NIAGARASHADER_API);
		SHADER_USE_PARAMETER_STRUCT(FSimulationShaderDefaultCS, FSimulationShader);

		//BEGIN_SHADER_PARAMETER_STRUCT(FOutputParameters, NIAGARASHADER_API)
		//	SHADER_PARAMETER(int,	Output_PositionComponent)
		//	SHADER_PARAMETER(int,	Output_ColorComponent)
		//	SHADER_PARAMETER(int,	Output_SpriteSizeComponent)
		//	SHADER_PARAMETER(int,	Output_PreviousPositionComponent)
		//	SHADER_PARAMETER(int,	Output_PreviousSpriteSizeComponent)
		//END_SHADER_PARAMETER_STRUCT()

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, NIAGARASHADER_API)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonShaderParameters,							CommonParameters)
			SHADER_PARAMETER_STRUCT_INCLUDE(FInitializeParticleModule_ShaderParameters,			Module0Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FInitialMeshOrientationModule_ShaderParameters,		Module1Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FShapeLocationModule_ShaderParameters,				Module2Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleColorModule_ShaderParameters,					Module3Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleSpriteSizeModule_ShaderParameters,			Module4Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleSpriteSizeBySpeedModule_ShaderParameters,		Module5Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleMeshSizeModule_ShaderParameters,				Module6Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleMeshSizeBySpeedModule_ShaderParameters,		Module7Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FMeshRotationRateModule_ShaderParameters,			Module8Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FSolveVelocitiesAndForcesModule_ShaderParameters,	Module9Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FSpriteFacingAndAlignmentModule_ShaderParameters,	Module10Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FSpriteRotationRateModule_ShaderParameters,			Module11Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FSubUVAnimationModule_ShaderParameters,				Module12Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FDynamicMaterialParametersModule_ShaderParameters,	Module13Params)

			SHADER_PARAMETER(int, Permutation_UniqueIDComponent)
			SHADER_PARAMETER(int, Permutation_PositionComponent)
			SHADER_PARAMETER(int, Permutation_ColorComponent)
			SHADER_PARAMETER(int, Permutation_DynamicMaterialParameter0Component)
			SHADER_PARAMETER(int, Permutation_MeshOrientationComponent)
			SHADER_PARAMETER(int, Permutation_RibbonWidthComponent)
			SHADER_PARAMETER(int, Permutation_ScaleComponent)
			SHADER_PARAMETER(int, Permutation_SpriteSizeComponent)
			SHADER_PARAMETER(int, Permutation_SpriteFacingComponent)
			SHADER_PARAMETER(int, Permutation_SpriteAlignmentComponent)
			SHADER_PARAMETER(int, Permutation_SpriteRotationComponent)
			SHADER_PARAMETER(int, Permutation_SubImageIndexComponent)
			SHADER_PARAMETER(int, Permutation_VelocityComponent)
			SHADER_PARAMETER(int, Permutation_PreviousPositionComponent)
			SHADER_PARAMETER(int, Permutation_PreviousMeshOrientationComponent)
			SHADER_PARAMETER(int, Permutation_PreviousRibbonWidthComponent)
			SHADER_PARAMETER(int, Permutation_PreviousScaleComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteSizeComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteFacingComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteAlignmentComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteRotationComponent)
			SHADER_PARAMETER(int, Permutation_PreviousVelocityComponent)
		END_SHADER_PARAMETER_STRUCT()
	};

	class FSimulationShaderExample1CS : public FSimulationShader
	{
	public:
		DECLARE_EXPORTED_GLOBAL_SHADER(FSimulationShaderExample1CS, NIAGARASHADER_API);
		SHADER_USE_PARAMETER_STRUCT(FSimulationShaderExample1CS, FSimulationShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, NIAGARASHADER_API)
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonShaderParameters, CommonParameters)
			SHADER_PARAMETER_STRUCT_INCLUDE(FInitializeParticleModule_ShaderParameters,			Module0Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FShapeLocationModule_ShaderParameters,				Module1Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleColorModule_ShaderParameters,					Module2Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FScaleSpriteSizeModule_ShaderParameters,			Module3Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FRotateAroundPointModule_ShaderParameters,			Module4Params)
			SHADER_PARAMETER_STRUCT_INCLUDE(FSolveVelocitiesAndForcesModule_ShaderParameters,	Module5Params)

			SHADER_PARAMETER(int, Permutation_UniqueIDComponent)
			SHADER_PARAMETER(int, Permutation_PositionComponent)
			SHADER_PARAMETER(int, Permutation_ColorComponent)
			SHADER_PARAMETER(int, Permutation_ScaleComponent)
			SHADER_PARAMETER(int, Permutation_RibbonWidthComponent)
			SHADER_PARAMETER(int, Permutation_SpriteSizeComponent)
			SHADER_PARAMETER(int, Permutation_SpriteRotationComponent)
			SHADER_PARAMETER(int, Permutation_VelocityComponent)
			SHADER_PARAMETER(int, Permutation_PreviousPositionComponent)
			SHADER_PARAMETER(int, Permutation_PreviousRibbonWidthComponent)
			SHADER_PARAMETER(int, Permutation_PreviousScaleComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteSizeComponent)
			SHADER_PARAMETER(int, Permutation_PreviousSpriteRotationComponent)
			SHADER_PARAMETER(int, Permutation_PreviousVelocityComponent)
		END_SHADER_PARAMETER_STRUCT()
	};
}
