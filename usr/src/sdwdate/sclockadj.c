/*
SPDX-FileCopyrightText: ENCRYPTED SUPPORT LLC <adrelanos@kicksecure.com>
SPDX-License-Identifier: AGPL-3.0-or-later
*/
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Gradual adjustment step: move the clock 5,000,000 ns per iteration (imitates
 * ntpdate), with a 1 second wait between steps. */
#define FULL_JUMP 5000000LL

/* The pure arithmetic below is split out so it can be unit-tested, fuzzed and
 * formally verified (CBMC) with no clock syscalls and no root. change_time_by_
 * nanoseconds and main() are the only impure parts. */

/* Normalise a signed nanoseconds-since-epoch value into (seconds, nanoseconds)
 * with 0 <= nanoseconds < 1,000,000,000 (EUCLIDEAN remainder, so tv_nsec is
 * NEVER negative -- clock_settime rejects a negative or out-of-range tv_nsec).
 * Returns 0 on success, -1 if the seconds part does not fit time_t. */
int split_ns_since_epoch(long long total_ns, time_t *out_sec, long *out_nsec)
{
    long long sec = total_ns / 1000000000LL;
    long long nsec = total_ns % 1000000000LL;
    if (nsec < 0)
    {
        nsec += 1000000000LL;
        sec -= 1;
    }
    /* Defensive: reject a seconds value that would not round-trip through
     * time_t (e.g. a 32-bit time_t). */
    if ((long long)(time_t)sec != sec)
    {
        return -1;
    }
    *out_sec = (time_t)sec;
    *out_nsec = (long)nsec; /* 0 <= nsec < 1e9, fits long */
    return 0;
}

/* Add add_ns nanoseconds to the timespec cur, writing the normalised result to
 * out. Every step is overflow-checked (no signed-overflow UB). Returns 0 on
 * success, -1 on overflow / out-of-range. */
int timespec_add_ns(const struct timespec *cur, long long add_ns,
                    struct timespec *out)
{
    long long cur_ns = 0;
    long long new_ns = 0;
    time_t sec = 0;
    long nsec = 0;
    if (__builtin_mul_overflow((long long)cur->tv_sec, 1000000000LL, &cur_ns))
    {
        return -1;
    }
    if (__builtin_add_overflow(cur_ns, (long long)cur->tv_nsec, &cur_ns))
    {
        return -1;
    }
    if (__builtin_add_overflow(cur_ns, add_ns, &new_ns))
    {
        return -1;
    }
    if (split_ns_since_epoch(new_ns, &sec, &nsec) != 0)
    {
        return -1;
    }
    out->tv_sec = sec;
    out->tv_nsec = nsec;
    return 0;
}

/* Parse the nanosecond argument. strtoll (NOT atoll, whose overflow is UB)
 * with full validation: rejects overflow, empty, trailing junk and (by the
 * caller) zero. Returns 0 on success, -1 on error. */
int parse_ns_arg(const char *str, long long *out)
{
    char *end = NULL;
    long long value = 0;
    errno = 0;
    value = strtoll(str, &end, 10);
    if (errno == ERANGE)
    {
        return -1;
    }
    if (end == str || *end != '\0')
    {
        return -1;
    }
    *out = value;
    return 0;
}

/* Split |ns| into (full 5,000,000-ns jumps, remainder). Uses unsigned
 * magnitude so llabs(LLONG_MIN) UB cannot occur. */
void compute_jumps(long long ns, unsigned long long *full_jumps,
                   unsigned long long *remainder)
{
    /* Overflow-safe |ns|: for ns < 0, negate (ns + 1) (which cannot overflow,
     * as ns + 1 > LLONG_MIN) into the non-negative signed range, then add 1 --
     * avoiding both llabs(LLONG_MIN) UB and a negative-to-unsigned conversion. */
    unsigned long long magnitude =
        (ns < 0) ? ((unsigned long long)(-(ns + 1)) + 1ULL)
                 : (unsigned long long)ns;
    *full_jumps = magnitude / (unsigned long long)FULL_JUMP;
    *remainder = magnitude % (unsigned long long)FULL_JUMP;
}

/* receive time adjustment, negative or positive, in nanoseconds */
/* exits program upon failure */
void change_time_by_nanoseconds(long long add_ns)
{
    struct timespec tps = { 0 }; /* tv_sec; tv_nsec */
    struct timespec new_tps = { 0 };
    if (clock_gettime(CLOCK_REALTIME, &tps) == -1)
    {
        perror("Failed to get current time!");
        exit(EXIT_FAILURE);
    }
    if (timespec_add_ns(&tps, add_ns, &new_tps) != 0)
    {
        fprintf(stderr, "Time arithmetic out of range!\n");
        exit(EXIT_FAILURE);
    }
    if (clock_settime(CLOCK_REALTIME, &new_tps) == -1)
    {
        perror("Failed to change clock!");
        exit(EXIT_FAILURE);
    }
}

#ifndef SCLOCKADJ_TEST
/* intended to be used only by sdwdate with sane inputs */
int main(int argc, char *argv[])
{
    long long ns_time_change = 0;
    unsigned long long number_of_full_jumps = 0;
    unsigned long long last_jump_nanoseconds = 0;
    unsigned long long i = 0;
    long long step = 0;
    long long last_step = 0;

    if (argc < 2)
    {
        fprintf(stderr, "Too few args!\n");
        exit(EXIT_FAILURE);
    }
    if (argc > 2)
    {
        fprintf(stderr, "Too many args!\n");
        exit(EXIT_FAILURE);
    }
    if (parse_ns_arg(argv[1], &ns_time_change) != 0)
    {
        fprintf(stderr, "Failed to parse nanosecond argument!\n");
        exit(EXIT_FAILURE);
    }
    if (ns_time_change == 0)
    {
        fprintf(stderr, "Nanosecond argument is zero, nothing to do!\n");
        exit(EXIT_FAILURE);
    }

    /* since the nanosecond jump is fixed, count the number of complete jumps. */
    compute_jumps(ns_time_change, &number_of_full_jumps, &last_jump_nanoseconds);

    step = (ns_time_change > 0) ? FULL_JUMP : -FULL_JUMP;
    last_step = (ns_time_change > 0)
                    ? (long long)last_jump_nanoseconds
                    : -(long long)last_jump_nanoseconds;

    for (i = 0; i < number_of_full_jumps; ++i)
    {
        sleep(1); /* a 1 second wait imitates ntpdate */
        change_time_by_nanoseconds(step);
    }
    sleep(1);
    change_time_by_nanoseconds(last_step);

    return EXIT_SUCCESS;
}
#endif
