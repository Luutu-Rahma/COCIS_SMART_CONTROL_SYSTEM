#pragma once

// Starts the serial command-line task. Call once from app_main(),
// after the other modules have been initialised.
void serial_cli_start(void);

// serial_cli.c - Member 5: serial command-line interface (Layer 1)
//
// How it works:
//   1. A FreeRTOS task reads characters one at a time from the serial port.
//   2. It echoes them back (the terminal doesn't do that for us) and
//      builds up a line until Enter is pressed.
//   3. The line is split into words ("logs 10" -> "logs", "10").
//   4. The first word is looked up in a command table, and the matching
//      handler function is called with the words as arguments.
//
// Adding a new command later = write one handler + add one table row.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"          // esp_get_free_heap_size()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_card_module.h"      // Member 4's module: sd_read_logs()
#include "serial_cli.h"

#define CLI_LINE_MAX   64        // longest command line we accept
#define CLI_ARGS_MAX   4         // most words we split a line into
#define CLI_PROMPT     "CoCIS> "

// A command handler receives the words typed by the user.
// argv[0] is the command name itself, like in a normal C main().
typedef void (*cli_handler_t)(int argc, char **argv);

// One row of the command table.
typedef struct {
    const char   *name;          // what the user types
    const char   *help;          // one-line description shown by "help"
    cli_handler_t handler;       // function to run
} cli_cmd_t;

// ---- Handler prototypes (bodies are below the table) ----
static void cmd_help(int argc, char **argv);
static void cmd_status(int argc, char **argv);
static void cmd_logs(int argc, char **argv);

// ---- The command table: the single place that defines the menu ----
static const cli_cmd_t s_cmds[] = {
    { "help",   "List all commands",                        cmd_help   },
    { "status", "Show system uptime and free memory",       cmd_status },
    { "logs",   "Show last N log lines (default 5, max 20)", cmd_logs   },
};
#define CLI_CMD_COUNT (sizeof(s_cmds) / sizeof(s_cmds[0]))

// ---------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------
static void cmd_help(int argc, char **argv)
{
    printf("Available commands:\n");
    for (size_t i = 0; i < CLI_CMD_COUNT; i++) {
        printf("  %-8s %s\n", s_cmds[i].name, s_cmds[i].help);
    }
}

static void cmd_status(int argc, char **argv)
{
    // Ticks since boot -> milliseconds -> seconds.
    uint32_t up_s = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
    printf("Uptime     : %lu s\n", (unsigned long)up_s);
    printf("Free heap  : %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    // Later layers will add: building occupancy, room occupancy, lights...
}

static void cmd_logs(int argc, char **argv)
{
    int n = 5;                       // default if the user gives no number
    if (argc >= 2) {
        n = atoi(argv[1]);           // "logs 10" -> 10
    }
    // sd_read_logs() prints the lines itself and clamps n to 1..20.
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

    // strtok_r splits on spaces. It modifies 'line' in place, which is
    // fine because we own this buffer.
    char *save = NULL;
    char *tok = strtok_r(line, " \t", &save);
    while (tok && argc < CLI_ARGS_MAX) {
        argv[argc++] = tok;
        tok = strtok_r(NULL, " \t", &save);
    }
    if (argc == 0) return;           // blank line

    for (size_t i = 0; i < CLI_CMD_COUNT; i++) {
        if (strcmp(argv[0], s_cmds[i].name) == 0) {
            s_cmds[i].handler(argc, argv);
            return;
        }
    }
    printf("Unknown command '%s'. Type 'help'.\n", argv[0]);
}

// ---------------------------------------------------------------
// The task that reads the keyboard
// ---------------------------------------------------------------
static void cli_task(void *arg)
{
    char line[CLI_LINE_MAX];
    int  len = 0;
    int  prev = 0;                   // previous character, to handle "\r\n"

    printf("\nCoCIS Smart Control - type 'help'\n" CLI_PROMPT);
    fflush(stdout);

    while (1) {
        int c = getchar();           // returns EOF (-1) when nothing is typed
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));   // yield so other tasks can run
            continue;
        }

        // Some terminals send "\r\n" for Enter; skip the second half.
        if (c == '\n' && prev == '\r') { prev = c; continue; }
        prev = c;

        if (c == '\r' || c == '\n') {        // Enter: run the line
            putchar('\n');
            line[len] = '\0';
            cli_execute(line);
            len = 0;
            printf(CLI_PROMPT);
            fflush(stdout);
        } else if (c == 0x08 || c == 0x7f) { // Backspace / Delete
            if (len > 0) {
                len--;
                printf("\b \b");             // move back, blank it, move back
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127 && len < CLI_LINE_MAX - 1) {
            line[len++] = (char)c;           // normal printable character
            putchar(c);                      // echo it
            fflush(stdout);
        }
    }
}

void serial_cli_start(void)
{
    // 4096-byte stack, priority 3 - plenty for a text shell.
    xTaskCreate(cli_task, "cli", 4096, NULL, 3, NULL);
}`