//
// Created by niooi on
// 7/24/25.
//

#include <prelude.h>
#include <profile.h>
#include <rand.h>
#include <time/time.h>

#include <absl/container/btree_set.h>
#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <cctype>
#include <cstdlib>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <string_view>
#include "engine/contexts/async/async.h"
#include "spdlog/common.h"

void init_loggers();

void v::init(const char* argv0)
{
    // enables tracy on static lib linkage
    TracyNoop;

    // TODO! this fails on windows i guess
    // absl::InitializeSymbolizer(argv0);
    absl::FailureSignalHandlerOptions fail_opts = { .symbolize_stacktrace = true };

    absl::InstallFailureSignalHandler(fail_opts);

    init_loggers();

    // INit engine subsystems
    time::init();
    rand::init();
}

void init_loggers()
{
    // Init loggers

    // Console logger
    const auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // Daily logger - a new file is created every day at 12 am
    const auto file_sink =
        std::make_shared<spd::sinks::daily_file_sink_mt>("./logs/log", 0, 0);

    const spd::sinks_init_list sinks = { stdout_sink, file_sink };

    const auto logger = std::make_shared<spd::logger>("", sinks.begin(), sinks.end());

    // defualt level
    logger->set_level(spd::level::trace);

    logger->flush_on(spd::level::err);

    set_default_logger(logger);

    // logging env override: V_LOG_LEVEL=trace|debug|info|warn|error|critical|off
    if (const char* env = std::getenv("V_LOG_LEVEL"))
    {
        auto s        = std::string_view(env);
        auto to_lower = [](char c)
        { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        std::string level_str;
        level_str.reserve(s.size());
        for (char c : s)
            level_str.push_back(to_lower(c));

        spd::level::level_enum lvl = spd::level::trace;
        if (level_str == "trace")
            lvl = spd::level::trace;
        else if (level_str == "debug")
            lvl = spd::level::debug;
        else if (level_str == "info")
            lvl = spd::level::info;
        else if (level_str == "warn" || level_str == "warning")
            lvl = spd::level::warn;
        else if (level_str == "error")
            lvl = spd::level::err;
        else if (level_str == "critical" || level_str == "fatal")
            lvl = spd::level::critical;
        else if (level_str == "off" || level_str == "none")
            lvl = spd::level::off;

        spd::set_level(lvl);
        logger->set_level(lvl);
    }
}
