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

#include <string>
#include <Exports.h>

namespace KS {
namespace SceneFusion2 {

    /**
     * Stores a pointer to a constant string. Can be dereferenced to access the string. All sfNames constructed with
     * the same string value will point to the same memory address. This is used to reduce memory and provide fast
     * string comparisons.
     */
    class EXTERNAL sfName
    {
    public:
        /**
         * Constructor. Points to nullptr.
         */
        sfName();

        /**
         * Constructor
         *
         * @param   const std::string& str. The string pointed to will be a copy of this string.
         */
        sfName(const std::string& str);

        /**
         * Constructor
         *
         * @param   const char* str. The string pointed to will be a copy of this string.
         */
        sfName(const char* str);

        /**
         * Checks if this sfName points to a valid string.
         *
         * @return  bool true if the string pointer is valid.
         */
        bool IsValid() const;

        /**
         * Dereferences to the constant string.
         *
         * @return  const std::string&
         */
        const std::string& operator*() const { return *m_strPtr; }

        /**
         * Deferences to the string pointer.
         *
         * @param   const std::string*
         */
        const std::string* operator->() const { return m_strPtr; }

        /**
         * Equality comparison.
         *
         * @param   const sfName& name to compare with.
         * @return  bool true if the names are the same.
         */
        inline bool operator==(const sfName& name) const
        {
            return m_strPtr == name.m_strPtr || (!IsValid() && !name.IsValid());
        }

        /**
         * Inequality comparison.
         *
         * @param   const sfName& name to compare with.
         * @param   bool true if the names are different.
         */
        inline bool operator !=(const sfName& name) const
        {
            return !(*this == name);
        }

    private:
        const std::string* m_strPtr;
        uint32_t m_tableId;
    };

} // SceneFusion2
} // KS

// Define a hash function for sfName
namespace std
{
    template<>
    struct hash<KS::SceneFusion2::sfName>
    {
        /**
         * Hashes an sfName by hashing the memory address of the string it points to.
         *
         * @param   const KS::SceneFusion2::sfName& key to hash
         * @return  size_t hash value
         */
        inline size_t operator()(const KS::SceneFusion2::sfName& key) const
        {
            return key == nullptr ? 0 : reinterpret_cast<uintptr_t>(&*key);
        }
    };
}