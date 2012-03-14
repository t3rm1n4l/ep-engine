#include <eviction.hh>

EvictItem *LRUPolicy::evict()
{
    LRUItem *ent = ++it;
    return static_cast<EvictItem *>(ent);
}

EvictItem *RandomPolicy::evict()
{
    EvictItem *ent = ++it;
    return ent;
}

bool EvictionManager::evictSize(size_t size)
{
    size_t cur = 0;

    if (pauseEviction) { return false; }

    while(cur < size) {
        EvictItem *ent = evpolicy->evict();
        if (ent == NULL) {
            getLogger()->log(EXTENSION_LOG_WARNING, NULL, "Eviction: Empty list, ejection failed.  Evicted only %udB out of a total %udB required.", cur, size);
            stats.evictStats.numEmptyQueue++;
            return false;
        }
        std::string k;
        uint16_t b;
        k = ent->getKey();
        b = ent->get_vbucket_id();
        delete ent;
        count--;

        RCPtr<VBucket> vb = store->getVBucket(b);
        int bucket_num(0);
        LockHolder lh = vb->ht.getLockedBucket(k, &bucket_num);
        StoredValue *v = vb->ht.unlocked_find(k, bucket_num, false);

        if (!v) {
            getLogger()->log(EXTENSION_LOG_INFO, NULL, "Eviction: Key not present.");
            stats.evictStats.failedTotal.numKeyNotPresent++;
    //        failedstats.numKeyNotPresent++;
        } else if (!v->ejectValue(stats, vb->ht)) {
            getLogger()->log(EXTENSION_LOG_INFO, NULL, "Eviction: Key not eligible for eviction.");
            if (v->isResident() == false) {
                stats.evictStats.failedTotal.numAlreadyEvicted++;
      //          failedstats.numAlreadyEvicted++;
            } else if (v->isClean() == false) {
                stats.evictStats.failedTotal.numDirties++;
        //        failedstats.numDirties++;
            } else if (v->isDeleted() == false) {
                stats.evictStats.failedTotal.numDeleted++;
          //      failedstats.numDeleted++;
            }
        } else {
            cur += v->valLength(); 
            /* update stats for eviction that just happened */
            stats.evictStats.numTotalKeysEvicted++;
            stats.evictStats.numKeysEvicted++;
        }
    }

    return true;
}

// Return policy if it requires a background job.
EvictionPolicy *EvictionManager::evictionBGJob(void) 
{
    if (pauseJob) {
        return NULL;
    }
    if (policyName != evpolicy->description()) {
        EvictionPolicy *p = EvictionPolicyFactory::getInstance(policyName, store, stats);
        if (p) {
            delete evpolicy;
            evpolicy = p;
        }
    }
    evpolicy->setSize(maxSize);
    if (evpolicy->backgroundJob) { 
        return evpolicy; 
    }
    else { 
        return NULL; 
    }
}
