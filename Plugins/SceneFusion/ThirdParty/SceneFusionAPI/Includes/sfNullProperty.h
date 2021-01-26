/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once
#include "sfProperty.h"

namespace KS{
namespace SceneFusion2{
    /**
     * A property that represents a null value.
     */
    class EXTERNAL sfNullProperty : 
        virtual public sfProperty
    {
    public:
        typedef std::shared_ptr<sfNullProperty> SPtr;

        /**
         * Destructor.
         */
        virtual ~sfNullProperty() {};

        /**
         * Static shared pointer constructor
         *
         * @return  SPtr
         */
        static SPtr Create();
    };
} // SceneFusion2
} // KS