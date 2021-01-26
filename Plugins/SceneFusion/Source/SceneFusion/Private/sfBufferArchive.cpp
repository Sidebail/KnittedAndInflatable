#include "../Public/sfBufferArchive.h"
#include "../Public/SceneFusion.h"
#include "../Public/sfObjectMap.h"
#include "../Public/sfLoader.h"
#include "../Public/sfUnrealUtils.h"

#define LOG_CHANNEL "sfBufferArchive"

namespace
{
    // Object types
    enum ObjectType : uint8_t
    {
        NUL = 0,        // the object is null
        UNSYNCED = 1,   // the object cannot be serialized. When deserializing, the reference will be unchanged.
        LEVEL = 2,      // the object is in the level
        ASSET = 3       // the object is an asset 
    };
}

FArchive& sfBufferArchive::operator<<(UObject*& uobjPtr)
{
    if (uobjPtr == nullptr)
    {
        uint8_t type = ObjectType::NUL;
        *this << type;
    }
    else if (uobjPtr->GetTypedOuter<ULevel>() != nullptr)
    {
        // The object is in the level
        sfObject::SPtr objPtr = sfObjectMap::GetSFObject(uobjPtr);
        if (objPtr == nullptr)
        {
            FString str = "Unable to serialize reference to unsynced " + uobjPtr->GetClass()->GetName() + " " +
                uobjPtr->GetName();
            KS::Log::Warning(TCHAR_TO_UTF8(*str), LOG_CHANNEL);
            uint8_t type = ObjectType::UNSYNCED;
            *this << type;
        }
        else
        {
            uint8_t type = ObjectType::LEVEL;
            *this << type;
            uint32_t id = objPtr->Id();
            *this << id;
        }
    }
    else if (uobjPtr->GetOutermost() == GetTransientPackage() || uobjPtr->HasAllFlags(RF_Transient))
    {
        // This may be a stand-in for a missing asset
        FString path = sfLoader::Get().GetPathFromStandIn(uobjPtr);
        if (path.IsEmpty())
        {
            FString str = "Unable to serialize reference to " + uobjPtr->GetClass()->GetName() + " " +
                uobjPtr->GetName() + " in the transient package.";
            KS::Log::Warning(TCHAR_TO_UTF8(*str), LOG_CHANNEL);
            uint8_t type = ObjectType::UNSYNCED;
            *this << type;
        }
        else
        {
            uint8_t type = ObjectType::ASSET;
            *this << type;
            uint32_t pathId = 0;
            if (SceneFusion::Get().Service->Session() != nullptr)
            {
                pathId = SceneFusion::Get().Service->Session()->GetStringTableId(TCHAR_TO_UTF8(*path));
                m_missingPathIds.Add(pathId);
            }
            *this << pathId;
        }
    }
    else
    {
        uint8_t type = ObjectType::ASSET;
        *this << type;
        FString path = sfUnrealUtils::ClassToFString(uobjPtr->GetClass()) + ";" + uobjPtr->GetPathName();
        uint32_t pathId = 0;
        if (SceneFusion::Get().Service->Session() != nullptr)
        {
            pathId = SceneFusion::Get().Service->Session()->GetStringTableId(TCHAR_TO_UTF8(*path));
        }
        *this << pathId;
    }
    return *this;
}

const TSet<uint32_t>& sfBufferArchive::MissingPathIds()
{
    return m_missingPathIds;
}

sfBufferReader::sfBufferReader(void* data, int64_t size) : 
    FBufferReaderBase{ data, size, false }
{

}

FArchive& sfBufferReader::operator<<(UObject*& uobjPtr)
{
    uint8_t type;
    *this << type;
    switch (type)
    {
        case ObjectType::NUL:
        {
            uobjPtr = nullptr;
            break;
        }
        case ObjectType::LEVEL:
        {
            uint32_t id;
            *this << id;
            if (SceneFusion::Service->Session() == nullptr)
            {
                break;
            }
            sfObject::SPtr objPtr = SceneFusion::Service->Session()->GetObject(id);
            UObject* referencePtr = sfObjectMap::GetUObject(objPtr);
            if (referencePtr == nullptr)
            {
                KS::Log::Warning("Unable to deserialize object reference.", LOG_CHANNEL);
            }
            else
            {
                uobjPtr = referencePtr;
            }
            break;
        }
        case ObjectType::ASSET:
        {
            uint32_t pathId;
            *this << pathId;
            if (SceneFusion::Get().Service->Session() == nullptr)
            {
                break;
            }
            FString str = FString(SceneFusion::Get().Service->Session()->GetStringFromTable(pathId)->c_str());
            FString path, className;
            if (!str.Split(";", &className, &path))
            {
                KS::Log::Warning("Invalid asset string: " + std::string(TCHAR_TO_UTF8(*str)), LOG_CHANNEL);
                break;
            }
            uobjPtr = sfLoader::Get().Load(path, className);
            if (uobjPtr->HasAllFlags(RF_Transient))
            {
                // This is a stand-in for a missing asset
                m_missingPathIds.Add(pathId);
            }
            break;
        }
    }
    return *this;
}

FArchive& sfBufferReader::operator<<(FName& name)
{
    FString str;
    *this << str;
    name = *str;
    return *this;
}

const TSet<uint32_t>& sfBufferReader::MissingPathIds()
{
    return m_missingPathIds;
}

#undef LOG_CHANNEL