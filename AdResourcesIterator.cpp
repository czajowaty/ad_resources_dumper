#include "AdResourcesIterator.hpp"

AdResourcesIterator::AdResourcesIterator(QByteArray const& resourcesData)
    : resourcesDataStart_{
          reinterpret_cast<uint8_t const*>(resourcesData.constData())},
      resourcesDataEnd_{
          reinterpret_cast<uint8_t const*>(resourcesData.constEnd())},
      resourceHeaderStart_{resourcesDataStart_}
{}

bool AdResourcesIterator::hasNext() const
{ return currentHeader()->type != ResourceType::None; }

AdResourceDescriptor AdResourcesIterator::next()
{
    AdResourceDescriptor nextDescriptor;
    nextDescriptor.header = moveHeader();
    nextDescriptor.data =
            resourcesDataStart_ + nextDescriptor.header->dataStartOffset;
    if (hasNext())
    {
        nextDescriptor.size =
                currentHeader()->dataStartOffset -
                nextDescriptor.header->dataStartOffset;
    }
    else
    { nextDescriptor.size = resourcesDataEnd_ - nextDescriptor.data; }
    return nextDescriptor;
}

ResourceHeader const* AdResourcesIterator::currentHeader() const
{ return reinterpret_cast<ResourceHeader const*>(resourceHeaderStart_); }

ResourceHeader const* AdResourcesIterator::moveHeader()
{
    auto resourceHeader = currentHeader();
    resourceHeaderStart_ += resourceHeader->headerSize;
    return resourceHeader;
}
