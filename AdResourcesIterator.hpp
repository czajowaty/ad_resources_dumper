#ifndef ADRESOURCESITERATOR_HPP
#define ADRESOURCESITERATOR_HPP

#include "AdDefinitions.hpp"
#include <QByteArray>
#include <utility>

struct AdResourceDescriptor
{
    ResourceHeader const* header;
    uint8_t const* data;
    uint32_t size;
};

class AdResourcesIterator
{
public:
    AdResourcesIterator(QByteArray const& resourcesData);

    bool hasNext() const;
    AdResourceDescriptor next();

private:
    ResourceHeader const* currentHeader() const;
    ResourceHeader const* moveHeader();

    uint8_t const* resourcesDataStart_;
    uint8_t const* resourcesDataEnd_;
    uint8_t const* resourceHeaderStart_;
};

#endif // ADRESOURCESITERATOR_HPP
