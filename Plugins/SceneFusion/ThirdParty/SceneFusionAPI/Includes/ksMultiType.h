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
#include <memory>

#include "Exports.h"
#include "ksVector3.h"
#include "ksQuaternion.h"
#include "Log.h"

#define ARRAY_FLAG 8
#define LOG_CHANNEL "KS::ksMultiType"

namespace KS {

    /**
     * Wraps a byte array and provides methods for converting the data to different types.
     */
    class ksMultiType : public std::enable_shared_from_this<ksMultiType>
    {
        friend class MultiTypeJsonUtil;

    public:
        typedef std::shared_ptr<ksMultiType> SPtr;

        /**
         * Types
         */
        enum
        {
            INT = (uint8_t)0,
            FLOAT = (uint8_t)1,
            STRING = (uint8_t)2,
            BOOL = (uint8_t)3,
            UINT = (uint8_t)4,
            BYTE = (uint8_t)5,
            LONG = (uint8_t)6,
            INT_ARRAY = INT | ARRAY_FLAG,
            FLOAT_ARRAY = FLOAT | ARRAY_FLAG,
            STRING_ARRAY = STRING | ARRAY_FLAG,
            BOOL_ARRAY = BOOL | ARRAY_FLAG,
            UINT_ARRAY = UINT | ARRAY_FLAG,
            BYTE_ARRAY = BYTE | ARRAY_FLAG,
            LONG_ARRAY = LONG | ARRAY_FLAG,
            UNDEFINED = 255
        };

        /**
         * Sets the value by copying data from a byte vector.
         *
         * @param   uint8_t type of value.
         * @param   const std::vector<uint8_t>& data to copy.
         * @param   int arrayLength
         */
        EXTERNAL void SetValue(uint8_t type, const std::vector<uint8_t>& data, int arrayLength);

        /**
         * Sets the value by copying data from a source array.
         *
         * @param   uint8_t type of value
         * @param   const uint8_t* byte data
         * @param   size_t data length
         * @param   int arrayLength
         */
        EXTERNAL void SetValue(uint8_t type, const uint8_t* byteArray, size_t byteCount, int arrayLength);

        /**
         * Static shared pointer constructor
         *
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr();

        /**
         * Constructor
         */
        EXTERNAL ksMultiType();

        /**
         * Static shared pointer constructor
         *
         * @param   uint8_t type of value
         * @param   const std::vector<uint8_t>& data to set
         * @param   int arrayLength
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(uint8_t type, const std::vector<uint8_t>& data, int arrayLength);

        /**
         * Constructor
         *
         * @param   uint8_t type of value
         * @param   const std::vector<uint8_t>& data to set
         * @param   int arrayLength
         */
        EXTERNAL ksMultiType(uint8_t type, const std::vector<uint8_t>& data, int arrayLength);

        /**
         * Constructor. Sets a byte array without copying.
         *
         * @param   std::vector<uint8_t>&& - byte array
         */
        EXTERNAL ksMultiType(std::vector<uint8_t>&& byteArray);

        /**
         * Static shared pointer constructor. Sets a byte array without copying.
         *
         * @param   std::vector<uint8_t>&& - byte array
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(std::vector<uint8_t>&& byteArray);

        /**
         * Static shared pointer constructor
         *
         * @param   uint8_t type of value
         * @param   const uint8_t* byte data
         * @param   size_t data length
         * @param   int arrayLength
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(uint8_t type, const uint8_t* byteArray, size_t byteCount, int arrayLength);

        /**
         * Constructor
         *
         * @param   uint8_t type of value
         * @param   const uint8_t* byte data
         * @param   size_t data length
         * @param   int arrayLength
         */
        EXTERNAL ksMultiType(uint8_t type, const uint8_t* byteArray, size_t byteCount, int arrayLength);

        /**
         * Static shared pointer constructor
         *
         * @param   int value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(int value);

        /**
         * Constructor
         *
         * @param   int value
         */
        EXTERNAL ksMultiType(int value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<int>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<int>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<int>& value
         */
        EXTERNAL ksMultiType(const std::vector<int>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   float value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(float value);

        /**
         * Constructor
         *
         * @param   float value
         */
        EXTERNAL ksMultiType(float value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<float>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<float>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<float>& value
         */
        EXTERNAL ksMultiType(const std::vector<float>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::string& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::string& value);

        /**
         * Constructor
         *
         * @param   const std::string& value
         */
        EXTERNAL ksMultiType(const std::string& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const char* value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const char* value);

        /**
         * Constructor
         *
         * @param   const char* value
         */
        EXTERNAL ksMultiType(const char* value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<std::string>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<std::string>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<std::string>& value
         */
        EXTERNAL ksMultiType(const std::vector<std::string>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   bool value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(bool value);

        /**
         * Constructor
         *
         * @param   bool value
         */
        EXTERNAL ksMultiType(bool value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<bool>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<bool>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<bool>& value
         */
        EXTERNAL ksMultiType(const std::vector<bool>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   uint32_t value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(uint32_t value);

        /**
         * Constructor
         *
         * @param   uint32_t value
         */
        EXTERNAL ksMultiType(uint32_t value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<uint32_t>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<uint32_t>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<uint32_t>& value
         */
        EXTERNAL ksMultiType(const std::vector<uint32_t>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   uint8_t value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(uint8_t value);

        /**
         * Constructor
         *
         * @param   uint8_t value
         */
        EXTERNAL ksMultiType(uint8_t value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<uint8_t>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<uint8_t>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<uint8_t>& value
         */
        EXTERNAL ksMultiType(const std::vector<uint8_t>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   int64_t value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(int64_t value);

        /**
         * Constructor
         *
         * @param   int64_t value
         */
        EXTERNAL ksMultiType(int64_t value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<int64_t>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<int64_t>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<int64_t>& value
         */
        EXTERNAL ksMultiType(const std::vector<int64_t>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const ksVector3& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const ksVector3& value);

        /**
         * Constructor
         *
         * @param   const ksVector3& value
         */
        EXTERNAL ksMultiType(const ksVector3& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<ksVector3>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<ksVector3>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<ksVector3>& value
         */
        EXTERNAL ksMultiType(const std::vector<ksVector3>& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const ksQuaternion& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const ksQuaternion& value);

        /**
         * Constructor
         *
         * @param   const ksQuaternion& value
         */
        EXTERNAL ksMultiType(const ksQuaternion& value);

        /**
         * Static shared pointer constructor
         *
         * @param   const std::vector<ksQuaternion>& value
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const std::vector<ksQuaternion>& value);

        /**
         * Constructor
         *
         * @param   const std::vector<ksQuaternion>& value
         */
        EXTERNAL ksMultiType(const std::vector<ksQuaternion>& value);

        /**
         * Static shared pointer copy constructor
         *
         * @param   const ksMultiType& value to copy.
         * @return  SPtr
         */
        EXTERNAL static SPtr CreateSPtr(const ksMultiType& value);

        /**
         * Copy constructor
         *
         * @param   const ksMultiType& value to copy.
         */
        EXTERNAL ksMultiType(const ksMultiType& value);

        /**
         * Move constructor
         *
         * @param   ksMultiType&& value to move.
         */
        EXTERNAL ksMultiType(ksMultiType&& value);

        /**
         * Destructor
         */
        EXTERNAL ~ksMultiType();

        /**
         * @return  int value
         */
        EXTERNAL int GetInt() const;

        /**
         * Implicit conversion to int.
         *
         * @return int value
         */
        EXTERNAL operator int() const;

        /**
         * @return  float value
         */
        EXTERNAL float GetFloat() const;

        /**
         * Implicit conversion to float.
         *
         * @return float value
         */
        EXTERNAL operator float() const;

        /**
         * @return  std::string value
         */
        EXTERNAL std::string GetString() const;

        /**
         * Implicit conversion to string.
         *
         * @return std::string value
         */
        EXTERNAL operator std::string() const;

        /**
         * @return  bool value
         */
        EXTERNAL bool GetBool() const;

        /**
         * Implicit conversion to bool.
         *
         * @return bool value
         */
        EXTERNAL operator bool() const;

        /**
         * @return  uint32_t value
         */
        EXTERNAL uint32_t GetUInt() const;

        /**
         * Implicit conversion to uint.
         *
         * @return uint32_t value
         */
        EXTERNAL operator uint32_t() const;

        /**
         * @return  uint8_t value
         */
        EXTERNAL uint8_t GetByte() const;

        /**
         * Implicit conversion to byte.
         *
         * @return uint8_t value
         */
        EXTERNAL operator uint8_t() const;

        /**
         * @return  int64_t value
         */
        EXTERNAL int64_t GetLong() const;

        /**
         * Implicit conversion to int64.
         *
         * @return int64_t value
         */
        EXTERNAL operator int64_t() const;

        /**
         * @return  ksVector3 value
         */
        EXTERNAL ksVector3 GetVector3() const;

        /**
         * Implicit conversion to ksVector3.
         *
         * @return ksVector3 value
         */
        EXTERNAL operator ksVector3() const;

        /**
         * @return ksQuaternion value
         */
        EXTERNAL ksQuaternion GetQuaternion() const;

        /**
         * Implicit conversion to ksQuaternion.
         *
         * @return ksQuaternion value
         */
        EXTERNAL operator ksQuaternion() const;

        /**
         * Puts the contents of the property into an int vector.
         *
         * @param   std::vector<int>& array to fill.
         */
        EXTERNAL void GetIntArray(std::vector<int>& array) const;

        /**
         * Puts the contents of the property into a float vector.
         *
         * @param   std::vector<float>& array to fill.
         */
        EXTERNAL void GetFloatArray(std::vector<float>& array) const;

        /**
         * Puts the contents of the property into a string vector.
         *
         * @param   std::vector<string>& array to fill.
         */
        EXTERNAL void GetStringArray(std::vector<std::string>& array) const;

        /**
         * Puts the contents of the property into a bool vector.
         *
         * @param   std::vector<bool>& array to fill.
         */
        EXTERNAL void GetBoolArray(std::vector<bool>& array) const;

        /**
         * Puts the contents of the property into a uint vector.
         *
         * @param   std::vector<uint32_t>& array to fill.
         */
        EXTERNAL void GetUIntArray(std::vector<uint32_t>& array) const;

        /**
         * Puts the contents of the property into a byte vector.
         *
         * @param   std::vector<uint8_t>& array to fill.
         */
        EXTERNAL void GetByteArray(std::vector<uint8_t>& array) const;

        /**
         * Puts the contents of the property into an int64 vector.
         *
         * @param   std::vector<int64_t>& array to fill.
         */
        EXTERNAL void GetLongArray(std::vector<int64_t>& array) const;

        /**
         * Puts the contents of the property into a ksVector3 vector.
         *
         * @param   std::vector<ksVector3>& array to fill.
         */
        EXTERNAL void GetVector3Array(std::vector<ksVector3>& array) const;

        /**
         * Puts the contents of the property into a ksQuaternion vector.
         *
         * @param   std::vector<ksQuaternion>& array to fill.
         */
        EXTERNAL void GetQuaternionArray(std::vector<ksQuaternion>& array) const;
        
        /**
         * @return  std::vector<uint8_t>& the underlying byte array.
         */
        EXTERNAL const std::vector<uint8_t>& GetData() const;

        /**
         * Moves and returns the underlying byte array.
         *
         * @return  std::vector<uint8_t>&&
         */
        EXTERNAL std::vector<uint8_t>&& MoveData();

        /**
         * @return  uint8_t type of value.
         */
        EXTERNAL uint8_t GetType() const;

        /**
         * @return  int array length.
         */
        EXTERNAL int GetArrayLength() const;

        /**
         * @return  bool is this an array type?
         */
        EXTERNAL bool IsArray() const;

        /**
         * Returns true if the given type is an array type.
         *
         * @param   uint8_t type
         * @return  bool
         */
        EXTERNAL static bool TypeIsArray(uint8_t type);

        /**
         * @return int size of type, factoring in array length. -1 for variable length types.
         */
        EXTERNAL int GetTypeSize() const;

        /**
         * Returns the number of bytes that a ksMultitype requires to store an array of data.
         *
         * @param   uint8_t type
         * @param   int arrayLength
         * @return  int
         */
        INTERNAL static int GetTypeSize(uint8_t type, int arrayLength = 1);

        /**
         * Assigns an int value.
         *
         * @param   int rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(int rhs);

        /**
         * Assigns an int array value.
         *
         * @param   const std::vector<int>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<int>& rhs);

        /**
         * Assigns a float value.
         *
         * @param   float rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(float rhs);

        /**
         * Assigns a float array value.
         *
         * @param   const std::vector<float>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<float>& rhs);

        /**
         * Assigns a string value.
         *
         * @param   std::string& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::string& rhs);

        /**
         * Assigns a string value.
         *
         * @param   const char* rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const char* rhs);

        /**
         * Assigns a string array value.
         *
         * @param   const std::vector<std::string>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<std::string>& rhs);

        /**
         * Assigns a bool value.
         *
         * @param   bool rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(bool rhs);

        /**
         * Assigns a bool array value.
         *
         * @param   const std::vector<bool>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<bool>& rhs);

        /**
         * Assigns a uint value.
         *
         * @param   uint32_t rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(uint32_t rhs);

        /**
         * Assigns a uint array value.
         *
         * @param   const std::vector<uint32_t>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<uint32_t>& rhs);

        /**
         * Assigns a byte value.
         *
         * @param   uint8_t rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(uint8_t rhs);

        /**
         * Assigns a byte array value.
         *
         * @param   const std::vector<uint8_t>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<uint8_t>& rhs);

        /**
         * Assigns an int64 value.
         *
         * @param   int64_t rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(int64_t rhs);

        /**
         * Assigns an int64 array value.
         *
         * @param   const std::vector<int64_t>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<int64_t>& rhs);

        /**
         * Assigns a ksVector3 value.
         *
         * @param   const ksVector3& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const ksVector3& rhs);

        /**
         * Assigns a ksVector3 array value.
         *
         * @param   const std::vector<ksVector3>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<ksVector3>& rhs);

        /**
         * Assigns a ksQuaternion value.
         *
         * @param   const ksQuaternion& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const ksQuaternion& rhs);

        /**
         * Assigns a ksQuaternion array value.
         *
         * @param   const std::vector<ksQuaternion>& rhs
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const std::vector<ksQuaternion>& rhs);

        /**
         * Copy assignment.
         *
         * @param   const ksMultiType& rhs to copy.
         * @return  ksMultiType& this
         */
        EXTERNAL ksMultiType& operator=(const ksMultiType& rhs);

        /**
         * Checks if two multiTypes are equivalent.
         *
         * @param   const ksMultiType& other
         * @return  bool true if the multiTypes are equivalent.
         */
        EXTERNAL bool operator==(const ksMultiType& other) const;

        /**
         * Checks if two multiTypes are not equivalent.
         *
         * @param   const ksMultiType& other
         * @return  bool true if the multiTypes are not equivalent.
         */
        EXTERNAL bool operator!=(const ksMultiType& other) const;

        /**
         * @return  std::string representation of the multiType.
         */
        EXTERNAL std::string ToString() const;

    private:
        uint8_t m_type;
        std::vector<uint8_t> m_data;
        int m_arrayLength = -1;

        /**
         * Sets the value to a string.
         *
         * @param   const std::string& value
         */
        void SetValue(const std::string& value);

        /**
         * Sets the value to a string array.
         *
         * @param   const std::vector<string>& array
         */
        void SetValue(const std::vector<std::string>& array);

        /**
         * Sets the value to a bool.
         *
         * @param   bool value
         */
        void SetValue(bool value);

        /**
         * Sets the value to a bool array.
         *
         * @param   const std::vector<bool>& array
         */
        void SetValue(const std::vector<bool>& array);

        /**
         * Sets the value to a byte.
         *
         * @param   uint8_t value
         */
        void SetValue(uint8_t value);

        /**
         * Sets the value.
         *
         * @param   const T& value to set.
         * @param   uint8_t type of value.
         */
        template<typename T>
        void SetValue(const T& value, uint8_t type)
        {
            m_type = type;
            const uint8_t* temp = reinterpret_cast<const uint8_t*>(&value);
            m_data.resize(sizeof(T));
            m_data.assign(temp, temp + sizeof(T));
            m_arrayLength = 1;
        }

        /**
         * Sets the value to an array to type T.
         *
         * @param   const T* values
         * @param   size_t size of the array.
         * @param   uint8_t type of value.
         */
        template<typename T>
        void SetValue(const T* values, size_t size, uint8_t type)
        {
            m_type = type;
            if (values == nullptr)
            {
                m_arrayLength = -1;
                return;
            }
            m_arrayLength = (int)size;
            m_data.reserve(sizeof(T) * size);
            const uint8_t* temp = reinterpret_cast<const uint8_t*>(values);
            m_data.assign(temp, temp + sizeof(T) * size);
        }

        /**
         * Parses the data as type T.
         *
         * @return  T value
         */
        template<typename T>
        T Parse() const
        {
            return *(reinterpret_cast<const T*>(m_data.data()));
        }

        /**
         * Parses the data as a string.
         *
         * @param   std::string& str to write to.
         */
        void ParseString(std::string& str) const;

        /**
         * Parses the data as a bool.
         *
         * @return  bool true if the least significant bit of the first byte is set.
         */
        bool ParseBool() const;

        /**
         * Converts a templated array to a string.
         *
         * @param   const T* array
         * @param   std::string& str to write to.
         */
        template<typename T>
        void ArrayToString(const T* array, int size, std::string& str) const
        {
            if (array == nullptr)
            {
                str = "null";
                return;
            }
            str = "[";
            for (int i = 0; i < size; i++)
            {
                if (i > 0)
                {
                    str += ", ";
                }
                str += std::to_string(array[i]);
            }
            str += "]";
        }

        /**
         * Parses the data as an array of type T and fills a T vector with a copy of the data.
         *
         * @param   std::vector<T>& vector to fill.
         * @param   int size - number of elements to add to vector.
         * @param   uint8_t type - if different from the actual type, will clear the vector and log a warning.
         * @param   std::string typeString of T to appear in log message if the type is wrong.
         */
        template<typename T>
        void GetArray(std::vector<T>& vector, int size, uint8_t type, std::string typeString) const
        {
            vector.clear();
            if (m_type == type)
            {
                const T* data = reinterpret_cast<const T*>(m_data.data());
                for (int i = 0; i < size; i++)
                {
                    vector.push_back(data[i]);
                }
            }
            else
            {
                KS::Log::Warning("Type mismatch: cannot get " + typeString + " array. Type: " +
                    std::to_string(m_type), LOG_CHANNEL);
            }
        }
    };
}
#undef LOG_CHANNEL
#undef ARRAY_FLAG
