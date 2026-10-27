#pragma once
// system_status.h - the "contract" between Member 5's menu and everyone else.
//
// Other modules do NOT need to know anything about the serial menu.
// They only write one small function that fills in a struct, and
// register it. The menu calls it whenever the user types a command.

#include <stdbool.h>

#define STATUS_ROOM_COUNT 2          // Lecture Room 1 and Lecture Room 2

// ---- The data each provider fills in ----
typedef struct {
    int inside_count;                // people currently inside the building
} building_status_t;

typedef struct {
    bool occupied;                   // is anyone detected in the room?
    bool light_on;                   // are the lights currently on?
} room_status_t;

typedef struct {
    int  open_minute;                // minutes since midnight, e.g. 8:00 = 480
    int  close_minute;               // e.g. 18:00 = 1080
    bool access_allowed_now;         // is the building open right now?
} schedule_status_t;

// ---- Provider function types ----
// Each returns true on success, false if the data isn't available.
// 'room' is 1 or 2.
typedef bool (*building_provider_t)(building_status_t *out);
typedef bool (*room_provider_t)(int room, room_status_t *out);
typedef bool (*schedule_provider_t)(schedule_status_t *out);

// ---- Registration: called by the OTHER members' modules at startup ----
void status_register_building(building_provider_t fn);
void status_register_room(room_provider_t fn);
void status_register_schedule(schedule_provider_t fn);

// ---- Reading: called by the serial menu ----
// All return false if nothing has been registered yet.
bool status_get_building(building_status_t *out);
bool status_get_room(int room, room_status_t *out);
bool status_get_schedule(schedule_status_t *out);

// Registers fake data so the menu can be tested before the real modules
// exist. DELETE the call (and demo_providers.c) when the real ones arrive.
void demo_providers_register(void);