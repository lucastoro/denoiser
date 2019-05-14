#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <unordered_set>

class thread_pool final {
public:
  using function_t = std::function<void()>;
  using id_t = uint64_t;

  thread_pool(size_t threads = std::thread::hardware_concurrency())
    : workers(0), stop(false), id_counter(0) {
    unique_lock lock(mutex);
    pool.reserve(threads);
    for (size_t i = 0; i < threads; ++i ) {
      pool.emplace_back([this](){ this->run(); });
    }

    while (workers != threads) {
      cond.wait(lock);
    }
  }
  ~thread_pool() {
    unique_lock lock(mutex);
    stop = true;
    cond.notify_all();
    while (workers != 0) {
      cond.wait(lock);
    }

    for (auto& thread : pool) {
      thread.join();
    }
  }

  id_t submit(function_t&& func) {
    unique_lock lock(mutex);
    ++id_counter;
    ids.insert(id_counter);
    queue.emplace(id_counter, std::move(func));
    cond.notify_all();
    return id_counter;
  }

  void wait(id_t id) {
    unique_lock lock(mutex);
    while (ids.count(id)) {
      cond.wait(lock);
    }
  }

  template <typename container>
  typename std::enable_if<not std::is_same<id_t, container>::value>::type
  wait(const container& cont) {
    for (const id_t id : cont) {
      wait(id);
    }
  }

private:

  using lock_guard = std::lock_guard<std::mutex>;
  using unique_lock= std::unique_lock<std::mutex>;

  void run() {
    unique_lock lock(mutex);

    ++workers;
    cond.notify_all();

    while (not stop) {

      while (not stop and queue.empty()) {
        cond.wait(lock);
      }

      while (not queue.empty()) {
        auto job = std::move(queue.front());
        queue.pop();
        lock.unlock();
        job.second();
        lock.lock();
        ids.erase(job.first);
        cond.notify_all();
      }
    }

    --workers;
    cond.notify_all();
  }

  std::vector<std::thread> pool;
  std::unordered_set<id_t> ids;
  std::queue<std::pair<id_t, function_t>> queue;
  std::mutex mutex;
  std::condition_variable cond;
  size_t workers;
  bool stop;
  id_t id_counter;
};
