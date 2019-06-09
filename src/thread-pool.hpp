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

  /**
   * c'tor, prepares and starts the thread pool
  */
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

  /**
   * d'tor, wait for all the jobs to complete
  */
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

  /**
   * schedule a job
   * \param func the opeation to execute
   * \return the id of the scheduled operation
  */
  id_t submit(function_t&& func) {
    unique_lock lock(mutex);
    ++id_counter;
    ids.insert(id_counter);
    queue.emplace(id_counter, std::move(func));
    cond.notify_all();
    return id_counter;
  }

  /**
   * wait for job to complete
   * \param id the job id
  */
  void wait(id_t id) {
    unique_lock lock(mutex);
    while (ids.count(id)) {
      cond.wait(lock);
    }
  }

  /**
   * wait for multiple jobs to complete
   * \param cont a container holding a set of id_t
  */
  template <typename Container>
  typename std::enable_if<not std::is_same<std::remove_cv<id_t>::type, Container>::value>::type
  wait(const Container& cont) {
    for (const id_t id : cont) {
      wait(id);
    }
  }

  template <typename It>
  struct is_random_iterator : public std::integral_constant<bool, std::is_same<
    typename std::iterator_traits<It>::iterator_category,
    std::random_access_iterator_tag
  >::value> {};

  template <typename Cn>
  struct is_random_container : public std::integral_constant<bool,
    is_random_iterator<typename Cn::iterator>::value>
  {};

  /**
   * executes a given lamba function on each element of a range splitting the work across mulitple
   * worker threads.
   * \param begin the iterator to the first element
   * \param end the past-last iterator
   * \param batch_size the number of elements to process per job
   * \param lambda the operation to perform on the data
   * \note each element is guaranteed to be processed only once but data access is not synchronized
   * \note the signature for lambda is void(element&)
  */
  template <typename Iterator, typename Lambda>
  typename std::enable_if<is_random_iterator<Iterator>::value, void>::type
  for_each(const Iterator& begin,
           const Iterator& end,
           size_t batch_size,
           const Lambda& lambda) {

    const auto size = std::distance(begin, end);
    const auto rest = size % batch_size;
    const auto runs = size / batch_size + (rest ? 1 : 0);

    std::vector<thread_pool::id_t> jobs;
    jobs.reserve(runs);

    for (size_t run = 0; run < runs; ++run) {
      jobs.push_back(submit([&lambda, run, runs, batch_size, &begin, &end](){
        auto it = std::next(begin, run * batch_size);
        const auto last = (run == runs - 1) ? end : std::next(it, batch_size);
        for(; it != last; ++it) {
          lambda(*it);
        }
      }));
    }

    wait(jobs);
  }

  /// see for_each(begin, end...)
  template <typename Container, typename Lambda>
  typename std::enable_if<is_random_container<Container>::value, void>::type
  for_each(Container& container, size_t batch_size, const Lambda& lambda) {
    for_each(std::begin(container), std::end(container), batch_size, lambda);
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

  struct job_t {
    inline job_t(id_t id, function_t&& func) : id(id), func(std::move(func)) {}
    id_t id;
    function_t func;
  };

  std::vector<std::thread> pool;
  std::unordered_set<id_t> ids;
  std::queue<job_t> queue;
  std::mutex mutex;
  std::condition_variable cond;
  size_t workers;
  bool stop;
  id_t id_counter;
};
