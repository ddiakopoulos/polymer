#pragma once

#ifndef render_logging_hpp
#define render_logging_hpp

#include "spdlog/spdlog.h"
#include "spdlog/sinks/ostream_sink.h"
#include "util.hpp"

namespace spd = spdlog;
namespace spdlog { class logger; };
typedef std::shared_ptr<spdlog::logger> Log;

struct Logger : public polymer::Singleton<Logger>
{
    const size_t qSize = 256;
    std::vector<spdlog::sink_ptr> sinks;
    Log assetLog;

    Logger()
    {
        spdlog::set_async_mode(qSize);
        sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>("asset_log.txt"));
        assetLog = std::make_shared<spdlog::logger>("asset_log", std::begin(sinks), std::end(sinks));
        //sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
    }

    void add_sink(spdlog::sink_ptr sink)
    {
        sinks.push_back(sink);
        assetLog = std::make_shared<spdlog::logger>("asset_log", std::begin(sinks), std::end(sinks));
    }

    friend class polymer::Singleton<Logger>;
};

// Implement singleton
template<> Logger * polymer::Singleton<Logger>::single = nullptr;

#endif // end render_logging_hpp