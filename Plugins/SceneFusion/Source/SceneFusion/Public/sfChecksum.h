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

#include "sfDictionaryProperty.h"
#include "sfListProperty.h"
#include "sfValueProperty.h"
#include "sfReferenceProperty.h"
#include "sfNullProperty.h"
#include "sfProperty.h"
#include <CoreMinimal.h>
#include <functional>

using namespace KS::SceneFusion2;

/**
 * Utility for computing checksums from sfproperties.
 */
class sfChecksum
{
public:
    /**
     * Filter delegate for filtering dictionary keys from the checksum.
     *
     * @param   const sfName& name of the property.
     * @return  bool true if the property should be included in the checksum.
     */
    typedef std::function<bool(const sfName& name)> Filter;

    /**
     * Computes a Fletcher-64 checksum from an sfproperty.
     *
     * @param   sfProperty::SPtr propertyPtr to compute checksum for.
     * @param   Filter filter for filtering dictionary sub-properties. If nullptr, all dictionary sub-properties will
     *          be included in the checksum.
     * @return  uint64_t checksum
     */
    static uint64_t Fletcher64(sfProperty::SPtr propertyPtr, Filter filter = nullptr);

private:
    /**
     * Adds a value to checksum1, then adds checksum1 to checksum2.
     *
     * @param   uint32_t value
     * @param   uint64_t& checksum1
     * @param   uint64_t& checksum2
     */
    static void Checksum(uint32_t value, uint64_t& checksum1, uint64_t& checksum2);

    /**
     * Updates two checksum values using property data.
     *
     * @param   sfProperty::SPtr propertyPtr to compute checksum from.
     * @param   uint64_t checksum1 to update.
     * @param   uint64_t checksum2 to update.
     * @param   Filter filter for filtering dictionary sub-properties. If nullptr, all dictionary sub-properties will
     *          be included in the checksum.
     */
    static void Checksum(sfProperty::SPtr propertyPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter);

    /**
     * Updates two checksum values using dictionary property data.
     *
     * @param   sfDictionaryProperty::SPtr dictPtr to compute checksum from.
     * @param   uint64_t checksum1 to update.
     * @param   uint64_t checksum2 to update.
     * @param   Filter filter for filtering dictionary sub-properties. If nullptr, all dictionary sub-properties will
     *          be included in the checksum.
     */
    static void Checksum(sfDictionaryProperty::SPtr dictPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter);

    /**
     * Updates two checksum values using list property data.
     *
     * @param   sfListProperty::SPtr listPtr to compute checksum from.
     * @param   uint64_t checksum1 to update.
     * @param   uint64_t checksum2 to update.
     * @param   Filter filter for filtering dictionary sub-properties. If nullptr, all dictionary sub-properties will
     *          be included in the checksum.
     */
    static void Checksum(sfListProperty::SPtr listPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter);

    /**
     * Updates two checksum values using value property data.
     *
     * @param   sfValue::SPtr valuePtr to compute checksum from.
     * @param   uint64_t checksum1 to update.
     * @param   uint64_t checksum2 to update.
     */
    static void Checksum(sfValueProperty::SPtr valuePtr, uint64_t& checksum1, uint64_t& checksum2);

    /**
     * Updates two checksum values using reference property data.
     *
     * @param   sfReference::SPtr referencePtr to compute checksum from.
     * @param   uint64_t checksum1 to update.
     * @param   uint64_t checksum2 to update.
     */
    static void Checksum(sfReferenceProperty::SPtr referencePtr, uint64_t& checksum1, uint64_t& checksum2);
};