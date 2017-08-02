#include <assert.h>

template<class T>
class LastNMinMax {
public:
	LastNMinMax(int size);
	
	void put(T x);
	T min() const;
	T max() const;
	T last() const {
		return they_[pos_];
	}
	int count() const { return count_; }
	
private:
	int size_;
	int pos_;
	int count_;
	
	T* they_;
};

template<class T>
LastNMinMax<T>::LastNMinMax(int size)
	: size_(size), pos_(-1), count_(0) {
	they_ = new T[size_];
}

template<class T>
void LastNMinMax<T>::put(T x) {
	pos_ = (pos_ + 1) % size_;
	they_[pos_] = x;
	if (count_ < size_) {
		count_++;
	}
}

template<class T>
T LastNMinMax<T>::min() const {
	assert(pos_ >= 0);
	T tmin = they_[0];
	for (int i = 1; i < count_; ++i) {
		if (they_[i] < tmin) {
			tmin = they_[i];
		}
	}
	return tmin;
}

template<class T>
T LastNMinMax<T>::max() const {
	assert(pos_ >= 0);
	T tmax = they_[0];
	for (int i = 1; i < count_; ++i) {
		if (they_[i] > tmax) {
			tmax = they_[i];
		}
	}
	return tmax;
}
