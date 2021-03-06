/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
 *   Copyright 2013 Zynga Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#ifndef SYNCOBJECT_HH
#define SYNCOBJECT_HH 1

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/time.h>

#include "common.hh"

/**
 * Abstraction built on top of pthread mutexes
 */
class SyncObject : public Mutex {
public:
    SyncObject() : Mutex() {
        if (pthread_cond_init(&cond, NULL) != 0) {
            throw std::runtime_error("MUTEX ERROR: Failed to initialize cond.");
        }
    }

    ~SyncObject() {
        if (pthread_cond_destroy(&cond) != 0) {
            throw std::runtime_error("MUTEX ERROR: Failed to destroy cond.");
        }
    }

    void wait() {
        if (pthread_cond_wait(&cond, &mutex) != 0) {
            throw std::runtime_error("Failed to wait for condition.");
        }
        setHolder(true);
    }

    bool wait(const struct timeval &tv) {
        struct timespec ts;
        ts.tv_sec = tv.tv_sec + 0;
        ts.tv_nsec = tv.tv_usec * 1000;

        switch (pthread_cond_timedwait(&cond, &mutex, &ts)) {
        case 0:
            setHolder(true);
            return true;
        case ETIMEDOUT:
            setHolder(true);
            return false;
        default:
            throw std::runtime_error("Failed timed_wait for condition.");
        }
    }

    bool wait(const double secs) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        advance_tv(tv, secs);
        return wait(tv);
    }

    void notify() {
        if(pthread_cond_broadcast(&cond) != 0) {
            throw std::runtime_error("Failed to broadcast change.");
        }
    }

private:
    pthread_cond_t cond;

    DISALLOW_COPY_AND_ASSIGN(SyncObject);
};

#endif

