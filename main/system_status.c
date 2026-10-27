// system_status.c - stores the registered provider functions.
#include "system_status.h"

// 'volatile' because one task may register while the menu task reads.
// Writing a 32-bit pointer is a single operation on the ESP32, so this is
// safe for registration at startup. (The real shared DATA still needs
// mutexes in each owner's module - that is Member 3's job.)
static volatile building_provider_t s_building = 0;
static volatile room_provider_t     s_room     = 0;
static volatile schedule_provider_t s_schedule = 0;

void status_register_building(building_provider_t fn) { s_building = fn; }
void status_register_room(room_provider_t fn)         { s_room = fn; }
void status_register_schedule(schedule_provider_t fn) { s_schedule = fn; }

bool status_get_building(building_status_t *out)
{
    building_provider_t fn = s_building;     // copy once, then test and call
    return fn ? fn(out) : false;
}

bool status_get_room(int room, room_status_t *out)
{
    room_provider_t fn = s_room;
    if (!fn || room < 1 || room > STATUS_ROOM_COUNT) return false;
    return fn(room, out);
}

bool status_get_schedule(schedule_status_t *out)
{
    schedule_provider_t fn = s_schedule;
    return fn ? fn(out) : false;
}