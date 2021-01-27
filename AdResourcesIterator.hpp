#ifndef ADRESOURCESITERATOR_HPP
#define ADRESOURCESITERATOR_HPP

#include "AdDefinitions.hpp"
#include <QByteArray>
#include <utility>

class AdResourcesIterator
{
public:
    AdResourcesIterator(QByteArray const& resourcesData);

    bool hasNext() const;
    std::pair<ResourceHeader const*, uint8_t const*> next();

private:
    ResourceHeader const* currentHeader() const;

    uint8_t const* resourcesDataStart_;
    uint8_t const* resourceHeaderStart_;
};

#endif // ADRESOURCESITERATOR_HPP
