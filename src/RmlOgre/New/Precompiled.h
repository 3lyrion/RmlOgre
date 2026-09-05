#pragma once

#include <RmlUi/Core.h>
#include <Ogre.h>

#include <typeindex>
#include <type_traits>

namespace RmlOgre
{

using uchar  = uint8_t;
using uint   = uint32_t;
using int8   = int8_t;
using int16  = int16_t;
using int32  = int32_t;
using int64  = int64_t;
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

template <typename T>
using Hash = robin_hood::hash<T>; // RmlUi

inline constexpr Hash<std::string_view> StringHasher;

using String = std::string;

template <typename T, typename Del = std::default_delete<T> >
using UniquePtr = std::unique_ptr<T, Del>;

template <typename T, typename Del = std::default_delete<T> >
using UPtr = UniquePtr<T, Del>;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
using SPtr = SharedPtr<T>;

template <typename T>
using Vector = std::vector<T>;

template <typename KeyT, typename ValT>
using FlatMap = itlib::flat_map<KeyT, ValT, std::less<KeyT>>; // RmlUi

template <typename KeyT, typename ValT>
using UnorderedMap = robin_hood::unordered_node_map<KeyT, ValT>; // RmlUi

template <typename KeyT, typename ValT>
using UMap = UnorderedMap<KeyT, ValT>;

using TypeInfo = std::type_info;

using TypeIndex = std::type_index;

template <typename MapT, typename KeyT>
constexpr auto find(MapT& map, KeyT&& key) -> typename MapT::mapped_type*
{
    auto entry = map.find(std::forward<KeyT>(key));
    if (entry == map.end())
        return nullptr;

    return &entry->second;
}

template <typename MapT, typename KeyT>
constexpr auto find(MapT const& map, KeyT&& key) -> typename MapT::mapped_type const*
{
    auto entry = map.find(std::forward<KeyT>(key));
    if (entry == map.end())
        return nullptr;

    return &entry->second;
}

} // namespace RmlOgre
