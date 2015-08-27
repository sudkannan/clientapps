// Copyright 2011 Google Inc. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Various stubs for the unit tests for the open-source version of Snappy.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include "snappy-test.h"

#ifdef HAVE_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>

DEFINE_bool(run_microbenchmarks, true,
            "Run microbenchmarks before doing anything else.");

namespace snappy {




string ReadTestDataFile(const string& base) {
  string contents;
  const char* srcdir = getenv("srcdir");  // This is set by Automake.
  if (srcdir) {
    File::ReadFileToStringOrDie(
        string(srcdir) + "/testdata/" + base, &contents);
  } else {
    File::ReadFileToStringOrDie("testdata/" + base, &contents);
  }
  return contents;
}

string StringPrintf(const char* format, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  return buf;
}

bool benchmark_running = false;
int64 benchmark_real_time_us = 0;
int64 benchmark_cpu_time_us = 0;
string *benchmark_label = NULL;
int64 benchmark_bytes_processed = 0;

void ResetBenchmarkTiming() {
  benchmark_real_time_us = 0;
  benchmark_cpu_time_us = 0;
}

#ifdef WIN32
LARGE_INTEGER benchmark_start_real;
FILETIME benchmark_start_cpu;
#else  // WIN32
struct timeval benchmark_start_real;
//struct rusage benchmark_start_cpu;
#endif  // WIN32

void StartBenchmarkTiming() {
#ifdef WIN32
  QueryPerformanceCounter(&benchmark_start_real);
  FILETIME dummy;
  CHECK(GetProcessTimes(
      GetCurrentProcess(), &dummy, &dummy, &dummy, &benchmark_start_cpu));
#else
  gettimeofday(&benchmark_start_real, NULL);
  /*if (getrusage(RUSAGE_SELF, &benchmark_start_cpu) == -1) {
    perror("getrusage(RUSAGE_SELF)");
    exit(1);
  }*/
#endif
  benchmark_running = true;
}

void StopBenchmarkTiming() {
  if (!benchmark_running) {
    return;
  }

#ifdef WIN32
  LARGE_INTEGER benchmark_stop_real;
  LARGE_INTEGER benchmark_frequency;
  QueryPerformanceCounter(&benchmark_stop_real);
  QueryPerformanceFrequency(&benchmark_frequency);

  double elapsed_real = static_cast<double>(
      benchmark_stop_real.QuadPart - benchmark_start_real.QuadPart) /
      benchmark_frequency.QuadPart;
  benchmark_real_time_us += elapsed_real * 1e6 + 0.5;

  FILETIME benchmark_stop_cpu, dummy;
  CHECK(GetProcessTimes(
      GetCurrentProcess(), &dummy, &dummy, &dummy, &benchmark_stop_cpu));

  ULARGE_INTEGER start_ulargeint;
  start_ulargeint.LowPart = benchmark_start_cpu.dwLowDateTime;
  start_ulargeint.HighPart = benchmark_start_cpu.dwHighDateTime;

  ULARGE_INTEGER stop_ulargeint;
  stop_ulargeint.LowPart = benchmark_stop_cpu.dwLowDateTime;
  stop_ulargeint.HighPart = benchmark_stop_cpu.dwHighDateTime;

  benchmark_cpu_time_us +=
      (stop_ulargeint.QuadPart - start_ulargeint.QuadPart + 5) / 10;
#else  // WIN32
  /*struct timeval benchmark_stop_real;
  gettimeofday(&benchmark_stop_real, NULL);
  benchmark_real_time_us +=
      1000000 * (benchmark_stop_real.tv_sec - benchmark_start_real.tv_sec);
  benchmark_real_time_us +=
      (benchmark_stop_real.tv_usec - benchmark_start_real.tv_usec);*/

  //struct rusage benchmark_stop_cpu;
  /*if (getrusage(RUSAGE_SELF, &benchmark_stop_cpu) == -1) {
    perror("getrusage(RUSAGE_SELF)");
    exit(1);
  }*/
 // benchmark_cpu_time_us += 1000000 * (benchmark_stop_cpu.ru_utime.tv_sec -
                                  //    benchmark_start_cpu.ru_utime.tv_sec);
  //benchmark_cpu_time_us += (benchmark_stop_cpu.ru_utime.tv_usec -
                     //       benchmark_start_cpu.ru_utime.tv_usec);
#endif  // WIN32

  benchmark_running = false;
}

void SetBenchmarkLabel(const string& str) {
  if (benchmark_label) {
    delete benchmark_label;
  }
  benchmark_label = new string(str);
}

void SetBenchmarkBytesProcessed(int64 bytes) {
  benchmark_bytes_processed = bytes;
}

struct BenchmarkRun {
  int64 real_time_us;
  int64 cpu_time_us;
};

struct BenchmarkCompareCPUTime {
  bool operator() (const BenchmarkRun& a, const BenchmarkRun& b) const {
    return a.cpu_time_us < b.cpu_time_us;
  }
};

void Benchmark::Run() {
  for (int test_case_num = start_; test_case_num <= stop_; ++test_case_num) {
    // Run a few iterations first to find out approximately how fast
    // the benchmark is.
    const int kCalibrateIterations = 100;
    ResetBenchmarkTiming();
    StartBenchmarkTiming();
    (*function_)(kCalibrateIterations, test_case_num);
    StopBenchmarkTiming();

    // Let each test case run for about 200ms, but at least as many
    // as we used to calibrate.
    // Run five times and pick the median.
    const int kNumRuns = 5;
    const int kMedianPos = kNumRuns / 2;
    int num_iterations = 0;
    if (benchmark_real_time_us > 0) {
      num_iterations = 200000 * kCalibrateIterations / benchmark_real_time_us;
    }
    num_iterations = max(num_iterations, kCalibrateIterations);
    BenchmarkRun benchmark_runs[kNumRuns];

    for (int run = 0; run < kNumRuns; ++run) {
      ResetBenchmarkTiming();
      StartBenchmarkTiming();
      (*function_)(num_iterations, test_case_num);
      StopBenchmarkTiming();

      benchmark_runs[run].real_time_us = benchmark_real_time_us;
      benchmark_runs[run].cpu_time_us = benchmark_cpu_time_us;
    }

    nth_element(benchmark_runs,
                benchmark_runs + kMedianPos,
                benchmark_runs + kNumRuns,
                BenchmarkCompareCPUTime());
    int64 real_time_us = benchmark_runs[kMedianPos].real_time_us;
    int64 cpu_time_us = benchmark_runs[kMedianPos].cpu_time_us;
    int64 bytes_per_second = benchmark_bytes_processed * 1000000 / cpu_time_us;

    string heading = StringPrintf("%s/%d", name_.c_str(), test_case_num);
    string human_readable_speed;
    if (bytes_per_second < 1024) {
      human_readable_speed = StringPrintf("%dB/s", bytes_per_second);
    } else if (bytes_per_second < 1024 * 1024) {
      human_readable_speed = StringPrintf(
          "%.1fkB/s", bytes_per_second / 1024.0f);
    } else if (bytes_per_second < 1024 * 1024 * 1024) {
      human_readable_speed = StringPrintf(
          "%.1fMB/s", bytes_per_second / (1024.0f * 1024.0f));
    } else {
      human_readable_speed = StringPrintf(
          "%.1fGB/s", bytes_per_second / (1024.0f * 1024.0f * 1024.0f));
    }

    fprintf(stderr,
#ifdef WIN32
            "%-18s %10I64d %10I64d %10d %s  %s\n",
#else
            "%-18s %10lld %10lld %10d %s  %s\n",
#endif
            heading.c_str(),
            static_cast<long long>(real_time_us * 1000 / num_iterations),
            static_cast<long long>(cpu_time_us * 1000 / num_iterations),
            num_iterations,
            human_readable_speed.c_str(),
            benchmark_label->c_str());
  }
}

}  // namespace snappy
