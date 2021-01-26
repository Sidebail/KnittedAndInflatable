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
#include <stack>
#include "sfProperty.h"

namespace KS {
namespace SceneFusion2 {
    // Forward declarations
    class PropertyIteratorInfo;

    /**
     * Iterator for all properties.
     */
    class EXTERNAL sfPropertyIterator
    {
    public:
        /**
         * Constructor
         *
         * @param   sfProperty::SPtr
         */
        sfPropertyIterator(sfProperty::SPtr propertyPtr);

        /**
         * Moves to next property.
         *
         * @return  bool - true if the iterator is pointing to a valid property
         */
        bool Next();

        /**
         * Gets the property this iterator currently point to.
         *
         * @return  sfProperty::SPtr
         */
        sfProperty::SPtr Value();
    private:
        sfProperty::SPtr m_current;
        std::stack<std::shared_ptr<PropertyIteratorInfo>> m_stack;
    };
} // SceneFusion2
} // KS