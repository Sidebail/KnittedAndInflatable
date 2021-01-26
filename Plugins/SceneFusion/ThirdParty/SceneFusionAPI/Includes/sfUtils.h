/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2019) KinematicSoup Technologies Incorporated
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

#include "sfDictionaryProperty.h"
#include "sfListProperty.h"
#include "sfValueProperty.h"
#include "sfReferenceProperty.h"
#include "sfNullProperty.h"
#include "sfObject.h"
#include "sfSession.h"

namespace KS
{
namespace SceneFusion2
{
    /**
     * Utility functions.
     */
    class EXTERNAL sfUtils
    {
    public:
        /**
         * Computes the size of a property.
         * 
         * @param   sfProperty::SPtr propPtr to compute size of.
         * @return  size_t size of the property.
         */
        static size_t SizeOf(sfProperty::SPtr propPtr);

        /**
         * Computes the size of an object.
         *
         * @param   sfObject::SPtr objPtr to compute size of.
         * @param   bool recurseChildren - if true, will include the size of all descendants of the object.
         * @return  size_t size of the object.
         */
        static size_t SizeOf(sfObject::SPtr objPtr, bool recurseChildren = true);

        /**
         * Computes the size of all strings in the string table.
         *
         * @param   sfSession::SPtr sessionPtr to compute string table size for.
         * @return  size_t size of the string table.
         */
        static size_t SizeOfStringTable(sfSession::SPtr sessionPtr);

        /**
         * Computes the size of all objects in the session and the string table.
         *
         * @param   sfSession::SPtr sessionPtr to compute size for.
         * @return  size_t size of all objects and the string table.
         */
        static size_t SizeOf(sfSession::SPtr sessionPtr);
    };
}//SceneFusion
}//KS
