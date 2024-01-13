/*
 * Copyright (C) 2022-2023 Jordan Bancino <@jordan:bancino.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <Rand.h>

#include <Util.h>
#include <Memory.h>

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>

#define RAND_STATE_VECTOR_LENGTH 624
#define RAND_STATE_VECTOR_M 397

#define RAND_UPPER_MASK 0x80000000
#define RAND_LOWER_MASK 0x7FFFFFFF
#define RAND_TEMPER_B 0x9D2C5680
#define RAND_TEMPER_C 0xEFC60000

typedef struct RandState
{
    uint32_t mt[RAND_STATE_VECTOR_LENGTH];
    int index;
} RandState;

static void
RandSeed(RandState * state, uint32_t seed)
{
    state->mt[0] = seed & 0xFFFFFFFF;

    for (state->index = 1; state->index < RAND_STATE_VECTOR_LENGTH; state->index++)
    {
        state->mt[state->index] = (6069 * state->mt[state->index - 1]) & 0xFFFFFFFF;
    }
}

static uint32_t
RandGenerate(RandState * state)
{
    static const uint32_t mag[2] = {0x0, 0x9908B0DF};

    uint32_t result;

    if (state->index >= RAND_STATE_VECTOR_LENGTH || state->index < 0)
    {
        int kk;

        if (state->index >= RAND_STATE_VECTOR_LENGTH + 1 || state->index < 0)
        {
            RandSeed(state, 4357);
        }

        for (kk = 0; kk < RAND_STATE_VECTOR_LENGTH - RAND_STATE_VECTOR_M; kk++)
        {
            result = (state->mt[kk] & RAND_UPPER_MASK) | (state->mt[kk + 1] & RAND_LOWER_MASK);
            state->mt[kk] = state->mt[kk + RAND_STATE_VECTOR_M] ^ (result >> 1) ^ mag[result & 0x1];
        }

        for (; kk < RAND_STATE_VECTOR_LENGTH - 1; kk++)
        {
            result = (state->mt[kk] & RAND_UPPER_MASK) | (state->mt[kk + 1] & RAND_LOWER_MASK);
            state->mt[kk] = state->mt[kk + (RAND_STATE_VECTOR_M - RAND_STATE_VECTOR_LENGTH)] ^ (result >> 1) ^ mag[result & 0x1];
        }

        result = (state->mt[RAND_STATE_VECTOR_LENGTH - 1] & RAND_UPPER_MASK) | (state->mt[0] & RAND_LOWER_MASK);
        state->mt[RAND_STATE_VECTOR_LENGTH - 1] = state->mt[RAND_STATE_VECTOR_M - 1] ^ (result >> 1) ^ mag[result & 0x1];
        state->index = 0;
    }

    result = state->mt[state->index++];
    result ^= (result >> 11);
    result ^= (result << 7) & RAND_TEMPER_B;
    result ^= (result << 15) & RAND_TEMPER_C;
    result ^= (result >> 18);

    return result;
}

static void
RandDestructor(void *p)
{
    Free(p);
}

/* Generate random numbers using rejection sampling. The basic idea is
 * to "reroll" if a number happens to be outside the range. However
 * this could be extremely inefficient.
 * 
 * Another idea would just be to "reroll" if the generated number ends up
 * in the previously "biased" range, and THEN do a modulo.
 * 
 * This would be far more efficient for small values of max, and fixes the
 * bias issue. */

/* This algorithm therefore computes N random numbers generally in O(N)
 * time, while being less biased. */
void
RandIntN(uint32_t *buf, size_t size, uint32_t max)
{
    static pthread_key_t stateKey;
    static bool createdKey = false;

    /* Limit the range to banish all previously biased results */
    const uint32_t allowed = RAND_MAX - RAND_MAX % max;

    RandState *state;
    uint32_t tmp;
    size_t i;

    if (!createdKey)
    {
        pthread_key_create(&stateKey, RandDestructor);
        createdKey = true;
    }

    state = pthread_getspecific(stateKey);

    if (!state)
    {
        /* Generate a seed from the system time, PID, and TID */
        uint64_t ts = UtilTsMillis();
        uint32_t seed = ts ^ getpid() ^ (unsigned long) pthread_self();

        state = Malloc(sizeof(RandState));
        RandSeed(state, seed);

        pthread_setspecific(stateKey, state);
    }

    /* Generate {size} random numbers. */
    for (i = 0; i < size; i++)
    {
        /* Most of the time, this will take about 1 loop */
        do
        {
            tmp = RandGenerate(state);
        } while (tmp > allowed);

        buf[i] = tmp % max;
    }
}

/* Generate just 1 random number */
uint32_t
RandInt(uint32_t max)
{
    uint32_t val = 0;

    RandIntN(&val, 1, max);
    return val;
}
