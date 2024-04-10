// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/AST.h"
#include "MuR/Skeleton.h"

namespace mu
{


	class NodeMeshReshape::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeMesh> BaseMesh;
		Ptr<NodeMesh> BaseShape;
		Ptr<NodeMesh> TargetShape;
		bool bReshapeVertices = true;
		bool bApplyLaplacian = false;
		bool bReshapeSkeleton = false;
		bool bReshapePhysicsVolumes = false;

		EVertexColorUsage ColorRChannelUsage = EVertexColorUsage::None;
		EVertexColorUsage ColorGChannelUsage = EVertexColorUsage::None;
		EVertexColorUsage ColorBChannelUsage = EVertexColorUsage::None;
		EVertexColorUsage ColorAChannelUsage = EVertexColorUsage::None;

		TArray<FBoneName> BonesToDeform;
		TArray<FBoneName> PhysicsToDeform;
        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 12;
			arch << ver;

			arch << BaseMesh;
			arch << BaseShape;
			arch << TargetShape;

			arch << bReshapeVertices;
			arch << bApplyLaplacian;
			
			arch << bReshapeSkeleton;
			arch << BonesToDeform;

			arch << bReshapePhysicsVolumes;
			arch << PhysicsToDeform;

			
			arch << ColorRChannelUsage;
			arch << ColorGChannelUsage;
			arch << ColorBChannelUsage;
			arch << ColorAChannelUsage;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver <= 11);

			arch >> BaseMesh;
			arch >> BaseShape;
			arch >> TargetShape;
			
			if (ver >= 8)
			{
				arch >> bReshapeVertices;
			}

			if (ver >= 11)
			{
				arch >> bApplyLaplacian;
			}

			arch >> bReshapeSkeleton;
			if (ver <= 9)
			{
				bool bEnableRigidParts_DEPRECATED;
				arch >> bEnableRigidParts_DEPRECATED;

				if (bEnableRigidParts_DEPRECATED)
				{
					ColorRChannelUsage = ColorGChannelUsage = ColorBChannelUsage = ColorAChannelUsage = EVertexColorUsage::ReshapeClusterId;
				}
			}

			if (ver <= 5)
			{
				bool bDeformAllBones_DEPRECATED;
				arch >> bDeformAllBones_DEPRECATED;
			}

			if (ver >= 12)
			{
				arch >> BonesToDeform;
			}
			else
			{
				int32 NumBonesToDeform = 0;
				if (ver >= 9)
				{
					TArray<uint16> BonesToDeform_DEPRECATED;
					arch >> BonesToDeform_DEPRECATED;
					NumBonesToDeform = BonesToDeform_DEPRECATED.Num();
				}
				else
				{
					TArray<string> BonesToDeform_DEPRECATED;
					arch >> BonesToDeform_DEPRECATED;
					NumBonesToDeform = BonesToDeform_DEPRECATED.Num();
				}

				BonesToDeform.SetNum(NumBonesToDeform);
				for (int32 BoneIndex = 0; BoneIndex < NumBonesToDeform; ++BoneIndex)
				{
					BonesToDeform[BoneIndex].Id = BoneIndex;
				}
			}

		  	if (ver >= 4)
		 	{
				arch >> bReshapePhysicsVolumes;
		 	}

			if (ver >= 5 && ver < 7)
			{
				bool bDeformAllPhysics_DEPRECATED;
				arch >> bDeformAllPhysics_DEPRECATED;
			}

			if (ver >= 12)
			{
				arch >> PhysicsToDeform;
			}
			else
			{
				int32 NumPhysicsToDeform = 0;
				if (ver >= 9)
				{
					TArray<uint16> PhysicsToDeform_DEPRECATED;
					arch >> PhysicsToDeform_DEPRECATED;
					NumPhysicsToDeform = PhysicsToDeform_DEPRECATED.Num();
				}
				else if (ver >= 5)
				{
					TArray<string> PhysicsToDeform_DEPRECATED;
					arch >> PhysicsToDeform_DEPRECATED;
					NumPhysicsToDeform = PhysicsToDeform_DEPRECATED.Num();
				}

				PhysicsToDeform.SetNum(NumPhysicsToDeform);
				for (int32 BoneIndex = 0; BoneIndex < NumPhysicsToDeform; ++BoneIndex)
				{
					PhysicsToDeform[BoneIndex].Id = BoneIndex;
				}
			}

			if (ver >= 10)
			{
				arch >> ColorRChannelUsage;
				arch >> ColorGChannelUsage;
				arch >> ColorBChannelUsage;
				arch >> ColorAChannelUsage;
			}
		}

		// NodeMesh::Private interface
        Ptr<NodeLayout> GetLayout( int index ) const override;

	};

}
