#include "AdDefinitions.hpp"

GraphicFlags operator|(GraphicFlags one, GraphicFlags other)
{
    using T = std::underlying_type<GraphicFlags>::type;
    return static_cast<GraphicFlags>(
                static_cast<T>(one) | static_cast<T>(other));
}

GraphicFlags operator&(GraphicFlags one, GraphicFlags other)
{
    using T = std::underlying_type<GraphicFlags>::type;
    return static_cast<GraphicFlags>(
                static_cast<T>(one) & static_cast<T>(other));
}

GraphicFlags operator^(GraphicFlags one, GraphicFlags other)
{
    using T = std::underlying_type<GraphicFlags>::type;
    return static_cast<GraphicFlags>(
                static_cast<T>(one) ^ static_cast<T>(other));
}
