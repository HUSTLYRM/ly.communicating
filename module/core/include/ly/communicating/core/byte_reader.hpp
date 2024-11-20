#pragma once

#include <memory>
#include <concepts>
#include <optional>

#include "basic_bytes.hpp"

namespace ly::communicating
{
    bool reader_loop_once(auto& reader, auto& packer, auto& package)
    {
        auto& buffer = buffer(package);
        const auto bytes = reader.read(buffer);
        if (reader.read(buffer) != sizeof(package))
        {
            //handle exception
            return false;
        }
        if (!packer.pack(package))
        {
            //handle exception
            return false;
        }
        return true;
    }

    template<typename object_type>
    concept buffer_source = requires(object_type & object)
    {
        { object.get() } -> std::same_as<byte_span>;
    };

    template<template<typename> typename object_type>
    template<typename package_type>
    concept packer = requires(object_type<package_type> &object, const_byte_span data, package_type & package)
    {
        { object.pack(data, package) } -> std::same_as<bool>;
    };

    template<template<typename> typename object_type>
    template<typename package_type>
    concept package_destination = requires(object_type<package_type> &object, const package_type & package)
    {
        { object.set(package) };
    };

    template<typename T>
    concept is_reader = requires(T & t, byte_span buffer)
    {
        { t.read(buffer) } -> std::same_as<size_type>;
    };


    class byte_reader
    {
    public:
        virtual ~byte_reader() = default;

        [[nodiscard]] virtual size_type read(byte_span buffer) = 0;
    };

    class byte_reader_provider
    {
    public:
        using reader_type = byte_reader;

        virtual ~byte_reader_provider() = default;

        [[nodiscard]] virtual std::optional<std::shared_ptr<reader_type> > get_reader() = 0;
    };

    template<typename T, typename reader_type = typename T::reader_type>
    concept is_reader_provider = is_reader<reader_type> && requires(T & t)
    {
        { t.get_reader() } -> std::same_as<std::optional<std::shared_ptr<reader_type> > >;
    };
}
