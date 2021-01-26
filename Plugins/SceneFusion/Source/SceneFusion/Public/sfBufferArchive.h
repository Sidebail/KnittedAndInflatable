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

#include <CoreMinimal.h>
#include <Serialization/BufferArchive.h>
#include <Serialization/BufferReader.h>
#include <CoreMinimal.h>

/**
 * Serializer that serializes UObject* as sfObject ids or asset path.
 */
class sfBufferArchive : public FBufferArchive
{
public:
    /**
     * Tries to serialize a uobject reference by its sfObject id or asset path.
     * 
     * @param   UObject*& uobjPtr reference to serialize.
     * @return  FArchive& this
     */
    virtual FArchive& operator<<(UObject*& uobjPtr) override;

    /**
     * @return  const TSet<uint32_t>& String table ids for missing asset paths the serializer encountered.
     */
    const TSet<uint32_t>& MissingPathIds();

private:
    TSet<uint32_t> m_missingPathIds;
};

/**
 * Deserializer that deserializes UObject* from sfObject ids or asset paths.
 */
class sfBufferReader : public FBufferReaderBase
{
public:
    /**
     * Constructor
     *
     * @param   void* data to deserialize from.
     * @param   int64_t size of the data.
     */
    sfBufferReader(void* data, int64_t size);

    /**
     * Tries to deserialize a uobject reference from an sfObject id or asset path.
     *
     * @param   UObject*& uobjPtr to deserialize.
     * @return  FArchive& this
     */
    virtual FArchive& operator<<(UObject*& uobjPtr) override;

    /**
     * Deserializes an FName from a string.
     *
     * @param   FName& name to deserialize.
     * @return  FArchive& this
     */
    virtual FArchive& operator<<(FName& name) override;

    /**
     * @return  const TSet<uint32_t>& String table ids for missing asset paths the serializer encountered.
     */
    const TSet<uint32_t>& MissingPathIds();

private:
    TSet<uint32_t> m_missingPathIds;
};