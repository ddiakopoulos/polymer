#pragma once

#ifndef polymer_timestamp_hpp
#define polymer_timestamp_hpp

#include <string>
#include <chrono>
#include <ctime>

namespace polymer
{

    struct polymer_time_point
    {
        int year;
        int month;
        int yearDay;
        int monthDay;
        int weekDay;
        int hour;
        int minute;
        int second;
        int isDST;

        polymer_time_point()
        { 
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            tm local_tm = *localtime(&tt);
            year = local_tm.tm_year + 1900;
            month = local_tm.tm_mon + 1;
            monthDay = local_tm.tm_mday;
            yearDay = local_tm.tm_yday;
            weekDay = local_tm.tm_wday;
            hour = local_tm.tm_hour;
            minute = local_tm.tm_min;
            second = local_tm.tm_sec;
            isDST = local_tm.tm_isdst;
        }
    };

    inline std::string make_timestamp()
    {
        polymer_time_point t = {};

        std::string timestamp =
            std::to_string(t.month) + "." +
            std::to_string(t.monthDay) + "." +
            std::to_string(t.year) + "-" +
            std::to_string(t.hour) + "." +
            std::to_string(t.minute) + "." +
            std::to_string(t.second);

        return timestamp;
    }

}

#endif // end polymer_timestamp_hpp