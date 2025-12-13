/*
 * mem/cluster.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the cluster allocator.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows. 
 */

#include <cstddef>

#define CLUSTER_SIZE 4096
#define CLUSTER_BITMAP_SIZE CLUSTER_SIZE / (sizeof(bitmap_unit) * 8)

class ClusterAllocator {
    typedef unsigned long bitmap_unit;

    struct cluster_t {
        cluster_t * next;
        unsigned long id;
        bitmap_unit bitmap[CLUSTER_BITMAP_SIZE];
        unsigned char ptr[];
    };

    const size_t elem_size; // size of each element in the cluster
    cluster_t * head; // pointer to first node of cluster list
    cluster_t * freecluster; // pointer to first potentially free cluster

    cluster_t * newcluster(long id);

public:
    ClusterAllocator(const size_t elem_size);
    ~ClusterAllocator();
    void * alloc();
    void free(void * ptr);
};
