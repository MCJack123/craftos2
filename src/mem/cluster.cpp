/*
 * mem/cluster.cpp
 * CraftOS-PC 2
 * 
 * This file implements the class for the cluster allocator.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows. 
 */

#include "cluster.hpp"
#include <climits>
#include <cstring>
#include <cstdlib>
#include <cassert>

#define BITMAP_UNIT_SIZE (sizeof(bitmap_unit) * 8)

ClusterAllocator::ClusterAllocator(const size_t elem_size): elem_size(elem_size + sizeof(void*)) {
    head = newcluster(0);
    freecluster = head;
}

ClusterAllocator::~ClusterAllocator() {
    cluster_t * cluster = head, * next;
    while (cluster != NULL) {
        next = cluster->next;
        ::free(cluster);
        cluster = next;
    }
}

ClusterAllocator::cluster_t * ClusterAllocator::newcluster(long id) {
    cluster_t * cluster = (cluster_t*)malloc(sizeof(cluster_t) + CLUSTER_SIZE * elem_size);
    memset(cluster, 0, sizeof(cluster_t) + CLUSTER_SIZE * elem_size);
    cluster->next = NULL;  /* ensure next pointer is NULL */
    cluster->id = id; /* set cluster number */
    for (int i = 0; i < CLUSTER_SIZE; i++)
        *(cluster_t**)(cluster->ptr + i * elem_size) = cluster;
    return cluster;
}

void * ClusterAllocator::alloc() {
    void *ptr = NULL;
    cluster_t *cluster, *next;
    int i, j;
    for (cluster = freecluster; ptr == NULL; cluster = cluster->next) {
        /* search for unused entry in cluster */
        for (i = 0; i < CLUSTER_BITMAP_SIZE; i++) {
            if (cluster->bitmap[i] != (bitmap_unit)~(bitmap_unit)0) {  /* empty space found? */
                // magic (https://graphics.stanford.edu/%7Eseander/bithacks.html#CountBitsSetParallel)
                // please don't use this on a 256-bit CPU
                bitmap_unit v = (cluster->bitmap[i] ^ (cluster->bitmap[i] + 1)) >> 1;
                v = v - ((v >> 1) & (bitmap_unit)~(bitmap_unit)0/3); // temp
                v = (v & (bitmap_unit)~(bitmap_unit)0/15*3) + ((v >> 2) & (bitmap_unit)~(bitmap_unit)0/15*3); // temp
                v = (v + (v >> 4)) & (bitmap_unit)~(bitmap_unit)0/255*15; // temp
                j = (bitmap_unit)(v * ((bitmap_unit)~(bitmap_unit)0/255)) >> (sizeof(bitmap_unit) - 1) * 8; // count
                ptr = (void*)(cluster->ptr + (i * BITMAP_UNIT_SIZE + j) * elem_size);
                cluster->bitmap[i] |= (1 << j);
                break;
            }
        }
        if (ptr != NULL) break;
        if (cluster->next == NULL) {  /* need new cluster? */
            next = newcluster(cluster->id + 1);
            cluster->next = next;  /* chain next cluster in list */
        }
    }
    freecluster = cluster;
    return (void*)((char*)ptr + sizeof(void*));
}

void ClusterAllocator::free(void * ptr) {
    cluster_t * cluster = *(cluster_t**)((char*)ptr - sizeof(cluster_t**));
    int idx = ((ptrdiff_t)ptr - (ptrdiff_t)cluster->ptr) / elem_size;
    cluster->bitmap[idx/BITMAP_UNIT_SIZE] &= ~(1 << (idx % BITMAP_UNIT_SIZE));  /* mark entry as freed */
    if (cluster->id < freecluster->id)
        freecluster = cluster;

    int i, empty, full;
    empty = cluster->bitmap[0] == 0;
    full = cluster->bitmap[0] == ULONG_MAX;
    for (i = 1; i < CLUSTER_BITMAP_SIZE && (empty || full); i++) {
        if (cluster->bitmap[i]) {  /* any entry in use? */
            empty = 0;
        }
        if (cluster->bitmap[i] != ULONG_MAX) {  /* any entry *not* in use? */
            full = 0;
        }
    }
    if (empty && cluster->next != NULL) {  /* entire cluster unused? */
        /* unlink and free cluster */
        if (head != cluster) {
            cluster_t * last = head;
            while (last->next != cluster && last->next != NULL) last = last->next;
            assert(last != NULL);
            // TODO: sweep previous empty clusters
            if (last != cluster) last->next = cluster->next;
        } else head = cluster->next;
        if (freecluster == cluster)
            freecluster = cluster->next;
        ::free(cluster);
    } else if (!full && cluster->id < freecluster->id) {
        freecluster = cluster;
    }
}
