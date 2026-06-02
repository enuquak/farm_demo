#include "route_cache.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

void test_update_and_query() {
    farm::RouteCache cache;

    cache.update(1001, 1);
    cache.update(1002, 2);

    auto result1 = cache.get_server_id(1001);
    assert(result1.has_value());
    assert(result1.value() == 1);

    auto result2 = cache.get_server_id(1002);
    assert(result2.has_value());
    assert(result2.value() == 2);

    // 不存在的玩家
    auto result3 = cache.get_server_id(9999);
    assert(!result3.has_value());

    std::cout << "test_update_and_query PASSED" << std::endl;
}

void test_remove() {
    farm::RouteCache cache;

    cache.update(1001, 1);
    auto result = cache.get_server_id(1001);
    assert(result.has_value());

    cache.remove(1001);
    auto result2 = cache.get_server_id(1001);
    assert(!result2.has_value());

    std::cout << "test_remove PASSED" << std::endl;
}

void test_update_overwrite() {
    farm::RouteCache cache;

    cache.update(1001, 1);
    assert(cache.get_server_id(1001).value() == 1);

    // 玩家转移到另一个服务器
    cache.update(1001, 2);
    assert(cache.get_server_id(1001).value() == 2);

    std::cout << "test_update_overwrite PASSED" << std::endl;
}

void test_clear() {
    farm::RouteCache cache;

    cache.update(1001, 1);
    cache.update(1002, 2);
    cache.update(1003, 3);
    assert(cache.size() == 3);

    cache.clear();
    assert(cache.size() == 0);
    assert(!cache.get_server_id(1001).has_value());
    assert(!cache.get_server_id(1002).has_value());
    assert(!cache.get_server_id(1003).has_value());

    std::cout << "test_clear PASSED" << std::endl;
}

void test_clear_server() {
    farm::RouteCache cache;

    cache.update(1001, 1);
    cache.update(1002, 1);
    cache.update(2001, 2);
    cache.update(2002, 2);
    cache.update(3001, 3);
    assert(cache.size() == 5);

    cache.clear_server(2);
    assert(cache.size() == 3);

    // Server 1 routes still exist
    assert(cache.get_server_id(1001).has_value());
    assert(cache.get_server_id(1001).value() == 1);
    assert(cache.get_server_id(1002).has_value());
    assert(cache.get_server_id(1002).value() == 1);

    // Server 2 routes removed
    assert(!cache.get_server_id(2001).has_value());
    assert(!cache.get_server_id(2002).has_value());

    // Server 3 route still exists
    assert(cache.get_server_id(3001).has_value());
    assert(cache.get_server_id(3001).value() == 3);

    std::cout << "test_clear_server PASSED" << std::endl;
}

void test_concurrent() {
    farm::RouteCache cache;
    const int NUM_THREADS = 4;
    const int NUM_OPS = 1000;

    // 并发写入
    std::vector<std::thread> writers;
    for (int t = 0; t < NUM_THREADS; t++) {
        writers.emplace_back([&cache, t]() {
            for (int i = 0; i < NUM_OPS; i++) {
                uint64_t player_id = t * NUM_OPS + i;
                cache.update(player_id, t);
            }
        });
    }
    for (auto& t : writers) t.join();

    // 验证所有数据
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < NUM_OPS; i++) {
            uint64_t player_id = t * NUM_OPS + i;
            auto result = cache.get_server_id(player_id);
            assert(result.has_value());
            assert(result.value() == static_cast<uint32_t>(t));
        }
    }

    // 并发读写
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int t = 0; t < NUM_THREADS; t++) {
        readers.emplace_back([&cache, &stop]() {
            while (!stop) {
                cache.get_server_id(0);
            }
        });
    }

    std::vector<std::thread> writers2;
    for (int t = 0; t < NUM_THREADS; t++) {
        writers2.emplace_back([&cache, &stop, t]() {
            for (int i = 0; i < NUM_OPS; i++) {
                cache.update(10000 + i, t);
            }
        });
    }
    for (auto& t : writers2) t.join();
    stop = true;
    for (auto& t : readers) t.join();

    std::cout << "test_concurrent PASSED" << std::endl;
}

int main() {
    test_update_and_query();
    test_remove();
    test_update_overwrite();
    test_clear();
    test_clear_server();
    test_concurrent();
    std::cout << "All RouteCache tests PASSED" << std::endl;
    return 0;
}
