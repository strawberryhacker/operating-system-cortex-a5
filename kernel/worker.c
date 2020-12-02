// Copyright (C) strawberryhacker

#include <citrus/worker.h>
#include <citrus/print.h>
#include <citrus/list.h>
#include <citrus/thread.h>

struct worker {
    struct thread* thread;

    struct list_node node;
};

struct work {
    void (*work_func)(void*);
    void* arg;
    struct list_node node;
};

struct work_queue {
    struct list_node work_list;
};

void worker_init(void)
{
    print("Worker init\n");
}
