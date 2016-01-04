#ifndef _APPMAND_H_
#define _APPMAND_H_

#define _GNU_SOURCE /* For recursive mutex initializer. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"

#define NUMBER_OF_THREADS 1
/* Maximum number of applications. */
#define MAX_NUMBER_APPLICATIONS 50
/* Manifest storage. */
#define MANIFEST_DIR "/etc/appmand/"

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0);

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0);

/*
 * Thread information.
 * Passed as an argument from main process to threads.
 */
typedef struct thread_info {
        pthread_t thread;
} thread_info;

/*
 * Application structure.
 * id           : application id.
 * path         : execution path.
 * group        : cgroup group name.
 * perms        : tbd.
 */
typedef struct application {
        int id;
        char* path;
        char* name;
        char* group;
        /* char* perms; */
} application;


const char *reasonstr(int, int);
void json_to_application (char *, int);

/*
 * Reads application list from manifest directory.
 * Returns 0 on success.
 */
int get_applist();

/*
 * Forks and execs application with the given id in child process.
 */
int run_app (int);

/*
 * Reply for dbus messages.
 */
void reply(DBusMessage*, DBusConnection*);

/*
 * Expose a method call and wait for it to be called.
 */
void listen ();

/*
 * Request handling with dbus.
 */
void *request_handler(void *);

/*
 * Handle termination status of child process.
 */
void handle_status(int, int);

/*
 * Signal handling for main process.
 */
void signal_handler(int, siginfo_t *, void *);

/* Application list. */
application APPLIST[MAX_NUMBER_APPLICATIONS];

#endif  /* not defined _APPMAND_H_ */
