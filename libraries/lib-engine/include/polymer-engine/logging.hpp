#pragma once

#ifndef polymer_engine_log_hpp
#define polymer_engine_log_hpp

#include "spdlog/spdlog.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/wincolor_sink.h"

#include "polymer-core/util/util.hpp"

namespace spd = spdlog;
namespace spdlog { class log; };

namespace polymer
{
    typedef std::shared_ptr<spdlog::logger> spdlog_t;

    struct log : public polymer::singleton<log>
    {
        std::vector<spdlog::sink_ptr> sinks;
        spdlog_t engine_log, input_log, import_log;

        log()
        {
            //spdlog::set_async_mode(64);

            sinks.push_back(std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>());

            engine_log = std::make_shared<spdlog::logger>("polymer-engine-log", sinks[0]);
            input_log = std::make_shared<spdlog::logger>("polymer-input-log", sinks[0]);
            import_log = std::make_shared<spdlog::logger>("polymer-import-log", sinks[0]);
        }

        void set_engine_logger(spdlog::sink_ptr sink)
        {
            engine_log = std::make_shared<spdlog::logger>("polymer-engine-log", sink);
        }

        friend class polymer::singleton<log>;
    };

    // Implement singleton
    template<> log * polymer::singleton<log>::single = nullptr;

} // end namespace polymer

#endif // end polymer_engine_log_hpp
