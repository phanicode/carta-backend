#include <iostream>
#include <mutex>
#include <list>

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
		while(!_q.empty()) {
			_q.pop_front();
		}
		_mtx.unlock();
    }
private:
    std::list<T> _q;
    std::mutex _mtx;
};
