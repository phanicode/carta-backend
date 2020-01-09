#include "Histogram.h"

#include <algorithm>

#include <omp.h>

#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <string.h>
#include <mutex>

using namespace carta;

Histogram::Histogram(int num_bins, float min_value, float max_value, const std::vector<float>& data)
    : _bin_width((max_value - min_value) / num_bins), _min_val(min_value), _hist(num_bins, 0), _data(data) {}

Histogram::Histogram(Histogram& h, tbb::split) : _bin_width(h._bin_width), _min_val(h._min_val), _hist(h._hist.size(), 0), _data(h._data) {}

void Histogram::operator()(const tbb::blocked_range<size_t>& r) {
    std::vector<int> tmp(_hist);
    for (auto i = r.begin(); i != r.end(); ++i) {
        auto v = _data[i];
        if (std::isnan(v) || std::isinf(v))
            continue;
        int bin = std::max(std::min((int)((v - _min_val) / _bin_width), (int)_hist.size() - 1), 0);
        ++tmp[bin];
    }
    _hist = tmp;
}


void Histogram::join(Histogram& h) { // NOLINT
    auto range = tbb::blocked_range<size_t>(0, _hist.size());
    auto loop = [this, &h](const tbb::blocked_range<size_t>& r) {
        size_t beg = r.begin();
        size_t end = r.end();
        std::transform(&h._hist[beg], &h._hist[end], &_hist[beg], &_hist[beg], std::plus<int>());
    };
    tbb::parallel_for(range, loop);
}


void Histogram::setup_bins(const int start, const int end) {
	int min_stride;
	std::function<void(std::vector<int> *, int, int)> calc_lambda = [&](std::vector<int> *hl, int lstart, int lend) {
        std::vector<int> * hr = new std::vector<int>(_hist.size());
        if ((lend - lstart) > min_stride) {
#pragma omp task
			calc_lambda(hl, lstart, lstart + ((lend - lstart) / 2));
#pragma omp task
			calc_lambda(hr, lstart + ((lend - lstart) / 2), lend);
#pragma omp taskwait
			std::transform(&(*hr)[0],&(*hr)[_hist.size()],&(*hl)[0],&(*hl)[0],std::plus<int>());
			delete hr;
        } else {
			for (auto i = lstart; i < lend; i++) {
				auto v = _data[i];
				if (std::isfinite(v)) {
					int binN = std::max(std::min((int)((v - _min_val) / _bin_width), (int)_hist.size() - 1), 0);
					++((*hl)[binN]);
				}
			}
        }
	};
#pragma omp parallel
	{
#pragma omp single
		{
			min_stride = _data.size() / omp_get_num_threads() + 1;
			calc_lambda(&_hist, 0, _data.size());
		}
	}
}

