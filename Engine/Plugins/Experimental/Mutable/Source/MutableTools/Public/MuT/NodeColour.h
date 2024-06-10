// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;


    //! %Base class of any node that outputs a colour.
    //! \ingroup model
    class MUTABLETOOLS_API NodeColour : public Node
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Node Interface
        //-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
        static const FNodeType* GetStaticType();


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        inline ~NodeColour() {}
    };

}

