//
// Created by lwh on 19-7-15.
//
#include <iostream>
#include <algorithm>
#include <functional>
#include "tracer.h"
#include "index_btree.h"

using namespace std;

uint64_t total = 1000000;
int pdegree = 2;
int simple = 0;
long total_runtime = 0;
long max_runtime = 0;
long min_runtime = std::numeric_limits<long>::max();

uint64_t *loads = nullptr;
index_btree *xh;

void initLoads() {
    g_thread_cnt = 1;
    g_part_cnt = 1;
    warmup_finish = false;
    loads = new uint64_t[total];

    for (int i = 0; i < total; i++) {
        loads[i] = i;
    }
}

void simpleOperationTests() {
    Tracer tracer;
    tracer.startTime();
    index_btree *xh = (index_btree *) _mm_malloc(sizeof(index_btree), 64);
    new(xh) index_btree();
    xh->init(1);
    cout << "Table init: " << tracer.getRunTime() << endl;
    tracer.startTime();

    for (int i = 0; i < total; i++) {
        itemid_t *m_item = (itemid_t *) mem_allocator.alloc(sizeof(itemid_t), 0);
        m_item->init();
        m_item->type = DT_row;
        m_item->location = &loads[i];
        m_item->valid = true;
        xh->index_insert(loads[i], m_item, 0);
    }

    cout << "Table insert: " << tracer.getRunTime() << endl;
    tracer.startTime();

    for (int i = 0; i < total; i++) {
        xh->index_remove(loads[i]);
    }
    cout << "Table empty: " << tracer.getRunTime() << endl;

    for (int i = 0; i < total; i++) {
        itemid_t *m_item;
        xh->index_read(loads[i], m_item, 0, -1);
        if (*((uint64_t *) m_item->location) != loads[i]) {
            cout << loads[i] << " " << m_item->valid << " " << *((uint64_t *) m_item->location) << endl;
        }
    }
}

struct paramstruct {
    int tid;
    index_btree *xh;
    long runtime = 0;
};

void *worker(void *args) {
    paramstruct *param = static_cast<paramstruct *>(args);
    Tracer tracer;
    tracer.startTime();

    for (int i = param->tid; i < total; i += pdegree) {
        itemid_t *m_item = (itemid_t *) mem_allocator.alloc(sizeof(itemid_t), 0);
        m_item->init();
        m_item->type = DT_row;
        m_item->location = &loads[i];
        m_item->valid = true;
        param->xh->index_insert(loads[i], m_item, 0);
    }

    param->runtime = tracer.getRunTime();
}

void insert() {
    total_runtime = 0;
    pthread_t threads[pdegree];
    paramstruct params[pdegree];

    for (int i = 0; i < pdegree; i++) {
        params[i].tid = i;
        params[i].xh = xh;
        pthread_create(&threads[i], nullptr, worker, &params[i]);
    }

    for (int i = 0; i < pdegree; i++) {
        pthread_join(threads[i], nullptr);
        total_runtime += params[i].runtime;
        if (params[i].runtime > max_runtime) {
            max_runtime = params[i].runtime;
        }
        if (params[i].runtime < min_runtime) {
            min_runtime = params[i].runtime;
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 3) {
        total = std::atol(argv[1]);
        pdegree = std::atoi(argv[2]);
        simple = std::atoi(argv[3]);
    }

    cout << "Thread: " << pdegree << " total: " << total << endl;

    Tracer tracer;
    tracer.startTime();
    initLoads();
    cout << "Init: " << tracer.getRunTime() << endl;
    tracer.startTime();

    xh = (index_btree *) _mm_malloc(sizeof(index_btree), 64);
    new(xh) index_btree();
    xh->init(1);

    cout << "Table init: " << tracer.getRunTime() << endl;
    tracer.startTime();

    if (simple) {
        simpleOperationTests();
    } else {
        insert();
    }

    cout << "Insert: " << tracer.getRunTime() << " minTime: " << min_runtime << " maxTime: " << max_runtime
         << " avgTime: " << ((double) total_runtime / pdegree) << " avgTpt: "
         << ((double) total * pdegree / total_runtime) << endl;

    delete xh;
    delete[] loads;
    return 0;
}