// Copyright (C) strawberryhacker

#ifndef EVENT_H
#define EVENT_H

#include <citrus/types.h>

// Event
//
// Mouse clicked
// Mouse double clicked
// Mouse released
// Mouse move
// Mouse enter
// Mouse leave

enum event_type {
    EVENT_BTN_PRESS,
    EVENT_BTN_RELEASE,
    EVENT_MOUSE_ENTER,
    EVENT_MOUSE_LEAVE,
    EVENT_MOUSE_MOVE
};

struct event {
    enum event_type type;
    u32 keycode;
};

#endif
