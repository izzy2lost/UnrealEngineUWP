// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"


namespace Chaos {

	class CHAOSFLESH_API FFleshCollectionFacade
	{
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;
	public:

		FFleshCollectionFacade(FManagedArrayCollection& InCollection);

		FFleshCollectionFacade(const FManagedArrayCollection& InCollection);
		
		/*Are all the public attributes avaiable.*/
		bool IsCompletelyValid() const;
	
		/*Has tetrahedral attributes {Tetrahderon, Vertices}.*/
		bool IsTetrahedronValid() const;

		/*Has hierarchy attributes {Transform, Parent}.*/
		bool IsHierarchyValid() const;

		/*View a Attribute on the collection*/
		template<class T>
		const TManagedArray<T>* FindAttribute(FString AttributeName, FString Group) const;

		/*Edit a Attribute on the collection*/
		template<class T>
		TManagedArray<T>* ModifyAttribute(FString AttributeName, FString Group);

		/* All the vertices mapped into component space. */
		void ComponentSpaceVertices(TArray<FVector3f>& OutComponentSpaceVertices);

		/* Range of vertices mapped into component space*/
		void ComponentSpaceVertices(TArray<FVector3f>& OutComponentSpaceVertices, int32 Start, int32 Count);

		/*Public Attributes*/
		TManagedArrayAccessor<FString> BoneName;
		TManagedArrayAccessor<FTransform3f> Transform;
		TManagedArrayAccessor<int32> Parent;
		TManagedArrayAccessor< TSet<int32> > Child;
		TManagedArrayAccessor<int32> BoneMap;
		TManagedArrayAccessor<FVector3f> Vertex;
		TManagedArrayAccessor<FIntVector3> Indices;
		TManagedArrayAccessor<FIntVector4> Tetrahedron;
		TManagedArrayAccessor<int32> VertexStart;
		TManagedArrayAccessor<int32> VertexCount;
		TManagedArrayAccessor<int32> FaceStart;
		TManagedArrayAccessor<int32> FaceCount;
	};

}
