#include <cstdint>
#include <chrono>
#ifndef TIMEOP_H
#define TIMEOP_H

using namespace std;

class TimeOp {
public:
  // return nansecond from epoch time
  static int64_t getClockNan() {
    return chrono::high_resolution_clock::now().time_since_epoch().count();
  }

  static std::chrono::high_resolution_clock::time_point now() {
        return std::chrono::high_resolution_clock::now();
    }

  // return time difference in nansecond
  static int64_t diffNan(chrono::_V2::system_clock::time_point start,
                             chrono::_V2::system_clock::time_point end) {
    return chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  }

  static double passedMilliSeconds(chrono::_V2::system_clock::time_point start,
                             chrono::_V2::system_clock::time_point end) {
    return chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000000.0;
  }
};  // class TimeOp

#endif  // TIMEOP_H