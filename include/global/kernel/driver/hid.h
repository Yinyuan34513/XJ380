#pragma once

#include "../../../include.h"

typedef struct _KEY_EVENT_RING
{
    uint16_t CNT;
    uint16_t NID;
    uint16_t EID;
} KEY_EVENT_RING;

uint32_t ConfigureHID(USB_COMMON *common);
