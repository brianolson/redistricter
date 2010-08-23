#include "LastNMinMax_inl.h"

//#include <assert.h>
//#include <stdio.h>
#include <iostream>

using std::cerr;
using std::endl;

static int exit_value = 0;

#define EXPECT_EQ(actual, canon) EXPECT_BODY(canon, actual, #canon, #actual, __FILE__, __LINE__)
template<class A, class B>
void EXPECT_BODY(A canon, B actual, const char* astr, const char* bstr, const char* file, int line) {
	if (canon != actual) {
		//fprintf(stderr, "%s:%d expected \"%s\" but got \"%s\"
		cerr << file << ':' << line << " EXPECT(" << bstr << ", " << astr << ") expected " << canon << " but got " << actual << endl;
		exit_value = 1;
	}
}

template<class T>
void NumericTestLastNMinMax() {
	LastNMinMax<T> ltest(20);
	ltest.put(2);
	EXPECT_EQ(ltest.min(), 2);
	EXPECT_EQ(ltest.max(), 2);
	ltest.put(4);
	EXPECT_EQ(ltest.min(), 2);
	EXPECT_EQ(ltest.max(), 4);
	ltest.put(1);
	EXPECT_EQ(ltest.min(), 1);
	EXPECT_EQ(ltest.max(), 4);
	for (int i = 0; i < 20; ++i) {
		ltest.put(2);
	}
	EXPECT_EQ(ltest.min(), 2);
	EXPECT_EQ(ltest.max(), 2);
}

int main(int argc, const char** argv) {
	//LastNMinMax<int> lint(20);
	//LastNMinMax<double> ld(20);
	NumericTestLastNMinMax<int>();
	NumericTestLastNMinMax<double>();
	return exit_value;
}