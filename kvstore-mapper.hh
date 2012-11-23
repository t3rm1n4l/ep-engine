/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef KVSTORE_MAPPER_HH
#define KVSTORE_MAPPER_HH 1

#include "queueditem.hh"

#define MAX_SHARDS_LIMIT 2520

/*
 * Class to distribute input data across kvstores.
 */
class KVStoreMapper {
public: 
    static void createKVMapper(int numKV, bool mapVB) {
        if (instance == NULL) {
            instance = new KVStoreMapper(numKV, mapVB);
        }
    }

    static void destroy() {
        delete instance;
        instance = NULL;
    }

    static int getVBucketToKVId(uint16_t vbid) {
        if (instance == NULL) {
            // Should never happen
            return -1;
        }
        //FIXME:: Fix with a better algorithm to map vbuckets
        return vbid % instance->numKVStores;
    }

    static int getKVStoreId(const std::string &key, uint16_t vbid) {
        if (instance == NULL) {
            // Should never happen
            return -1;
        }
        if (instance->mapVBuckets) {
            return KVStoreMapper::getVBucketToKVId(vbid);
        }

        int h = 94418953;
        int i=0;
        const char *str = key.c_str();

        for(i=0; str[i] != 0x00; i++) {
            h = ((h << 5) + h) ^ str[i];
        }
        return std::abs(h / MAX_SHARDS_LIMIT) % (int)instance->numKVStores;
    }

    static std::vector<uint16_t> getVBucketsForKVStore(int kvid) {
        assert(instance != NULL);
        (void)  kvid;
        std::vector<uint16_t> kvstore_vbs;
        kvstore_vbs.push_back(0);
        return kvstore_vbs;
    }

private:
    KVStoreMapper(int numKV, bool mapVB) : numKVStores(numKV), mapVBuckets(mapVB) {}

    static KVStoreMapper *instance;
    int numKVStores;
    bool mapVBuckets;
};

#endif
