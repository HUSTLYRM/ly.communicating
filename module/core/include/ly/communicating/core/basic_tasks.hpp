#pragma once

#include <concepts>

#include "basic_bytes.hpp"

namespace ly::communicating {
    template<typename object_type>
    concept is_byte_reader = requires(object_type &object, byte_span buffer) {
        { object.read(buffer) } -> std::same_as<bool>;
    };

    template<typename object_type>
    concept is_byte_writer = requires(object_type &object, byte_span buffer) {
        { object.write(buffer) } -> std::same_as<bool>;
    };

    template<typename object_type, typename item_type = typename object_type::item_type>
    concept is_byte_packer = requires(object_type &object, byte_span buffer, item_type &item) {
        { object.pack(buffer, item) } -> std::same_as<bool>;
    };

    template<typename object_type, typename item_type = typename object_type::item_type>
    concept is_item_unpacker = requires(object_type &object, const item_type &item, byte_span buffer) {
        { object.unpack(item, buffer) } -> std::same_as<bool>;
    };

    template<typename object_type, typename item_type = typename object_type::item_type>
    concept is_item_sink = requires(object_type &object, const item_type &item) {
        { object.set(item) } -> std::same_as<bool>;
    };

    template<typename object_type, typename item_type = typename object_type::item_type>
    concept is_item_source = requires(object_type &object, item_type &item) {
        { object.get(item) } -> std::same_as<bool>;
    };

    template<typename object_type>
    concept is_result_monitor = requires(object_type &object, int result) {
        { object.handle(result) } -> std::same_as<bool>;
    };

    enum basic_task_failure : int {
        reader_failure = -1,
        packer_failure = -2,
        sink_failure = -3,
        writer_failure = -1,
        unpacker_failure = -2,
        source_failure = -3
    };


    template<
        typename reader_type,
        typename packer_type,
        typename sink_type>
        requires (std::is_same_v<typename packer_type::item_type, typename sink_type::item_type>)
                 && is_byte_reader<reader_type>
                 && is_byte_packer<packer_type>
                 && is_item_sink<sink_type>
    class reader_task {
        using item_type = typename reader_type::item_type;
        std::shared_ptr<reader_type> reader;
        std::shared_ptr<packer_type> packer;
        std::shared_ptr<sink_type> sink;
        byte_array<sizeof(item_type)> buffer;
        item_type item;

    public:
        reader_task(std::shared_ptr<reader_type> reader,
            std::shared_ptr<packer_type> packer,
            std::shared_ptr<sink_type> sink) :
            reader(reader), packer(packer), sink(sink) {}

        int run_once() noexcept {
            if (!reader->read(buffer)) return reader_failure;
            if (!packer->pack(buffer, item)) return packer_failure;
            if (!sink->set(item)) return sink_failure;
            return 0;
        }

        int operator()() noexcept { return run_once(); }
    };

    template<
        is_byte_reader reader_type,
        is_byte_packer packer_type,
        is_item_sink sink_type,
        is_result_monitor monitor_type>
    class monitored_reader_task {
        reader_task<reader_type, packer_type, sink_type> task;
        std::shared_ptr<monitor_type> monitor;

    public:
        monitored_reader_task(std::shared_ptr<reader_type> reader,
            std::shared_ptr<packer_type> packer,
            std::shared_ptr<sink_type> sink, std::shared_ptr<monitor_type> monitor) :
            task(reader, packer, sink), monitor(monitor) {}

        void run() noexcept { while (monitor->handle(task.run_once())); }

        void operator()() noexcept { run(); }
    };

    template<
        typename writer_type,
        typename unpacker_type,
        typename source_type>
        requires (std::is_same_v<typename unpacker_type::item_type, typename source_type::item_type>)
                 && is_byte_writer<writer_type>
                 && is_item_unpacker<unpacker_type>
                 && is_item_source<source_type>
    class writer_task {
        using item_type = typename source_type::item_type;
        std::shared_ptr<writer_type> writer;
        std::shared_ptr<unpacker_type> unpacker;
        std::shared_ptr<source_type> source;
        byte_array<sizeof(item_type)> buffer;
        item_type item;

    public:
        writer_task(std::shared_ptr<writer_type> writer,
            std::shared_ptr<unpacker_type> unpacker,
            std::shared_ptr<source_type> source) :
            writer(writer), unpacker(unpacker), source(source) {}

        int run_once() noexcept {
            if (!source->get(item)) return source_failure;
            if (!unpacker->unpack(item, buffer)) return unpacker_failure;
            if (!writer->write(buffer)) return writer_failure;
            return 0;
        }

        int operator()() noexcept { return run_once(); }
    };

    template<
        is_item_source source_type,
        is_byte_writer writer_type,
        is_item_unpacker unpacker_type,
        is_result_monitor monitor_type>
    class monitored_writer_task {
        writer_task<writer_type, unpacker_type, source_type> task;
        std::shared_ptr<monitor_type> monitor;

    public:
        monitored_writer_task(std::shared_ptr<writer_type> writer,
            std::shared_ptr<unpacker_type> unpacker,
            std::shared_ptr<source_type> source, std::shared_ptr<monitor_type> monitor) :
            task(writer, unpacker, source), monitor(monitor) {}

        void run() noexcept { while (monitor->handle(task.run_once())); }
        void operator()() noexcept { run(); }
    };
}
