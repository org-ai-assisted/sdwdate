/*
 * SPDX-FileCopyrightText: ENCRYPTED SUPPORT LLC <adrelanos@kicksecure.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _GNU_SOURCE
#define POSITIVE_STEP_NS 5000000
#define NEGATIVE_STEP_NS -5000000
#define NS_IN_SEC 1000000000

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

void print_help_and_exit(void) {
  fprintf(stderr,
    "sclockadj: gradually shift system time a specified number of nanoseconds forward or backward\n");
  fprintf(stderr,
    "Usage: sclockadj <shift_ns>\n");
  exit(0);
}

bool step_timestamp(struct timespec *ts_ptr, int64_t *time_shift_ns_ptr) {
  int64_t time_shift_ns = 0;
  struct timespec ts = { 0 };
  int64_t shift_val = 0;
  int64_t temp_ns = 0;
  int64_t temp_sec = 0;

  if (ts_ptr == NULL || time_shift_ns_ptr == NULL) {
    return false;
  }
  time_shift_ns = *time_shift_ns_ptr;
  ts = *ts_ptr;

  if (time_shift_ns > 0) {
    if (time_shift_ns > POSITIVE_STEP_NS) {
      shift_val = POSITIVE_STEP_NS;
      time_shift_ns -= POSITIVE_STEP_NS;
    } else {
      shift_val = time_shift_ns;
      time_shift_ns = 0;
    }
  } else {
    if (time_shift_ns < NEGATIVE_STEP_NS) {
      shift_val = NEGATIVE_STEP_NS;
      time_shift_ns -= NEGATIVE_STEP_NS;
    } else {
      shift_val = time_shift_ns;
      time_shift_ns = 0;
    }
  }

  temp_ns = (int64_t)ts.tv_nsec;
  temp_sec = (int64_t)ts.tv_sec;
  assert(temp_ns >= 0);
  assert(temp_ns < NS_IN_SEC);

  temp_ns += shift_val; // note this may subtract
  if (temp_ns >= NS_IN_SEC) {
    if (temp_sec == INT64_MAX) {
      return false;
    }
    temp_ns -= NS_IN_SEC;
    temp_sec++;
  } else if (temp_ns < 0) {
    if (temp_sec == INT64_MIN) {
      return false;
    }
    temp_ns += NS_IN_SEC;
    temp_sec--;
  }

  ts.tv_nsec = (uint32_t)(temp_ns);
  ts.tv_sec = (time_t)(temp_sec);
  *ts_ptr = ts;
  *time_shift_ns_ptr = time_shift_ns;
  return true;
}

void slew_system_time(int64_t time_shift_ns) {
  struct timespec ts = { 0 };

  while (time_shift_ns != 0) {
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
      err(1, "ERROR: Could not get current time");
    }
    if (!step_timestamp(&ts, &time_shift_ns)) {
      errx(1, "ERROR: Could not step timestamp!\n");
    }
    if (clock_settime(CLOCK_REALTIME, &ts) == -1) {
      err(1, "ERROR: Could not set current time");
    }
    sleep(1);
  }
}

int64_t parse_int(char *in_str) {
  long long raw_parse_rslt = 0;
  char *check_ptr = NULL;

  errno = 0;
  raw_parse_rslt = strtoll(in_str, &check_ptr, 10);
  if (errno != 0) {
    return 0;
  }
  if (*check_ptr != '\0') {
    errno = EOPNOTSUPP;
    return 0;
  }
  // Probably impossible but check anyway
  if (raw_parse_rslt > INT64_MAX || raw_parse_rslt < INT64_MIN) {
    errno = ERANGE;
    return 0;
  }
  return (int64_t)(raw_parse_rslt);
}

#ifndef SCLOCKADJ_TEST
int main(int argc, char **argv) {
  int64_t time_shift_int = 0;

  if (argc != 2) {
    errx(1, "ERROR: Incorrect number of arguments! See --help.\n");
  }
  if (argv[1][0] == '\0') {
    errx(1, "ERROR: Empty argument! See --help.\n");
  }
  if (strcmp(argv[1], "--help") == 0) {
    print_help_and_exit();
  }
  time_shift_int = parse_int(argv[1]);
  if (errno != 0) {
    errx(1, "ERROR: Time shift value is not an integer or is out of range!\n");
  }

  slew_system_time(time_shift_int);
  exit(0);
}
#endif
