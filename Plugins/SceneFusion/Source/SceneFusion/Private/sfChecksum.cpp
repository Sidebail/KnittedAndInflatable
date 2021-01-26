#include "../Public/sfChecksum.h"
#include "../Public/SceneFusion.h"

using namespace KS;

uint64_t sfChecksum::Fletcher64(sfProperty::SPtr propertyPtr, Filter filter)
{
    // Fletcher-64 computes two 32-bit checksums and combines them to form a 64-bit checksum. The first is the modular
    // sum of each value, and the second is computed from the first by adding the first to the the second every time a
    // value is added to the first.
    uint64_t checksum1 = 0;
    uint64_t checksum2 = 0;
    Checksum(propertyPtr, checksum1, checksum2, filter);
    return checksum1 + (checksum2 << 32);
}

void sfChecksum::Checksum(uint32_t value, uint64_t& checksum1, uint64_t& checksum2)
{
    checksum1 += value;
    checksum1 %= UINT32_MAX;
    checksum2 += checksum1;
    checksum2 %= UINT32_MAX;
}

void sfChecksum::Checksum(sfProperty::SPtr propertyPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter)
{
    Checksum((uint32_t)propertyPtr->Type(), checksum1, checksum2);
    switch (propertyPtr->Type())
    {
        case sfProperty::DICTIONARY:
        {
            Checksum(propertyPtr->AsDict(), checksum1, checksum2, filter);
            break;
        }
        case sfProperty::LIST:
        {
            Checksum(propertyPtr->AsList(), checksum1, checksum2, filter);
            break;
        }
        case sfProperty::VALUE:
        {
            Checksum(propertyPtr->AsValue(), checksum1, checksum2);
            break;
        }
        case sfProperty::REFERENCE:
        {
            Checksum(propertyPtr->AsReference(), checksum1, checksum2);
            break;
        }
    }
}

void sfChecksum::Checksum(sfDictionaryProperty::SPtr dictPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter)
{
    // Dictionary key order is not defined, so get the keys and sort them
    TArray<uint32_t> nameIds;
    for (auto iter = dictPtr->begin(); iter != dictPtr->end(); ++iter)
    {
        // Use the filter to exclude keys we don't want
        if (filter == nullptr || filter(iter->first))
        {
            nameIds.Add(SceneFusion::Service->Session()->GetStringTableId(iter->first));
        }
    }
    nameIds.Sort();

    for (uint32_t id : nameIds)
    {
        Checksum(id, checksum1, checksum2);
        sfName name = SceneFusion::Service->Session()->GetStringFromTable(id);
        Checksum(dictPtr->Get(name), checksum1, checksum2, filter);
    }
}

void sfChecksum::Checksum(sfListProperty::SPtr listPtr, uint64_t& checksum1, uint64_t& checksum2, Filter filter)
{
    for (auto iter = listPtr->begin(); iter != listPtr->end(); ++iter)
    {
        Checksum(*iter, checksum1, checksum2, filter);
    }
}

void sfChecksum::Checksum(sfValueProperty::SPtr valuePtr, uint64_t& checksum1, uint64_t& checksum2)
{
    const ksMultiType& multiType = valuePtr->GetValue();
    Checksum((uint32_t)multiType.GetType(), checksum1, checksum2);
    if (multiType.IsArray())
    {
        Checksum(multiType.GetArrayLength(), checksum1, checksum2);
    }
    const std::vector<uint8_t>& data = multiType.GetData();
    const uint32_t* dataPtr = reinterpret_cast<const uint32_t*>(data.data());
    // Add each block of 4 bytes to the checksum
    int size = data.size() / 4;
    for (int i = 0; i < size; i++)
    {
        Checksum(dataPtr[i], checksum1, checksum2);
    }
    // Zero-pad the remaining bytes (if any) and add the result to the checksum
    int index = size * 4;
    if (index < data.size())
    {
        uint32_t value = 0;
        int n = 0;
        while (index < data.size())
        {
            value += data[index] << (8 * n);
            index++;
            n++;
        }
        Checksum(value, checksum1, checksum2);
    }
}

void sfChecksum::Checksum(sfReferenceProperty::SPtr referencePtr, uint64_t& checksum1, uint64_t& checksum2)
{
    Checksum(referencePtr->GetObjectId(), checksum1, checksum2);
}