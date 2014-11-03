//
// Stopwatch measing time in milliseconds.
//
// Nick Zaborovskii, Aug-2014
//

#pragma once

#include <chrono>

#ifdef _SWATCH_TEST_
#include <thread>
#endif //_SWATCH_TEST_


struct swatch {
	swatch() : _start(std::chrono::system_clock::now()) {}

	void reset() {
		_start = std::chrono::system_clock::now();
	}

	void stop(const char *msg = "") {
		long long duration = get();
		printf("[%s] %lld microsec\n", msg, duration);
	}

	__inline long long get() {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - _start).count();
	}
#ifdef _SWATCH_TEST_
	void test() {
		reset();
		std::this_thread::sleep_for(std::chrono::milliseconds(2511));
		int elapsed = int(get());
		printf("Slept time is 2511, elapsed time is: %d\n", elapsed);
	}
#endif // _SWATCH_TEST_

private:
	std::chrono::time_point<std::chrono::system_clock> _start;
};