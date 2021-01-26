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

#include <functional>
#include <list>
#include <memory>
#include <stdexcept>
#include <stack>

namespace KS {
    /**
     * Templated base class for hierarchy objects that have a parent and a list of children of type T.
     */
    template <typename T>
    class ksHierarchyObject : public std::enable_shared_from_this<T>
    {
    public:
        /**
         * Callback for depth-first searches.
         *
         * @param   std::shared_ptr<T> - pointer to the object being iterated.
         * @return  bool - true to iterate the children of object.
         */
        typedef std::function<bool(std::shared_ptr<T>)> ForEachCallback;

        /**
         * Constructor
         */
        ksHierarchyObject()
        {
        }

        /**
         * Destructor
         */
        virtual ~ksHierarchyObject()
        {
        }

        /**
         * Get a pointer to this object's parent object
         *
         * @return  std::shared_ptr<T>
         */
        std::shared_ptr<T> Parent()
        {
            return m_parentPtr.lock();
        }

        /**
         * Return a reference to this object's children list
         *
         * @return  const std::list<std::shared_ptr<T>>&
         */
        const std::list<std::shared_ptr<T>>& Children()
        {
            return m_children;
        }
        
        /**
         * Return a pointer to a child object at a specific index. If the index is out of bounds
         * then a null pointer is returned.
         *
         * @param   size_t - child index
         * @return  std::shared_ptr<T>
         */
        std::shared_ptr<T> Child(size_t index)
        {
            if (m_children.size() <= index) {
                return nullptr;
            }
            auto iter = std::next(m_children.begin(), index);
            return *iter;
        }

        /**
         * Get the index of a child in this object.  Return -1 if the child is not in the list of children
         *
         * @param   std::shared_ptr<T> - pointer to the child object
         * @return  int - index of the child, -1 if the child was not found
         */
        int IndexOfChild(std::shared_ptr<T> childPtr)
        {
            auto iter = m_children.begin();
            int index = 0;
            while (iter != m_children.end()) {
                if (*iter == childPtr) {
                    return index;
                }
                index++;
                iter++;
            }
            return -1;
        }

        /**
         * Removes the object from its parent. Does nothing if the object has no parent.
         */
        void Detach()
        {
            std::shared_ptr<T> parentPtr = Parent();
            if (parentPtr != nullptr)
            {
                parentPtr->RemoveChild(std::enable_shared_from_this<T>::shared_from_this());
            }
        }

        /**
         * Checks if an object is a descendant of this object.
         *
         * @param   std::shared_ptr<T> - pointer to the object to check.
         * @return  bool - true if obj is a descendant of this object.
         */
        bool IsDescendantOf(std::shared_ptr<T> objPtr)
        {
            if (objPtr == nullptr)
            {
                return false;
            }

            std::shared_ptr<T> parentPtr = Parent();
            while (parentPtr != nullptr) {
                if (objPtr == parentPtr)
                {
                    return true;
                }
                parentPtr = parentPtr->Parent();
            }
            return false;
        }

        /**
         * Adds a child to the object if that does not create a circular reference. If the child has another parent,
         * removes it from its parent first. Throws std::invalid_argument exception if the child is null.
         *
         * @param   std::shared_ptr<T> - pointer to the child to add.
         * @return  bool - true if the child was added. False if it could not be added, either because it was already
         *          added or adding it would create a circular reference.
         */
        virtual bool AddChild(std::shared_ptr<T> childPtr)
        {
            return PerformAddChild(childPtr);
        }

        /**
         * Inserts a child at an index if that does not create a circular reference. If the child has another parent,
         * removes it from its parent first. Throws std::invalid_argument exception if the child is null.
         *
         * @param   int - index to insert at.
         * @param   std::shared_ptr<T> - child to insert.
         * @return  bool - true if the child was inserted. False if it could not be inserted, either because it was
         *          already added, adding it would create a circular reference, or the index was out of bounds.
         */
        virtual bool InsertChild(int index, std::shared_ptr<T> childPtr)
        {
            return PerformInsertChild(index, childPtr);
        }

        /**
         * Removes a child from this object. Throws std::invalid_argument exception if child is null.
         *
         * @param   std::shared_ptr<T> - child to remove.
         * @return  bool - true if the child was found and removed.
         */
        virtual bool RemoveChild(std::shared_ptr<T> childPtr)
        {
            return PerformRemoveChild(childPtr);
        }

        /**
         * Iterates the descendants using depth-first search.
         *
         * @param   ForEachCallback - callback to call on descendants. If it returns false, will not iterate children.
         */
        void ForEachDescendant(ForEachCallback callback)
        {
            for (auto childPtr : m_children)
            {
                if (callback(childPtr))
                {
                    childPtr->ForEachDescendant(callback);
                }
            }
        }
        
        /**
         * Iterates this object and its descendants using depth-first search.
         *
         * @param   ForEachCallback - callback to call on descendants. If it returns false, will not iterate children.
         */
        void ForSelfAndDescendants(ForEachCallback callback)
        {
            if (callback(std::enable_shared_from_this<T>::shared_from_this()))
            {
                ForEachDescendant(callback);
            }
        }

        /** 
         * Ancestor Iterator.  Traverses parents until the root of the tree is reached. A null result indicates 
         * the end of iteration.
         */
        template <typename A>
        class AncestorIter
        {
        public:
            /**
             * Constructor
             *
             * @param   A - first iteration object
             */
            AncestorIter(A obj) : 
                m_current{ obj }
            {
            }

            /**
             * Advance the iterator
             *
             * @return  bool - false if we have reached the end of the iteration
             */
            bool Next() 
            { 
                if (m_current != nullptr) {
                    m_current = m_current->Parent();
                }
                return (m_current!= nullptr);
            }

            /**
             * Equals comparison
             * 
             * @param   const AncestorIter<A>& - other iterator
             */
            bool operator==(const AncestorIter<A>& other) const 
            { 
                return m_current == other.m_current;
            }

            /**
             * Not-equals comparison
             *
             * @param   const AncestorIter<A>& - other iterator
             */
            bool operator!=(const AncestorIter<A>& other) const
            { 
                return m_current != other.m_current;
            }

            /**
             * Current iteration object.
             *
             * @param   A - current object
             */
            A Value() 
            { 
                return m_current;
            }
        private:
            A m_current;
        };

        /**
         * Get an ancestor iterator that includes the current object.
         *
         * @return  AncestorIter<std::shared_ptr<T>>
         */
        AncestorIter<std::shared_ptr<T>> SelfAndAncestors()
        {
            return AncestorIter<std::shared_ptr<T>>{ std::enable_shared_from_this<T>::shared_from_this() };
        }

        /**
         * Get an ancestor iterator that does not include the current object.
         *
         * @return  AncestorIter<std::shared_ptr<T>>
         */
        AncestorIter<std::shared_ptr<T>> Ancestors()
        {
            return AncestorIter<std::shared_ptr<T>>{ m_parentPtr.lock() };
        }

        /**
         * Descendant Iterator.  Traverses children depth first until the sub tree of this object has been traversed.
         * A null result indicates the end of iteration.
         */
        template <typename D>
        class DescendantIter
        {
        public:
            /**
             * Constructor
             *
             * @param   D -  first iteration object
             */
            DescendantIter(D obj) : 
                m_current{ obj }
            {
            }

            /**
             * Advance the iterator
             *
             * @return  bool - false if we have reached the end of the iteration
             */
            bool Next()
            {
                if (m_current != nullptr) {
                    if (m_current->Children().size() > 0)
                    {
                        typename std::list<D>::const_iterator iter = m_current->Children().cbegin();
                        m_stack.push(std::make_pair(m_current, iter));
                        m_current = *(m_stack.top().second);
                        return m_current != nullptr;
                    }
                    else {
                        while (m_stack.size() > 0) {
                            m_stack.top().second++;
                            if (m_stack.top().second != m_stack.top().first->Children().cend())
                            {
                                m_current = *(m_stack.top().second);
                                return m_current != nullptr;
                            }
                            else {
                                m_stack.pop();
                            }
                        }
                        m_current = nullptr;
                    }
                }
                return m_current != nullptr;
            }

            /**
             * Equals comparison
             *
             * @param   const DescendantIter<D>& - other iterator
             */
            bool operator==(const DescendantIter<D>& other) const
            {
                return m_current == other.m_current && m_stack == other.m_stack;
            }

            /**
             * Not-equals comparison
             *
             * @param   const DescendantIter<D>& - other iterator
             */
            bool operator!=(const DescendantIter<D>& other) const
            {
                return m_current != other.m_current || m_stack != other.m_stack;
            }

            /**
             * Current iteration object.
             *
             * @param   D - current object
             */
            D Value()
            {
                return m_current;
            }
        private:
            std::stack<std::pair<D, typename std::list<D>::const_iterator>> m_stack;
            D m_current;
        };

        /**
         * Get an descendant iterator that includes the current object.
         *
         * @return  DescendantIter<std::shared_ptr<T>>
         */
        DescendantIter<std::shared_ptr<T>> SelfAndDescendants()
        {
            return DescendantIter<std::shared_ptr<T>>{ std::enable_shared_from_this<T>::shared_from_this() };
        }

        /**
         * Get an descendant iterator that does not include the current object.
         *
         * @return  DescendantIter<std::shared_ptr<T>>
         */
        DescendantIter<std::shared_ptr<T>> Descendants()
        {
            auto iter = DescendantIter<std::shared_ptr<T>>{ std::enable_shared_from_this<T>::shared_from_this() };
            iter.Next();
            return iter;
        }
    protected:
        std::weak_ptr<T> m_parentPtr;
        std::list<std::shared_ptr<T>> m_children;

        /**
         * Moves a child to a new index. Throws std::invalid_argument exception if child is null.
         *
         * @param   std::shared_ptr<T> - child to move.
         * @param   int - new index
         * @return  bool - true if the child was found and removed.
         */
        virtual bool MoveChild(std::shared_ptr<T> childPtr, int newIndex)
        {
            if (childPtr == nullptr)
            {
                throw std::invalid_argument("Argument null exception");
            }

            auto iter = std::find(m_children.begin(), m_children.end(), childPtr);
            if (iter != m_children.end())
            {
                m_children.remove(childPtr);
                m_children.insert(std::next(m_children.begin(), newIndex), childPtr);
                return true;
            }
            return false;
        }

        /**
         * Protected implementation of AddChild. The public AddChild calls this one. Derived classes can override
         * either to change public or internal behaviour.
         * 
         * Adds a child to the object if that does not create a circular reference. If the child has another parent,
         * removes it from its parent first. Throws std::invalid_argument exception if the child is null.
         *
         * @param   std::shared_ptr<T> - pointer to the child to add.
         * @return  bool - true if the child was added. False if it could not be added, either because it was already
         *          added or adding it would create a circular reference.
         */
        virtual bool PerformAddChild(std::shared_ptr<T> childPtr)
        {
            if (childPtr == nullptr)
            {
                throw std::invalid_argument("Argument null exception");
            }

            if (childPtr->m_parentPtr.lock() == std::enable_shared_from_this<T>::shared_from_this() ||
                childPtr == std::enable_shared_from_this<T>::shared_from_this() ||
                IsDescendantOf(childPtr))
            {
                return false;
            }

            childPtr->PerformDetach();
            childPtr->m_parentPtr = std::enable_shared_from_this<T>::shared_from_this();
            m_children.push_back(childPtr);
            return true;
        }

        /**
         * Protected implementation of InsertChild. The public InsertChild calls this one. Derived classes can override
         * either to change public or internal behaviour.
         *
         * Inserts a child at an index if that does not create a circular reference. If the child has another parent,
         * removes it from its parent first. Throws std::invalid_argument exception if the child is null.
         *
         * @param   int - index to insert at.
         * @param   std::shared_ptr<T> - child to insert.
         * @return  bool - true if the child was inserted. False if it could not be inserted, either because it was
         *          already added, adding it would create a circular reference, or the index was out of bounds.
         */
        virtual bool PerformInsertChild(int index, std::shared_ptr<T> childPtr)
        {
            if (childPtr == nullptr)
            {
                throw std::invalid_argument("Argument null exception");
            }

            if (index > (int)m_children.size() || index < 0 ||
                childPtr->m_parentPtr.lock() == std::enable_shared_from_this<T>::shared_from_this() ||
                childPtr == std::enable_shared_from_this<T>::shared_from_this() ||
                IsDescendantOf(childPtr))
            {
                return false;
            }

            childPtr->PerformDetach();
            childPtr->m_parentPtr = std::enable_shared_from_this<T>::shared_from_this();
            auto iter = std::next(m_children.begin(), index);
            m_children.insert(iter, childPtr);
            return true;
        }

        /**
         * Protected implementation of RemoveChild. The public RemoveChild calls this one. Derived classes can override
         * either to change public or internal behaviour.
         *
         * Removes a child from this object. Throws std::invalid_argument exception if child is null.
         *
         * @param   std::shared_ptr<T> - child to remove.
         * @return  bool - true if the child was found and removed.
         */
        virtual bool PerformRemoveChild(std::shared_ptr<T> childPtr)
        {
            if (childPtr == nullptr)
            {
                throw std::invalid_argument("Argument null exception");
            }

            auto iter = std::find(m_children.begin(), m_children.end(), childPtr);
            if (iter != m_children.end()) 
            {
                m_children.remove(childPtr);
                childPtr->m_parentPtr.reset();
                return true;
            }
            return false;
        }

        /**
         * Protected implementation of Detach.
         *
         * Removes the object from its parent. Does nothing if the object has no parent.
         */
        void PerformDetach()
        {
            std::shared_ptr<T> parentPtr = Parent();
            if (parentPtr != nullptr)
            {
                std::static_pointer_cast<ksHierarchyObject<T>>(parentPtr)->
                    PerformRemoveChild(std::enable_shared_from_this<T>::shared_from_this());
            }
        }
    };
}