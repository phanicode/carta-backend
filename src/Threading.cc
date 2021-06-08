/* This file is part of the CARTA Image Viewer: https://github.com/CARTAvis/carta-backend
   Copyright 2018, 2019, 2020, 2021 Academia Sinica Institute of Astronomy and Astrophysics (ASIAA),
   Associated Universities, Inc. (AUI) and the Inter-University Institute for Data Intensive Astronomy (IDIA)
   SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "Threading.h"
#include <list>
#include <mutex>
#include <thread>
#include "OnMessageTask.h"

namespace carta {
int ThreadManager::_omp_thread_count = 0;

void ThreadManager::ApplyThreadLimit() {
    // Skip application if we are already inside an OpenMP parallel block
    if (omp_get_num_threads() > 1) {
        return;
    }

    if (_omp_thread_count > 0) {
        omp_set_num_threads(_omp_thread_count);
    } else {
        omp_set_num_threads(omp_get_num_procs());
    }
}

void ThreadManager::SetThreadLimit(int count) {
    _omp_thread_count = count;
    ApplyThreadLimit();
}

std::list<OnMessageTask*> __task_queue;
std::mutex __task_queue_mtx;
std::condition_variable __task_queue_cv;
std::list<std::thread*> __workers;
volatile bool __has_exited = false;

void ThreadManager::QueueTask(OnMessageTask* tsk) {
    std::unique_lock<std::mutex> lock(__task_queue_mtx);
    __task_queue.push_back(tsk);
    __task_queue_cv.notify_one();
}

void ThreadManager::StartEventHandlingThreads(int num_threads) {
    auto thread_lambda = []() {
        OnMessageTask* tsk;

        do {
            tsk = nullptr;
            std::unique_lock<std::mutex> lock(__task_queue_mtx);
            if (!(tsk = __task_queue.front())) {
                __task_queue_cv.wait(lock);
            } else {
                __task_queue.pop_front();
                lock.unlock();
                tsk->execute();
            }

            if (__has_exited) {
                return;
            }
        } while (true);
    };

    // Start worker threads
    for (int i = 0; i < num_threads; i++) {
        __workers.push_back(new std::thread(thread_lambda));
    }
}

void ThreadManager::ExitThreads() {
    __has_exited = true;
    __task_queue_cv.notify_all();

    while (!__workers.empty()) {
        __workers.pop_front();
    }
}

} // namespace carta