#ifndef BINARY_STAT_LOGGER_H
#define BINARY_STAT_LOGGER_H

class SolverStats;
template<class T> class LastNMinMax;

class BinaryStatLogger {
public:
	// outname is strdup()ed and may be clobbered after calling constructor.
	virtual ~BinaryStatLogger();
	
	// return NULL on failure
	static BinaryStatLogger* open(const char* name);
	
	// return true if ok.
	virtual bool log(const SolverStats* it,
					 LastNMinMax<double>* recentKmpp, LastNMinMax<double>* recentSpread) = 0;
protected:
	BinaryStatLogger();
	// factory calls this:
	virtual bool init(const char* name) = 0;
private:
	// never do this
	BinaryStatLogger(const BinaryStatLogger& x) {};
};


#endif /* BINARY_STAT_LOGGER_H */
