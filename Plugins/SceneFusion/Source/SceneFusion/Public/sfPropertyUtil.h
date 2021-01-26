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

#include "sfValueProperty.h"
#include "sfNullProperty.h"
#include "../Public/SceneFusion.h"
#include <CoreMinimal.h>
#include <Math/IntRect.h>

using namespace KS;
using namespace KS::SceneFusion2;

#define LOG_CHANNEL "sfPropertyUtil"

/**
 * Utility for converting between SF properties and common Unreal types.
 */
class sfPropertyUtil
{
public:
    /**
     * Constructs a property from a vector.
     *
     * @param   const FVector& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromVector(const FVector& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to a vector.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FVector
     */
    static FVector ToVector(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FVector>(propertyPtr);
    }

    /**
     * Constructs a property from a rotator.
     *
     * @param   const FRotator& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromRotator(const FRotator& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to a rotator.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FRotator
     */
    static FRotator ToRotator(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FRotator>(propertyPtr);
    }

    /**
     * Constructs a property from a quat.
     *
     * @param   const FQuat& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromQuat(const FQuat& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to a quat.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FQuat
     */
    static FQuat ToQuat(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FQuat>(propertyPtr);
    }

    /**
     * Constructs a property from a box.
     *
     * @param   const FBox& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromBox(const FBox& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to a box.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FBox
     */
    static FBox ToBox(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FBox>(propertyPtr);
    }

    /**
     * Constructs a property from a box2d.
     *
     * @param   const FBox2D& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromBox2D(const FBox2D& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to a box2d.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FBox2D
     */
    static FBox2D ToBox2D(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FBox2D>(propertyPtr);
    }

    /**
     * Constructs a property from an int rect.
     *
     * @param   const FIntRect& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromIntRect(const FIntRect& value)
    {
        return ToProperty(value);
    }

    /**
     * Converts a property to an int rect.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FIntRect
     */
    static FIntRect ToIntRect(sfProperty::SPtr propertyPtr)
    {
        return FromProperty<FIntRect>(propertyPtr);
    }

    /**
     * Constructs a property from a string.
     *
     * @param   const FString& value
     * @return  sfValueProperty::SPtr
     */
    static sfValueProperty::SPtr FromString(const FString& value)
    {
        if (SceneFusion::Service->Session() == nullptr)
        {
            KS::Log::Error("Cannot convert string to property; session is nullptr", LOG_CHANNEL);
            return sfValueProperty::Create(0);
        }
        std::string str = TCHAR_TO_UTF8(*value);
        uint32_t id = SceneFusion::Service->Session()->GetStringTableId(str);
        return sfValueProperty::Create(id);
    }

    /**
     * Converts a property to a string.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FString
     */
    static FString ToString(sfProperty::SPtr propertyPtr)
    {
        if (SceneFusion::Service->Session() == nullptr)
        {
            KS::Log::Error("Cannot convert property to string; session is nullptr", LOG_CHANNEL);
            return "";
        }
        if (propertyPtr == nullptr || propertyPtr->Type() != sfProperty::VALUE)
        {
            return "";
        }
        sfValueProperty::SPtr valuePtr = propertyPtr->AsValue();
        if (valuePtr->GetValue().GetType() == ksMultiType::STRING)
        {
            std::string str = valuePtr->GetValue();
            return FString(UTF8_TO_TCHAR(str.c_str()));
        }
        uint32_t id = valuePtr->GetValue();
        sfName name = SceneFusion::Service->Session()->GetStringFromTable(id);
        return FString(UTF8_TO_TCHAR(name->c_str()));
    }

    /**
     * Constructs a property from an FText.
     *
     * @param   const FText& value
     * @return  sfProperty::SPtr
     */
    static sfProperty::SPtr FromText(const FText& value)
    {
        sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
        if (sessionPtr == nullptr)
        {
            KS::Log::Error("Cannot convert FText to property; session is nullptr", LOG_CHANNEL);
            return sfNullProperty::Create();
        }
        std::vector<uint32_t> stringIds;
        if (value.IsCultureInvariant())
        {
            stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*value.ToString())));
        }
        else if (value.IsFromStringTable())
        {
            FName table;
            FString key;
            if (FTextInspector::GetTableIdAndKey(value, table, key))
            {
                stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*table.ToString())));
                stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*key)));
            }
        }
        else
        {
            stringIds.emplace_back(sessionPtr->GetStringTableId(
                TCHAR_TO_UTF8(*FTextInspector::GetNamespace(value).Get(""))));
            stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*FTextInspector::GetKey(value).Get(""))));
            stringIds.emplace_back(sessionPtr->GetStringTableId(TCHAR_TO_UTF8(*value.ToString())));
        }
        if (stringIds.size() == 0)
        {
            return sfNullProperty::Create();
        }
        return sfValueProperty::Create(stringIds);
    }

    /**
     * Converts a property to an FText.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  FText
     */
    static FText ToText(sfProperty::SPtr propertyPtr)
    {
        if (propertyPtr->Type() == sfProperty::NUL)
        {
            return FText::FromString("");
        }
        sfSession::SPtr sessionPtr = SceneFusion::Service->Session();
        std::vector<uint32_t> stringIds;
        propertyPtr->AsValue()->GetValue().GetUIntArray(stringIds);
        if (stringIds.size() == 1)
        {
            // CultureInvariant = true, stringIds = text
            FString text = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str());
            return FText::AsCultureInvariant(text);;
        }
        FString key = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[1])->c_str());
        if (stringIds.size() == 2)
        {
            // stringIds = table, key
            FName table = FName(UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str()));
            return FText::FromStringTable(table, key);
        }
        else if (stringIds.size() == 3)
        {
            // stringIds = namespace, key, text
            FString nameSpace = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[0])->c_str());
            FString sourceString = UTF8_TO_TCHAR(sessionPtr->GetStringFromTable(stringIds[2])->c_str());
            return FText::ChangeKey(nameSpace, key, FText::FromString(sourceString));
        }
        return FText::FromString("");
    }

    /**
     * Converts a property to an sfName.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  sfName
     */
    static sfName ToName(sfProperty::SPtr propertyPtr)
    {
        if (SceneFusion::Service->Session() == nullptr)
        {
            KS::Log::Error("Cannot convert property to name; session is nullptr", LOG_CHANNEL);
            return "";
        }
        if (propertyPtr == nullptr || propertyPtr->Type() != sfProperty::VALUE)
        {
            return "";
        }
        sfValueProperty::SPtr valuePtr = propertyPtr->AsValue();
        if (valuePtr->GetValue().GetType() == ksMultiType::STRING)
        {
            std::string str = valuePtr->GetValue();
            return str;
        }
        uint32_t id = valuePtr->GetValue();
        return SceneFusion::Service->Session()->GetStringFromTable(id);
    }

private:
    /**
     * Constructs a property from a T.
     *
     * @param   const T& value
     * @return  sfValueProperty::SPtr
     */
    template<typename T>
    static sfValueProperty::SPtr ToProperty(const T& value)
    {
        const uint8_t* temp = reinterpret_cast<const uint8_t*>(&value);
        ksMultiType multiType(ksMultiType::BYTE_ARRAY, temp, sizeof(T), sizeof(T));
        return sfValueProperty::Create(std::move(multiType));
    }

    /**
     * Converts a property to T.
     *
     * @param   sfProperty::SPtr propertyPtr
     * @return  T
     */
    template<typename T>
    static T FromProperty(sfProperty::SPtr propertyPtr)
    {
        if (propertyPtr == nullptr || propertyPtr->Type() != sfProperty::VALUE)
        {
            return T();
        }
        sfValueProperty::SPtr valuePtr = propertyPtr->AsValue();
        return *(reinterpret_cast<const T*>(valuePtr->GetValue().GetData().data()));
    }
};

#undef LOG_CHANNEL