// serial_cli.c - Member 5: serial command-line interface (Layer 2)
//
// Layer 1 gave us the shell (read a line, split it, dispatch to a handler).
// Layer 2 adds commands that show live data from the other modules through
// the provider interface in system_status.h.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"          // esp_get_free_heap_size()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_card_module.h"      // Member 4: sd_read_logs()
#include "system_status.h"       // providers from every other member
#include "serial_cli.h"

#define CLI_LINE_MAX   64
#define CLI_ARGS_MAX   4
#define CLI_PROMPT     "CoCIS> "

typedef void (*cli_handler_t)(int argc, char **argv);

typedef struct {
    const char   *name;
    const char   *help;
    cli_handler_t handler;
} cli_cmd_t;

static void cmd_help(int argc, char **argv);
static void cmd_status(int argc, char **argv);
static void cmd_building(int argc, char **argv);
static void cmd_rooms(int argc, char **argv);
static void cmd_schedule(int argc, char **argv);
static void cmd_logs(int argc, char **argv);

static const cli_cmd_t s_cmds[] = {
    { "help",     "List all commands",                         cmd_help     },
    { "status",   "Full system overview",                      cmd_status   },
    { "building", "People currently inside the building",      cmd_building },
    { "rooms",    "Room occupancy and lights ('rooms 1' for one room)", cmd_rooms },
    { "schedule", "Building access hours and whether open now", cmd_schedule },
    { "logs",     "Show last N log lines (default 5, max 20)", cmd_logs     },
};
#define CLI_CMD_COUNT (sizeof(s_cmds) / sizeof(s_cmds[0]))

// ---------------------------------------------------------------
// Printing helpers - shared by "status" and the individual commands
// ---------------------------------------------------------------
static void print_building(void)
{
    building_status_t b;
    if (status_get_building(&b)) {
        printf("Building   : %d person(s) inside\n", b.inside_count);
    } else {
        printf("Building   : not connected yet\n");
    }
}

static void print_room(int room)
{
    room_status_t r;
    if (status_get_room(room, &r)) {
        printf("Room %d     : %s, lights %s\n", room,
               r.occupied ? "occupied" : "empty",
               r.light_on ? "ON" : "OFF");
    } else {
        printf("Room %d     : not connected yet\n", room);
    }
}

static void print_schedule(void)
{
    schedule_status_t s;
    if (status_get_schedule(&s)) {
        // minutes since midnight -> HH:MM
        printf("Schedule   : %02d:%02d to %02d:%02d, building is %s now\n",
               s.open_minute / 60,  s.open_minute % 60,
               s.close_minute / 60, s.close_minute % 60,
               s.access_allowed_now ? "OPEN" : "CLOSED");
    } else {
        printf("Schedule   : not connected yet\n");
    }
}

// ---------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------
static void cmd_help(int argc, char **argv)
{
    printf("Available commands:\n");
    for (size_t i = 0; i < CLI_CMD_COUNT; i++) {
        printf("  %-9s %s\n", s_cmds[i].name, s_cmds[i].help);
    }
}

static void cmd_status(int argc, char **argv)
{
    uint32_t up_s = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
    printf("Uptime     : %lu s\n", (unsigned long)up_s);
    printf("Free heap  : %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    print_building();
    for (int r = 1; r <= STATUS_ROOM_COUNT; r++) print_room(r);
    print_schedule();
}

static void cmd_building(int argc, char **argv) { print_building(); }

static void cmd_rooms(int argc, char **argv)
{
    if (argc >= 2) {
        int r = atoi(argv[1]);
        if (r < 1 || r > STATUS_ROOM_COUNT) {
            printf("Room must be 1 to %d.\n", STATUS_ROOM_COUNT);
            return;
        }
        print_room(r);
    } else {
        for (int r = 1; r <= STATUS_ROOM_COUNT; r++) print_room(r);
    }
}

static void cmd_schedule(int argc, char **argv) { print_schedule(); }

static void cmd_logs(int argc, char **argv)
{
    int n = 5;
    if (argc >= 2) n = atoi(argv[1]);
    esp_err_t err = sd_read_logs(n);
    if (err == ESP_ERR_NOT_FOUND) {
        printf("No log file yet.\n");
    } else if (err != ESP_OK) {
        printf("Could not read logs (%s)\n", esp_err_to_name(err));
    }
}

// ---------------------------------------------------------------
// Parsing and dispatch
// ---------------------------------------------------------------
static void cli_execute(char *line)
{
    char *argv[CLI_ARGS_MAX];
    int argc = 0;

    char *save = NULL;
    char *tok = strtok_r(line, " \t", &save);
    while (tok && argc < CLI_ARGS_MAX) {
        argv[argc++] = tok;
        tok = strtok_r(NULL, " \t", &save);
    }
    if (argc == 0) return;

    for (size_t i = 0; i < CLI_CMD_COUNT; i++) {
        if (strcmp(argv[0], s_cmds[i].name) == 0) {
            s_cmds[i].handler(argc, argv);
            return;
        }
    }
    printf("Unknown command '%s'. Type 'help'.\n", argv[0]);
}

// ---------------------------------------------------------------
// The task that reads the keyboard (unchanged from Layer 1)
// ---------------------------------------------------------------
static void cli_task(void *arg)
{
    char line[CLI_LINE_MAX];
    int  len = 0;
    int  prev = 0;

    printf("\nCoCIS Smart Control - type 'help'\n" CLI_PROMPT);
    fflush(stdout);

    while (1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (c == '\n' && prev == '\r') { prev = c; continue; }
        prev = c;

        if (c == '\r' || c == '\n') {
            putchar('\n');
            line[len] = '\0';
            cli_execute(line);
            len = 0;
            printf(CLI_PROMPT);
            fflush(stdout);
        } else if (c == 0x08 || c == 0x7f) {
            if (len > 0) {
                len--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127 && len < CLI_LINE_MAX - 1) {
            line[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

void serial_cli_start(void)
{
    xTaskCreate(cli_task, "cli", 4096, NULL, 3, NULL);
}
