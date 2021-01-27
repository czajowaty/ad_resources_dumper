#include "AdResourcesIterator.hpp"

AdResourcesIterator::AdResourcesIterator(QByteArray const& resourcesData)
    : resourcesDataStart_{reinterpret_cast<uint8_t const*>(resourcesData.constData())},
      resourceHeaderStart_{resourcesDataStart_}
{}

bool AdResourcesIterator::hasNext() const
{ return currentHeader()->type != ResourceType::None; }

std::pair<ResourceHeader const*, uint8_t const*> AdResourcesIterator::next()
{
    auto const* nextResourceHeader = currentHeader();
    resourceHeaderStart_ += nextResourceHeader->headerSize;
    auto const* resourceData =
            resourcesDataStart_ + nextResourceHeader->dataStartOffset;
    return {nextResourceHeader, resourceData};
}

ResourceHeader const* AdResourcesIterator::currentHeader() const
{ return reinterpret_cast<ResourceHeader const*>(resourceHeaderStart_); }
