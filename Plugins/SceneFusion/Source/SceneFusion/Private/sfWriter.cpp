#include "../Public/sfWriter.h"

#include <strstream>

using namespace KS;

sfWriter& sfWriter::Get()
{
    static sfWriter writer;
    return writer;
}

std::string sfWriter::Write(sfObject::SPtr objPtr, bool recurseChildren)
{
    if (objPtr == nullptr)
    {
        return "";
    }
    std::strstream stream;
    Write(&stream, objPtr, recurseChildren);
    return stream.str();
}

void sfWriter::Write(std::ostream* outputPtr, sfObject::SPtr objPtr, bool recurseChildren)
{
    if (objPtr != nullptr)
    {
        Write(outputPtr, objPtr, objPtr->Session(), 0, recurseChildren);
    }
}

void sfWriter::Write(
    std::ostream* outputPtr,
    sfObject::SPtr objPtr,
    sfSession::SPtr sessionPtr,
    int depth,
    bool recurseChildren)
{
    if (objPtr == nullptr)
    {
        return;
    }
    // indentation
    for (int i = 0; i < depth; i++)
    {
        *outputPtr << "  ";
    }
    *outputPtr << "(Id: #" << objPtr->Id() << ", Type: " << *objPtr->Type() << ", Property: ";
    Write(outputPtr, objPtr->Property(), sessionPtr, depth);
    if (recurseChildren && objPtr->Children().size() > 0)
    {
        *outputPtr << ", Children:\n";
        bool first = true;
        for (auto iter : objPtr->Children())
        {
            if (!first)
            {
                *outputPtr << ",\n";
            }
            first = false;
            Write(outputPtr, iter, sessionPtr, depth + 1, true);
        }
        *outputPtr << "\n";
        // indentation
        for (int i = 0; i < depth; i++)
        {
            *outputPtr << "  ";
        }
    }
    *outputPtr << ")";
}

std::string sfWriter::Write(sfProperty::SPtr propPtr)
{
    if (propPtr == nullptr)
    {
        return "";
    }
    std::strstream stream;
    Write(&stream, propPtr);
    return stream.str();
}

void sfWriter::Write(std::ostream* outputPtr, sfProperty::SPtr propPtr)
{
    if (propPtr != nullptr)
    {
        sfSession::SPtr sessionPtr = propPtr->GetContainerObject() == nullptr ?
            nullptr : propPtr->GetContainerObject()->Session();
        Write(outputPtr, propPtr, sessionPtr, 0);
    }
}

void sfWriter::Write(std::ostream* outputPtr, sfProperty::SPtr propPtr, sfSession::SPtr sessionPtr, int depth)
{
    if (propPtr == nullptr)
    {
        return;
    }
    switch (propPtr->Type())
    {
        case sfProperty::DICTIONARY:
        {
            WriteDictionary(outputPtr, propPtr->AsDict(), sessionPtr, depth);
            break;
        }
        case sfProperty::LIST:
        {
            WriteList(outputPtr, propPtr->AsList(), sessionPtr, depth);
            break;
        }
        case sfProperty::VALUE:
        {
            WriteValue(outputPtr, propPtr->AsValue(), sessionPtr);
            break;
        }
        case sfProperty::REFERENCE:
        {
            WriteReference(outputPtr, propPtr->AsReference(), sessionPtr);
            break;
        }
        case sfProperty::NUL:
        {
            WriteNull(outputPtr, propPtr->AsNull(), sessionPtr);
            break;
        }
    }
}

void sfWriter::WriteDictionary(
    std::ostream* outputPtr,
    sfDictionaryProperty::SPtr dictPtr,
    sfSession::SPtr sessionPtr,
    int depth)
{
    *outputPtr << "{";
    bool first = true;
    for (auto iter = dictPtr->begin(); iter != dictPtr->end(); ++iter)
    {
        if (first)
        {
            *outputPtr << "\n";
            first = false;
        }
        else
        {
            *outputPtr << ",\n";
        }
        // indentation
        for (int i = 0; i < depth + 1; i++)
        {
            *outputPtr << "  ";
        }
        *outputPtr << *iter->first << ": ";
        Write(outputPtr, iter->second, sessionPtr, depth + 1);
    }
    if (!first)
    {
        *outputPtr << "\n";
        // indentation
        for (int i = 0; i < depth; i++)
        {
            *outputPtr << "  ";
        }
    }
    *outputPtr << "}";
}

void sfWriter::WriteList(std::ostream* outputPtr, sfListProperty::SPtr listPtr, sfSession::SPtr sessionPtr, int depth)
{
    *outputPtr << "[";
    bool first = true;
    for (auto iter = listPtr->begin(); iter != listPtr->end(); ++iter)
    {
        if (!first)
        {
            *outputPtr << ", ";
        }
        first = false;
        Write(outputPtr, *iter, sessionPtr, depth + 1);
    }
    *outputPtr << "]";
}

void sfWriter::WriteValue(
    std::ostream* outputPtr,
    sfValueProperty::SPtr valuePtr,
    sfSession::SPtr sessionPtr)
{
    if (valuePtr->GetValue().GetType() == ksMultiType::UINT && sessionPtr != nullptr)
    {
        sfName name = sessionPtr->TryGetStringFromTable(valuePtr->GetValue());
        if (name.IsValid())
        {
            *outputPtr << "\"" << *name << "\" (" << valuePtr->ToString() << ")";
            return;
        }
    }
    else if (valuePtr->GetValue().GetType() == ksMultiType::STRING)
    {
        *outputPtr << "\"" << valuePtr->ToString() << "\"";
        return;
    }
    *outputPtr << valuePtr->ToString();
}

void sfWriter::WriteReference(
    std::ostream* outputPtr,
    sfReferenceProperty::SPtr referencePtr,
    sfSession::SPtr sessionPtr)
{
    *outputPtr << referencePtr->ToString();
}

void sfWriter::WriteNull(std::ostream* outputPtr, sfNullProperty::SPtr nullPtr, sfSession::SPtr sessionPtr)
{
    *outputPtr << nullPtr->ToString();
}