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
    typedef std::shared_ptr<spdlog::logger> spdlog_t;

    struct log : public polymer::singleton<log>
    {
        const size_t qSize = 256;
        std::vector<spdlog::sink_ptr> sinks;
        spdlog_t engine_log, input_log, import_log;

        log()
        {
            spdlog::set_async_mode(qSize);
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>("polymer-engine-log.txt", true));
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>("polymer-input-log.txt", true));
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>("polymer-import-log.txt", true));
            engine_log = std::make_shared<spdlog::logger>("polymer-engine-log", sinks[0]);
            input_log = std::make_shared<spdlog::logger>("polymer-input-log", sinks[1]);
            import_log = std::make_shared<spdlog::logger>("polymer-import-log", sinks[2]);
        }

        void replace_sink(spdlog::sink_ptr sink)
        {
            sinks.push_back(sink);
            engine_log = std::make_shared<spdlog::logger>("polymer-engine-log", std::begin(sinks), std::end(sinks)); // fixme - this is not correct
        }

        friend class polymer::singleton<log>;
    };

    // Implement singleton
    template<> log * polymer::singleton<log>::single = nullptr;

} // end namespace polymer

#endif // end polymer_engine_log_hpp
