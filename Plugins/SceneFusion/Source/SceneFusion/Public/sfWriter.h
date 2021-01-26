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

#include <ostream>
#include <string>
#include <sfObject.h>
#include <sfSession.h>
#include <sfProperty.h>
#include <sfDictionaryProperty.h>
#include <sfListProperty.h>
#include <sfValueProperty.h>
#include <sfReferenceProperty.h>
#include <sfNullProperty.h>

using namespace KS::SceneFusion2;

class sfWriter
{
public:
    /**
     * @return  sfWriter& singleton instance.
     */
    static sfWriter& Get();

    /**
     * Writes an object to a string.
     *
     * @param   sfObject::SPtr objPtr to write.
     * @param   bool recurseChildren - if true, will also write descendants of the object.
     * @return  std::string
     */
    std::string Write(sfObject::SPtr objPtr, bool recurseChildren = false);

    /**
     * Writes an object to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfObject::SPtr objPtr to write.
     * @param   bool recurseChildren - if true, will also write descendants of the object.
     */
    virtual void Write(std::ostream* outputPtr, sfObject::SPtr objPtr, bool recurseChildren = false);

    /**
     * Writes an object to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfObject::SPtr objPtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     * @param   int depth of the object. Controls indentation.
     * @param   bool recurseChildren - if true, will also write descendants of the object.
     */
    virtual void Write(
        std::ostream* outputPtr,
        sfObject::SPtr objPtr,
        sfSession::SPtr sessionPtr,
        int depth,
        bool recurseChildren);

    /**
     * Writes a property to a string.
     *
     * @param   std::ostream* outputPtr to write to.
     * @return  std::string
     */
    std::string Write(sfProperty::SPtr propPtr);

    /**
     * Writes a property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfProperty::SPtr propPtr to write.
     */
    virtual void Write(std::ostream* outputPtr, sfProperty::SPtr propPtr);

    /**
     * Writes a property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfProperty::SPtr propPtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     * @param   int depth of the property. Controls indentation.
     */
    virtual void Write(
        std::ostream* outputPtr,
        sfProperty::SPtr propPtr,
        sfSession::SPtr sessionPtr,
        int depth);

protected:
    /**
     * Writes a dictionary property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfDictionaryProperty::SPtr dictPtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     * @param   int depth of the property. Controls indentation.
     */
    virtual void WriteDictionary(
        std::ostream* outputPtr,
        sfDictionaryProperty::SPtr dictPtr,
        sfSession::SPtr sessionPtr,
        int depth);

    /**
     * Writes a list property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfListProperty::SPtr listPtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     * @param   int depth of the property. Controls indentation.
     */
    virtual void WriteList(
        std::ostream* outputPtr,
        sfListProperty::SPtr listPtr,
        sfSession::SPtr sessionPtr,
        int depth);

    /**
     * Writes a value property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfValueProperty::SPtr valuePtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     */
    virtual void WriteValue(
        std::ostream* outputPtr,
        sfValueProperty::SPtr valuePtr,
        sfSession::SPtr sessionPtr);

    /**
     * Writes a reference property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfReferenceProperty::SPtr referencePtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     */
    virtual void WriteReference(
        std::ostream* outputPtr,
        sfReferenceProperty::SPtr referencePtr,
        sfSession::SPtr sessionPtr);

    /**
     * Writes a null property to an output stream.
     *
     * @param   std::ostream* outputPtr to write to.
     * @param   sfNullProperty::SPtr nullPtr to write.
     * @param   sfSession::SPtr sessionPtr. If provided, will lookup uints in the session's string table.
     */
    virtual void WriteNull(std::ostream* outputPtr, sfNullProperty::SPtr nullPtr, sfSession::SPtr sessionPtr);
};