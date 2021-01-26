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
#include <ksMultiType.h>
#include "sfProperty.h"

namespace KS {
namespace SceneFusion2 {
    /**
     * A value property. Values are stored as ksMultiTypes.
     */
    class EXTERNAL sfValueProperty : 
        virtual public sfProperty
    {
    public:
        typedef std::shared_ptr<sfValueProperty> SPtr;

        /**
         * Static shared pointer constructor
         *
         * @param   ksMultiType& value
         * @return  SPtr
         */
        static SPtr Create(ksMultiType& value);

        /**
         * Static shared pointer constructor
         *
         * @param   ksMultiType&& value
         * @return  SPtr
         */
        static SPtr Create(ksMultiType&& value);

        /**
         * Destructor
         */
        virtual ~sfValueProperty() {};

        /**
         * Gets property value.
         * 
         * @return const ksMultiType&
         */
        virtual const ksMultiType& GetValue() const = 0;

        /**
         * Sets property value.
         * 
         * @param   const ksMultiType& - value
         */
        virtual void SetValue(const ksMultiType& value) = 0;

        /**
         * Sets property value.
         * 
         * @param   ksMultiType&& - value
         */
        virtual void SetValue(ksMultiType&& value) = 0;
    };
} // SceneFusion2
} // KS