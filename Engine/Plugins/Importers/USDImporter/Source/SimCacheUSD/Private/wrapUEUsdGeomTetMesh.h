// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// TODO: Figure out whether we want/need python bindings, and what the right way to do them is.
#if 0//USE_USD_SDK

// python bindings:
#include "pxr/usd/usd/pyConversions.h"
#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyResultsConversions.h"
#include "pxr/base/tf/pyUtils.h"
#include "pxr/base/tf/wrapTypeHelpers.h"
#include <boost/python.hpp>

PXR_NAMESPACE_OPEN_SCOPE

#define WRAP_CUSTOM
	template <class Cls> static void _CustomWrapCode(Cls& _class)

		// fwd decl.
		WRAP_CUSTOM;

	static UsdAttribute
		_CreateTetVertexIndicesAttr(
			UEUsdGeomTetMesh& self,
			object defaultVal, bool writeSparsely)
	{
		return self.CreateTetVertexIndicesAttr(
			UsdPythonToSdfType(defaultVal, SdfValueTypeNames->Int4Array), writeSparsely);
	}

	void wrapUEUsdGeomTetMesh()
	{
		typedef UEUsdGeomTetMesh This;

		class_<This, boost::python::bases<UsdGeomMesh> >
			cls("TetMesh");
		cls
			.def(init<UsdPrim>(arg("prim")))
			.def(init<UsdSchemaBase const&>(arg("schemaObj")))
			.def(TfTypePythonClass())

			.def("Get", &This::Get, (arg("stage"), arg("path")))
			.staticmethod("Get")

			.def("GetSchemaAttributeNames",
				&This::GetSchemaAttributeNames,
				arg("includeInherited") = true,
				boost::python::return_value_policy<TfPySequenceToList>())
			.staticmethod("GetSchemaAttributeNames")

			.def("_GetStaticTfType", &TfType const& (*)()) TfType::Find<This>,
			boost::python::return_value_policy<boost::python::return_by_value>())
			.staticmethod("_GetStaticTfType")

			.def(!self)

			.def("GetTetVertexIndicesAttr",
				&This::GetTetVertexIndicesAttr)
			.def("CreateTetVertexIndicesAttr",
				&_CreateTetVertexIndicesAttr,
				(boost::python::arg("defaultValue") = object(),
					boost::python::arg("writeSparsely") = false))
			;

		_CustomWrapCode(cls);
	}

}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USE_USD_SDK
