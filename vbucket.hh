/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef VBUCKET_HH
#define VBUCKET_HH 1

#include <cassert>

#include <map>
#include <vector>
#include <sstream>
#include <algorithm>

#include <memcached/engine.h>

#include "common.hh"
#include "atomic.hh"
#include "stored-value.hh"

class VBucketFilter {
public:

    explicit VBucketFilter() : acceptable() {}

    explicit VBucketFilter(std::vector<uint16_t> a) : acceptable(a) {
        std::sort(acceptable.begin(), acceptable.end());
    }

    bool operator ()(uint16_t v) {
        return acceptable.empty() || std::binary_search(acceptable.begin(),
                                                        acceptable.end(), v);
    }

private:

    std::vector<uint16_t> acceptable;
};

/**
 * An individual vbucket.
 */
class VBucket : public RCValue {
public:

    VBucket(int i, vbucket_state_t initialState) :
        ht(), id(i), state(initialState) {}

    int getId(void) { return id; }
    vbucket_state_t getState(void) { return state; }
    void setState(vbucket_state_t to, SERVER_CORE_API *core = NULL);

    const char * getStateString(void) {
        return VBucket::toString(state);
    }

    bool addPendingOp(const void *cookie) {
        LockHolder lh(pendingOpLock);
        if (state == active) {
            // State transitioned while we were waiting.
            return false;
        }
        pendingOps.push_back(cookie);
        return true;
    }

    void fireAllOps(SERVER_CORE_API *core, ENGINE_ERROR_CODE code = ENGINE_SUCCESS);

    HashTable               ht;

    static const char* toString(vbucket_state_t s) {
        switch(s) {
        case active: return "active"; break;
        case replica: return "replica"; break;
        case pending: return "pending"; break;
        case dead: return "dead"; break;
        }
        return "unknown";
    }

private:

    int                      id;
    Atomic<vbucket_state_t>  state;
    Mutex                    pendingOpLock;
    std::vector<const void*> pendingOps;

    DISALLOW_COPY_AND_ASSIGN(VBucket);
};

class NeedMoreBuckets : std::exception {};

class VBucketHolder : public RCValue {
public:
    VBucketHolder(size_t sz) :
        buckets(new RCPtr<VBucket>[sz]),
        size(sz) {}

    VBucketHolder(const RCPtr<VBucketHolder> &vbh, size_t sz) :
        buckets(new RCPtr<VBucket>[sz]),
        size(sz) {

        // No shrinkage allowed currently.
        assert(sz >= vbh->getSize());

        std::copy(buckets, buckets+vbh->getSize(), buckets);
    }

    ~VBucketHolder() {
        delete[] buckets;
    }

    RCPtr<VBucket> getBucket(int id) const {
        assert(id >= 0);
        if (static_cast<size_t>(id) < size) {
            return buckets[id];
        } else {
            RCPtr<VBucket> r;
            return r;
        }
    }

    void addBucket(const RCPtr<VBucket> &b) {
        if (static_cast<size_t>(b->getId()) < size) {
            buckets[b->getId()].reset(b);
        } else {
            throw new NeedMoreBuckets;
        }
    }

    void removeBucket(int id) {
        assert(id >= 0);
        if (static_cast<size_t>(id) < size) {
            buckets[id].reset();
        }
    }

    std::vector<int> getBuckets(void) const {
        std::vector<int> rv;
        for (size_t i = 0; i < size; ++i) {
            RCPtr<VBucket> b(buckets[i]);
            if (b) {
                rv.push_back(b->getId());
            }
        }
        return rv;
    }

    size_t getSize(void) const {
        return size;
    }

private:
    RCPtr<VBucket> *buckets;
    size_t size;
};

/**
 * A map of known vbuckets.
 */
class VBucketMap {
public:
    VBucketMap() : buckets(new VBucketHolder(4096)) { }

    void addBucket(RCPtr<VBucket> &b) {
        assert(b);
        RCPtr<VBucketHolder> o(buckets);
        try {
            o->addBucket(b);
        } catch (NeedMoreBuckets &e) {
            grow(b->getId())->addBucket(b);
        }
    }

    void removeBucket(int id) {
        RCPtr<VBucketHolder> o(buckets);
        o->removeBucket(id);
    }

    void addBuckets(const std::vector<VBucket*> &newBuckets) {
        std::vector<VBucket*>::const_iterator it;
        for (it = newBuckets.begin(); it != newBuckets.end(); ++it) {
            RCPtr<VBucket> v(*it);
            addBucket(v);
        }
    }

    RCPtr<VBucket> getBucket(int id) const {
        RCPtr<VBucketHolder> o(buckets);
        return o->getBucket(id);
    }

    std::vector<int> getBuckets(void) {

        RCPtr<VBucketHolder> o(buckets);
        return o->getBuckets();
    }

private:
    RCPtr<VBucketHolder> grow(size_t id) {
        LockHolder lh(mutex);
        if (buckets->getSize() <= id) {
            // still not big enough
            size_t n(0);
            for (n = 4096; n <= id; n *= 2) {} // find next power of 2
            RCPtr<VBucketHolder> nbh(new VBucketHolder(buckets, n));
            buckets = nbh;
        }
        return buckets;
    }

    mutable RCPtr<VBucketHolder> buckets;
    Mutex mutex; // Not acquired often, but you could have a lot of stuff waiting on it

    DISALLOW_COPY_AND_ASSIGN(VBucketMap);
};

#endif /* VBUCKET_HH */
