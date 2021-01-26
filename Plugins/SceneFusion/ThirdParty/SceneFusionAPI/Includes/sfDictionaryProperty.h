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
#include <unordered_map>
#include "sfProperty.h"
#include "sfName.h"

namespace KS {
namespace SceneFusion2 {
    /**
     * A property with subproperty fields. Field keys are strings.
     */
    class EXTERNAL sfDictionaryProperty : 
        virtual public sfProperty
    {
    public:
        typedef std::shared_ptr<sfDictionaryProperty> SPtr;

        /**
         * Static shared pointer constructor
         *
         * @return  SPtr
         */
        static SPtr Create();

        /**
         * Destructor.
         */
        virtual ~sfDictionaryProperty() {};

        /**
         * Get the dictionary size.
         * 
         * @return int
         */
        virtual int Size() = 0;

        /**
         * Get a property mapped to a key.
         * 
         * @param   const sfName& - key.
         * @return  sfProperty::SPtr
         * @exception std::invalid_argument
         */
        virtual sfProperty::SPtr Get(const sfName& key) = 0;

        /**
         * Add a property to the dictionary. If the mapped key is used then the mapped
         * property will be replaced with the new property.
         * 
         * @param   const sfName& - key.
         * @param   sfProperty::SPtr - value.
         * @exception std::invalid_argument
         */
        virtual void Set(const sfName& key, sfProperty::SPtr value) = 0;

        /**
         * Try to get a property mapped to a key, if the key is not found then return false without modifying
         * the value reference.
         * 
         * @param   const sfName& - key
         * @param   out sfProperty::SPtr& value of field, or null if the field was not found.
         * @return  bool true if the field was found.
         */
        virtual bool TryGet(const sfName& key, sfProperty::SPtr& value) = 0;

        /**
         * Checks if mapping exists for a key.
         * 
         * @param   const sfName& - key
         * @return  bool true if the field exists.
         */
        virtual bool HasKey(const sfName& key) = 0;

        /**
         * Removes a property mapped to a key.
         * 
         * @param   const sfName& - key
         * @return  bool true if the field was removed.
         */
        virtual bool Remove(const sfName& key) = 0;

        typedef typename std::unordered_map<sfName, sfProperty::SPtr>::const_iterator const_iterator;

        /**
         * @return  const_iterator pointing to the first pair in the dictionary.
         */
        virtual const_iterator begin() const = 0;

        /**
         * @return  const_iterator pointing past the last pair in the dictionary.
         */
        virtual const_iterator end() const = 0;
    };
} // SceneFusion2
} // KS