#ifndef STAT_THING_H
#define STAT_THING_H

class StatThing {
public:
	StatThing();
	
	double minimum;
	double maximum;
	double sum;
	int count;
	
	void log(double x);
	inline double average() const {
		return sum / count;
	}
	inline double min() const {
		return minimum;
	}
	inline double max() const {
		return maximum;
	}
	inline void clear() {
		minimum = 0.0;
		maximum = 0.0;
		sum = 0.0;
		count = 0;
	}
};

#endif /* STAT_THING_H */