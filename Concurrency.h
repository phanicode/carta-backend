#pragma once

#include <iostream>
#include <list>
#include <mutex>
#include <shared_mutex>

#define __USE_TBB__ 0

template <class T>
class concurrent_queue {
public:
    concurrent_queue<T>() {}
    ~concurrent_queue<T>() {
        clear();
    }
    void push(T& elt) {
        _mtx.lock();
        _q.push_back(elt);
        _mtx.unlock();
    }
    bool try_pop(T& elt) {
        bool ret = false;
        _mtx.lock();
        if (!_q.empty()) {
            ret = true;
            elt = _q.front();
            _q.pop_front();
        }
        _mtx.unlock();
        return ret;
    }
    void clear() {
        _mtx.lock();
        while (!_q.empty()) {
            _q.pop_front();
        }
        _mtx.unlock();
    }

private:
    std::list<T> _q;
    std::mutex _mtx;
};

#if __USE_TBB__
#include <tbb/queuing_rw_mutex.h>

class rw_mutex_writer {
public:
    rw_mutex_writer(tbb::queuing_rw_mutex& mtx) {
        _lock.acquire(mtx, true);
    }
    ~rw_mutex_writer() {}
    void release() {
        _lock.release();
    }

private:
    tbb::queuing_rw_mutex::scoped_lock _lock;
};

class rw_mutex_reader {
public:
    rw_mutex_reader(tbb::queuing_rw_mutex& mtx) {
        _lock.acquire(mtx, false);
    }
    ~rw_mutex_reader() {}
    void release() {
        _lock.release();
    }

private:
    tbb::queuing_rw_mutex::scoped_lock _lock;
};
#else
class rw_mutex_writer {
public:
    rw_mutex_writer(std::shared_mutex& mtx) {
        _mtx = &mtx;
        //		std::cout << this << " " << &mtx  << " rw_mutex_writer "  << std::endl;
        mtx.lock();
        is_locked = true;
    }
    ~rw_mutex_writer() {
        //		std::cout << this << " " << _mtx  << " ~rw_mutex_writer "  << std::endl;
        if (is_locked) {
            _mtx->unlock();
        }
    }
    void release() {
        //		std::cout << this << " " << _mtx  << " rw_mutex_writer::release "  << std::endl;
        if (is_locked) {
            is_locked = false;
            _mtx->unlock();
        }
    }

private:
    std::shared_mutex* _mtx;
    bool is_locked;
};

class rw_mutex_reader {
public:
    rw_mutex_reader(std::shared_mutex& mtx) {
        //		std::cout << this << " " << &mtx  << " rw_mutex_reader "  << std::endl;
        mtx.lock_shared();
        _mtx = &mtx;
        is_locked = true;
    }
    ~rw_mutex_reader() {
        //		std::cout << this << " " << _mtx  << " ~rw_mutex_reader "  << std::endl;
        if (is_locked) {
            _mtx->unlock_shared();
        }
    }
    void release() {
        //		std::cout  << this << " " << _mtx << " rw_mutex_reader::release "<< std::endl;
        if (is_locked) {
            is_locked = false;
            _mtx->unlock_shared();
        }
    }

private:
    std::shared_mutex* _mtx;
    bool is_locked;
};
#endif
