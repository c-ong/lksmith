/*
 * vim: ts=8:sw=8:tw=79:noet
 *
 * Copyright (c) 2011-2012, the Locksmith authors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "lksmith.h"
#include "test.h"

#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_WRAPPER_VOID(fn) \
static void *fn##_wrap(void *v __attribute__((unused))) { \
	return (void*)(intptr_t)fn(); \
}

#define THREAD_WRAPPER_INT(fn) \
static void *fn##_wrap(void *v) { \
	return (void*)(intptr_t)fn((int)(intptr_t)v); \
}

static struct lksmith_mutex g_lock1 = LKSMITH_MUTEX_INITIALIZER;
static struct lksmith_mutex g_lock2 = LKSMITH_MUTEX_INITIALIZER;

static sem_t g_inver_sem1;
static sem_t g_inver_sem2;

static int inver_thread_a(void)
{
	EXPECT_ZERO(lksmith_mutex_lock(&g_lock1));
	EXPECT_ZERO(lksmith_mutex_lock(&g_lock2));
	EXPECT_ZERO(lksmith_mutex_unlock(&g_lock2));
	EXPECT_ZERO(sem_post(&g_inver_sem1));
	EXPECT_ZERO(sem_wait(&g_inver_sem2));
	EXPECT_ZERO(lksmith_mutex_unlock(&g_lock1));
	return 0;
}

THREAD_WRAPPER_VOID(inver_thread_a);

static int inver_thread_b(void)
{
	EXPECT_ZERO(sem_wait(&g_inver_sem1));
	EXPECT_ZERO(lksmith_mutex_lock(&g_lock2));
	printf("doing the prelock that SHOULD generate an error...\n");
	EXPECT_EQ(lksmith_mutex_trylock(&g_lock1), EBUSY);
	printf("=====================\n");
	EXPECT_ZERO(sem_post(&g_inver_sem2));
	EXPECT_ZERO(lksmith_mutex_unlock(&g_lock2));
	return 0;
}

THREAD_WRAPPER_VOID(inver_thread_b);

static int test_ab_inversion(void)
{
	pthread_t thread_a, thread_b;
	void *rval;

	EXPECT_ZERO(sem_init(&g_inver_sem1, 0, 0));
	EXPECT_ZERO(sem_init(&g_inver_sem2, 0, 0));
	clear_recorded_errors();
	lksmith_set_error_cb(record_error);
	EXPECT_ZERO(pthread_create(&thread_a, NULL, inver_thread_a_wrap, NULL));
	EXPECT_ZERO(pthread_create(&thread_b, NULL, inver_thread_b_wrap, NULL));
	EXPECT_ZERO(pthread_join(thread_a, &rval));
	EXPECT_EQ(rval, NULL);
	EXPECT_ZERO(pthread_join(thread_b, &rval));
	EXPECT_EQ(rval, NULL);
	EXPECT_EQ(find_recorded_error(EDEADLK), 1);
	EXPECT_ZERO(sem_destroy(&g_inver_sem1));
	EXPECT_ZERO(sem_destroy(&g_inver_sem2));

	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_ab_inversion());

	return EXIT_SUCCESS;
}
