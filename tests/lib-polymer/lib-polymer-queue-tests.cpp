#include "lib-polymer.hpp"

using namespace polymer;

#include "doctest.h"

struct queue_accumulator
{
    int32_t accumulator{ -1 };
};

TEST_CASE("test mpsc_queue_bounded (size 1024)")
{
    scoped_timer t("mpsc_queue_bounded (size 1024)");
    mpsc_queue_bounded<queue_accumulator, 1024> mpsc_queue1024;
    std::vector<std::thread> threads;

    REQUIRE(mpsc_queue1024.size() == 0);
    REQUIRE(mpsc_queue1024.empty());

    // 256 total messages
    const int32_t num_producers = 8;
    const int32_t num_events = 32;

    for (int32_t i = 0; i < num_producers; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int32_t j = 0; j < num_events; ++j)
            {
                queue_accumulator qa{ num_events };
                bool result = mpsc_queue1024.emplace_back(qa);
                REQUIRE(result);
            }
        });
    }

    std::thread consumer_thread([&]()
    {
        // Flush
        while(!mpsc_queue1024.empty())
        { 
            auto result = mpsc_queue1024.pop_front();
            REQUIRE(result.first);
            REQUIRE(result.second.accumulator != -1);
        }
    });

    for (auto & t : threads) t.join();
    consumer_thread.join();
}

TEST_CASE("test mpsc_queue_bounded (size 512)")
{
    scoped_timer t("mpsc_queue_bounded (size 512)");
    mpsc_queue_bounded<queue_accumulator, 512> mpsc_queue512;
    std::vector<std::thread> threads;

    // 2048 total messages
    const int32_t num_producers = 64;
    const int32_t num_events = 32;
    std::atomic<int32_t> producer_index = 0;

    for (int32_t i = 0; i < num_producers; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int32_t j = 0; j < num_events; ++j)
            {
                queue_accumulator qa{ num_events };
                const bool result = mpsc_queue512.emplace_back(qa);
                if (producer_index >= 512) REQUIRE_FALSE(result);
                else REQUIRE(result);
                producer_index++;
            }
        });
    }

    uint32_t index = 0;
    std::thread consumer_thread([&]()
    {
        // Flush
        while (!mpsc_queue512.empty())
        {
            auto result = mpsc_queue512.pop_front();

            if (index >= 512)
            {
                REQUIRE_FALSE(result.first);
                REQUIRE(result.second.accumulator == -1);
            }
            else
            {
                REQUIRE(result.first);
                REQUIRE(result.second.accumulator != -1);
            }

            index++;
        }
    });

    for (auto & t : threads) t.join();
    consumer_thread.join();
}

/*
TEST_CASE("test mpsc_queue_bounded (timed test, size 64)")
{
    manual_timer t;

    mpsc_queue_bounded<queue_accumulator, 64> mpsc_queue64;
    std::vector<std::thread> threads;

    const int32_t num_producers = 2;
    const int32_t num_events = 1024;

    t.start();

    for (int32_t i = 0; i < num_producers; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int32_t j = 0; j < num_events; ++j)
            {
                queue_accumulator qa{ j };
                const bool result = mpsc_queue64.emplace_back(qa);
                CHECK(result);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    int32_t consumed = 0;
    std::thread consumer_thread([&]()
    {
        // Flush approximately 10 times faster
        while (true)
        {
            if (!mpsc_queue64.empty())
            {
                auto result = mpsc_queue64.pop_front();
                consumed++;
                CHECK(result.first); // this is okay to fail sometimes
            }

            if (consumed == 2048)
            {
                std::cout << "Queue Finished @ " << t.running() << std::endl;
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    for (auto & t : threads) t.join();
    consumer_thread.join();
}
*/