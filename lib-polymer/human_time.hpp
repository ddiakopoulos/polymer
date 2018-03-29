#ifndef human_time_h
#define human_time_h

#include <string.h>
#include <chrono>
#include <ctime>

namespace polymer
{

    struct HumanTime
    {

        typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> days;

        int year;
        int month;
        int yearDay;
        int monthDay;
        int weekDay;
        int hour;
        int minute;
        int second;
        int isDST;

        HumanTime() 
        { 
            update();
        }

        void update()
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

        std::string make_timestamp()
        {
            std::string timestamp =
                std::to_string(month) + "." +
                std::to_string(monthDay) + "." +
                std::to_string(year) + "-" +
                std::to_string(hour) + "." +
                std::to_string(minute) + "." +
                std::to_string(second);
            return timestamp;
        }
    };

}

#endif // human_time_h