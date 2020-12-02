// Copyright (C) strawberryhacker

#ifndef SEM_H
#define SEM_H

#include <citrus/sem.h>
#include <citrus/list.h>

struct sem {
    u32 lock;
    struct list_head wait_list;
}

void sem_wait(struct sem* sem);

#endif
