#pragma once

#ifndef polymer_engine_log_hpp
#define polymer_engine_log_hpp

#include "spdlog/spdlog.h"
#include "spdlog/sinks/ostream_sink.h"
#include "util.hpp"

namespace spd = spdlog;
namespace spdlog { class log; };

namespace polymer
{
    typedef std::shared_ptr<spdlog::logger> LogT;

    struct log : public polymer::singleton<log>
    {
        const size_t qSize = 256;
        std::vector<spdlog::sink_ptr> sinks;
        LogT assetLog;

        log()
        {
            spdlog::set_async_mode(qSize);
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>("polymer-engine-log.txt"));
            assetLog = std::make_shared<spdlog::logger>("polymer-engine-log", std::begin(sinks), std::end(sinks));
        }

        void replace_sink(spdlog::sink_ptr sink)
        {
            sinks.push_back(sink);
            assetLog = std::make_shared<spdlog::logger>("polymer-engine-log", std::begin(sinks), std::end(sinks));
        }

        friend class polymer::singleton<log>;
    };

    // Implement singleton
    template<> log * polymer::singleton<log>::single = nullptr;

} // end namespace polymer

#endif // end polymer_engine_log_hpp
