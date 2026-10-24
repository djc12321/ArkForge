/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_RESTOOL_THREAD_POOL_H
#define OHOS_RESTOOL_THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace OHOS {
namespace Global {
namespace Restool {
class ThreadPool {
public:
    ~ThreadPool();

    /**
     * @brief Start the thread pool
     * @param threadCount the count of thread pool
     */
    uint32_t Start(const size_t &threadCount);

    /**
     * @brief Stop the thread pool
     */
    void Stop();

    /**
     * @brief Enqueue a task to queue of thread pool
     * @param f the function to execute
     * @param args the args of the function
     */
    template <class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> Enqueue(F &&f, Args &&...args);

    static ThreadPool &GetInstance();

private:
    ThreadPool();
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    void WorkInThread();
    std::vector<std::thread> workerThreads_;
    std::queue<std::function<void()>> tasks_;

    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool running_{ false };
};

template <typename F, typename... Args>
std::future<typename std::result_of<F(Args...)>::type> ThreadPool::Enqueue(F &&f, Args &&...args)
{
    using return_type = typename std::result_of<F(Args...)>::type;
    using p_task = std::packaged_task<return_type()>;
    auto task = std::make_shared<p_task>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.emplace([task]() { (*task)(); });
    }
    condition_.notify_one();
    return res;
}
} // namespace Restool
} // namespace Global
} // namespace OHOS
#endif
