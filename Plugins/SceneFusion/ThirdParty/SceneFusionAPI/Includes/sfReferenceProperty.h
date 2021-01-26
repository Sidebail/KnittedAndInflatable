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

namespace KS {
namespace SceneFusion2 {
    
    /**
     * A property that references an sfObject by storing an object id. All references to an object can be retrieved
     * from the session using sfSession.GetReferences(sfObject).
     */
    class EXTERNAL sfReferenceProperty :
        virtual public sfProperty
    {
    public:
        typedef std::shared_ptr<sfReferenceProperty> SPtr;

        /**
         * Static shared pointer constructor.
         *
         * @param   uint32_t objectId of referenced object.
         * @return  SPtr
         */
        static SPtr Create(uint32_t objectId);

        /**
         * Deconstructor
         */
        virtual ~sfReferenceProperty() {};

        /**
         * @return uint32_t id of referenced object.
         */
        virtual uint32_t GetObjectId() = 0;

        /**
         * Sets the referenced object id.
         *
         * @param   uint32_t objectId
         */
        virtual void SetObjectId(uint32_t objectId) = 0;
    };
} // SceneFusion2
} // KS