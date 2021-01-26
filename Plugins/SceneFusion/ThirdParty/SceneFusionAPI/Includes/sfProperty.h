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
#include <memory>
#include <string>
#include <sstream>
#include <Exports.h>

#include "sfName.h"

namespace KS {
namespace SceneFusion2 {
    // Forward declarations
    class sfPropertyIterator;
    class sfValueProperty;
    class sfListProperty;
    class sfDictionaryProperty;
    class sfReferenceProperty;
    class sfNullProperty;
    class sfObject;


    /**
     * Scene Fusion property
     */
    class EXTERNAL sfProperty :
        public std::enable_shared_from_this<sfProperty>
    {
    public:
        typedef std::shared_ptr<sfProperty> SPtr;

        /**
         * Property types
         */
        enum Types
        {
            VALUE = 0,
            DICTIONARY = 1,
            LIST = 2,
            NUL = 3,
            REFERENCE = 4
        };

        /**
         * Destructor.
         */
        virtual ~sfProperty() {};

        /**
         * Cast this instance to an sfValueProperty shared pointer.
         *
         * @return  std::shared_ptr<sfValueProperty>
         */
        std::shared_ptr<sfValueProperty> AsValue();

        /**
         * Cast this instance to an sfListProperty shared pointer.
         *
         * @return  std::shared_ptr<sfListProperty>
         */
        std::shared_ptr<sfListProperty> AsList();

        /**
         * Cast this instance to an sfDictionaryProperty shared pointer.
         *
         * @return  std::shared_ptr<sfDictionaryProperty>
         */
        std::shared_ptr<sfDictionaryProperty> AsDict();

        /**
         * Cast this instance to an sfReferenceProperty shared pointer.
         *
         * @return  std::shared_ptr<sfReferenceProperty>
         */
        std::shared_ptr<sfReferenceProperty> AsReference();

        /**
         * Cast this instance to an sfNullProperty shared pointer.
         *
         * @return  std::shared_ptr<sfNullProperty>
         */
        std::shared_ptr<sfNullProperty> AsNull();

        /**
         * String representation of this object.
         *
         * @return  std::string
         */
        virtual std::string ToString() = 0;

        /**
         * Get the property type.
         *
         * @return  Types
         */
        virtual Types Type() = 0;

        /**
         * Get the parent property.
         *
         * @return sfProperty::SPtr
         */
        virtual sfProperty::SPtr GetParentProperty() = 0;
        
        /**
         * Get the sfObject containing this property.
         *
         * @return  std::shared_ptr<sfObject> object containing this property, or nullptr if the property is not in an
         *          object.
         */
        virtual std::shared_ptr<sfObject> GetContainerObject() = 0;

        /**
         * Can we edit the property? False if we cannot edit the object the property belongs to.
         *
         * @return  bool
         */
        virtual bool CanEdit() = 0;

        /**
         * Property key. 
         * If this property has a parent and the parent is a sfDictionaryProperty then this function
         * returns the key used to store this property, otherwise it will return an empty string.
         *
         * @return  const sfName&
         */
        virtual const sfName& Key() = 0;

        /**
         * Property index. 
         * If this property has a parent and the parent is a sfListProperty then this function
         * returns the index used to store this property, otherwise it will return -1.
         *
         * @return  int32_t
         */
        virtual int32_t Index() = 0;

        /**
         * Property path. Constructs a string describing the parenting of this property. 
         * If the property has no parent than an empty string is returned.
         * If the property has a dictionary parent then a key value is written preceeded by a period if the parent has its own parents.
         * If the property has a list parent then an index value is written enclosed by brackets.
         *
         * Examples:
         *   If the parent is null return ""
         *   If the parent is a dictionary then return "key"
         *   If the parent is a list then return "[index]"
         *   If the parent is a dictionary with a list parent then return "[index].key"
         *
         * @return  std::string 
         */
        virtual std::string GetPath() = 0;

        /**
         * Depth of the property.
         *
         * @return  int
         */
        virtual int GetDepth() = 0;

        /**
         * Return an iterator for this property and all sub properties.
         *
         * @return  sfPropertyIterator
         */
        virtual sfPropertyIterator Iterate() = 0;

        /**
         * Copy this property and all subproperties.
         *
         * @return  sfProperty::SPtr
         */
        virtual sfProperty::SPtr Clone() = 0;

        /**
         * Performs a comparison of a property with another property. If a property has subproperties
         * (sfListProperty, sfDictionaryProperty, etc) then all subproperties are checked as well.
         *
         * @param   sfProperty::SPtr - property to compare
         * @return  bool - true if the properties have the same values
         */
        virtual bool Equals(sfProperty::SPtr other) = 0;
    };
} // SceneFusion2
} // KS