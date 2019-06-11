#include "thread-pool.hpp"

size_t thread_pool::max_threads = 0;

thread_pool::thread_pool(size_t threads)
  : workers(0), stop(false), id_counter(0) {
  unique_lock lock(mutex);

  if (0 == threads) {
    threads = max_threads ? max_threads : std::thread::hardware_concurrency();
  }

  pool.reserve(threads);
  for (size_t i = 0; i < threads; ++i ) {
    pool.emplace_back([this](){ this->run(); });
  }

  while (workers != threads) {
    cond.wait(lock);
  }
}

thread_pool::~thread_pool() {
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

thread_pool::id_t thread_pool::submit(function_t&& func) {
  unique_lock lock(mutex);
  ++id_counter;
  ids.insert(id_counter);
  queue.emplace(id_counter, std::move(func));
  cond.notify_all();
  return id_counter;
}

void thread_pool::wait(id_t id) {
  unique_lock lock(mutex);
  while (ids.count(id)) {
    cond.wait(lock);
  }
}

/**
 * the workers loop
*/
void thread_pool::run() {
  unique_lock lock(mutex);

  ++workers;
  cond.notify_all();

  while (not stop) {

    while (not stop and queue.empty()) {
      cond.wait(lock);
    }

    while (not queue.empty()) {
      const auto job = std::move(queue.front());
      queue.pop();
      lock.unlock();
      job.func();
      lock.lock();
      ids.erase(job.id);
      cond.notify_all();
    }
  }

  --workers;
  cond.notify_all();
}

void thread_pool::set_max_threads(size_t t) {
  max_threads = t;
}
