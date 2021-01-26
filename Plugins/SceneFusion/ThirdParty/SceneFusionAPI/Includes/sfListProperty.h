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
#include <vector>
#include "sfProperty.h"

namespace KS{
namespace SceneFusion2{
    /**
     * A property that is a list of properties.
     */
    class EXTERNAL sfListProperty : 
        virtual public sfProperty
    {
    public:
        typedef std::shared_ptr<sfListProperty> SPtr;

        /**
         * Static shared pointer constructor.
         *
         * @return  SPtr
         */
        static SPtr Create();

        /**
         * Destructor.
         */
        virtual ~sfListProperty() {};

        /**
         * Get the size of the list.
         * 
         * @return  int size
         */
        virtual int Size() = 0;

        /**
         * Resize the list. If the new size is less than the current size then
         * values at the end of the list will be removed. If the new size if greater
         * than the current size then the extra indices will be filled with sfNullProperty.
         *
         * @param   int - size
         * @exception std::out_of_range
         */
        virtual void Resize(int size) = 0;

        /**
         * Get the property at an index. 
         *
         * @param   int - index
         * @return  sfProperty::SPtr
         * @exception std::out_of_range
         */
        virtual sfProperty::SPtr Get(int index) = 0;

        /**
         * Set the property at an index. 
         *
         * @param   int - index
         * @param   sfProperty::SPtr - property
         * @exception std::out_of_range
         */
        virtual void Set(int index, sfProperty::SPtr property) = 0;

        /**
         * Add a property to the end of the list.
         * 
         * @param   sfProperty::SPtr - property
         */
        virtual void Add(sfProperty::SPtr property) = 0;

        /**
         * Add a list of properties to the end of the list. 
         * 
         * @param   std::vector<sfProperty::SPtr>& - property list
         */
        virtual void AddRange(std::vector<sfProperty::SPtr>& properties) = 0;

        /**
         * Insert a property into the list.
         *
         * @param   int - index
         * @param   sfProperty::SPtr - property
         * @exception std::out_of_range
         */
        virtual void Insert(int index, sfProperty::SPtr property) = 0;

        /**
         * Inserts a list properties into the list. 
         * 
         * @param   int - index
         * @param   std::vector<sfProperty::SPtr>& - properties list
         *
         * @exception std::out_of_range
         */
        virtual void InsertRange(int index, std::vector<sfProperty::SPtr>& properties) = 0;

        /**
         * Removes a property at an index. 
         *
         * @param   int - index
         *
         * @exception std::out_of_range
         */
        virtual void Remove(int index) = 0;

        /**
         * Remove multiple properties starting from an index.
         * 
         * @param   int - index
         * @param   int - number of properties to remove
         *
         * @exception std::out_of_range
         */
        virtual void RemoveRange(int index, int count) = 0;

        typedef typename std::vector<sfProperty::SPtr>::const_iterator const_iterator;

        /**
         * @return  const_iterator pointing to the first element in the list.
         */
        virtual const_iterator begin() const = 0;

        /**
         * @return  const_iterator pointing past the last element in the list.
         */
        virtual const_iterator end() const = 0;
    };
} // SceneFusion2
} // KS