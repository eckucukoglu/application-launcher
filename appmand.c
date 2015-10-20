#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU /* For recursive mutex initializer. */
#include <pthread.h>
#include <dbus/dbus.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include "cJSON.h"
#include <unistd.h>

#define NUMBER_OF_THREADS 1

/* Maximum number of allowed applications that defined in manifests. */
#define MAX_NUMBER_APPLICATIONS 50

/* 
 * Application manifests are stored in here 
 * like <appid>.mf as json structure. 
 */
#define MANIFEST_DIR "/etc/appmand/"

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0);
        
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0);

/* 
 * Recursive mutex is used since thread that grabs the mutex 
 * must be the same thread that release the mutex.
 */
pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/*
 * Global condition variable, means there exists children to wait.
 */
pthread_cond_t child_exists = PTHREAD_COND_INITIALIZER;

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
 * group        : cgroup gproup name.
 * perms        : tbd.
 */
typedef struct application {
        int id;
        char* path;
        char* name;
        char* group;
        /* char* perms; */
} application;

/*
 * Application list.
 */
application APPLIST[MAX_NUMBER_APPLICATIONS];

/*
 * 
 */
const char *reasonstr(int signal, int code) {
        if (signal == SIGCHLD) {
                switch(code) {
                case CLD_EXITED:        return("CLD_EXITED");
                case CLD_KILLED:        return("CLD_KILLED");
                case CLD_DUMPED:        return("CLD_DUMPED");
                case CLD_TRAPPED:       return("CLD_TRAPPED");
                case CLD_STOPPED:       return("CLD_STOPPED");
                case CLD_CONTINUED:     return("CLD_CONTINUED");
                }
        }

        return "unknown";
}

void json_to_application (char *text, int index) {
	cJSON *root;
	
	root = cJSON_Parse(text);
	if (!root) {
	        printf("Error before: [%s]\n",cJSON_GetErrorPtr());
	} else {
		APPLIST[index].id = cJSON_GetObjectItem(root,"id")->valueint;
		APPLIST[index].path = cJSON_GetObjectItem(root,"path")->valuestring;
		APPLIST[index].name = cJSON_GetObjectItem(root,"name")->valuestring;
		APPLIST[index].group = cJSON_GetObjectItem(root,"group")->valuestring;

#ifdef DEBUG
                printf("Application(%d): %s, on path %s\n", APPLIST[index].id, 
                        APPLIST[index].name, APPLIST[index].path);
                fflush(stdout);
#endif /* DEBUG */

		/* Fix pointer problem and free cjson object. */
		/* cJSON_Delete(root); */
	}
}

/*
 * Reads application list from manifest directory.
 * Returns 0 on success.
 */
int get_applist() {
        DIR * d;
        struct dirent *entry;
        FILE *file;
        long size;
        char *filecontent;
        int app_index = 0;
        int ret;
        
        ret = chdir(MANIFEST_DIR);
        if (ret!=0) {
                perror("Error:");
        }
        
        d = opendir(MANIFEST_DIR);
        
        if (! d) {
                fprintf(stderr, "Cannot open directory '%s': %s\n",
                        MANIFEST_DIR, strerror (errno));
                exit (EXIT_FAILURE);
        }
        
        while ((entry = readdir(d)) != NULL && 
                app_index < MAX_NUMBER_APPLICATIONS) {
                
                if (!strcmp (entry->d_name, "."))
                        continue;
                if (!strcmp (entry->d_name, ".."))    
                        continue;
                
                file = fopen(entry->d_name, "r");
                if (!file) {
                        fprintf(stderr, "Error : Failed to open entry file\n");
                        /* TODO: Close directory. */
                        /* perror(file),exit(1); */
                        exit (EXIT_FAILURE);
                }
                fseek(file, 0, SEEK_END);
                size = ftell(file);
                rewind(file);
                filecontent = calloc(1, size+1);
                if (!filecontent) {
                        fclose(file);
                        fputs("memory alloc fails",stderr);
                        exit(1);
                }
                
                if (1 != fread(filecontent, size, 1, file)) {
                        fclose(file);
                        free(filecontent);
                        fputs("entire read fails",stderr);
                        exit(1);
                }
                
                json_to_application(filecontent, app_index);
                app_index++;
                
                free(filecontent);
                fclose(file);
        }
        
        
        /* Close the directory. */
        if (closedir (d)) {
                fprintf (stderr, "Could not close '%s': %s\n",
                        MANIFEST_DIR, strerror (errno));
                exit (EXIT_FAILURE);
        }
        
        return 0;
}

/*
 * 
 */
int run_app (int appid) {
        int rc;
        
        printf("Running application id: %d\n", appid);       
        pid_t pid = fork();
        
        if (pid == 0) {
                printf("Child process is going to execute %s\n", 
                        APPLIST[appid].name);
                        
                rc = execl(APPLIST[appid].path, APPLIST[appid].name, (char*)NULL);
                if (rc == -1) {
                        handle_error("execl");
                }
        } else if (pid < 0) {
                return -1;
        }
        
        return 0;
}

/*
 * Reply for dbus messages.
 */
void reply(DBusMessage* msg, DBusConnection* conn) {
        DBusMessage* reply;
        DBusMessageIter args;
        /* dbus_uint32_t level = 21614; */
        dbus_uint32_t serial = 0;
        char* param = "";
        int rc;

        /* Read the arguments. */
        if (!dbus_message_iter_init(msg, &args))
                fprintf(stderr, "Message has no arguments!\n"); 
        else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                fprintf(stderr, "Argument is not string!\n"); 
        else 
                dbus_message_iter_get_basic(&args, &param);

        printf("Method called with %s\n", param);

        /* Run the corresponding application */
        rc = run_app(atoi(param));
        if (rc == 0) {
                /* Signal the condition variable that application has executed. */
                pthread_mutex_lock(&mutex);
                pthread_cond_signal(&child_exists);
                pthread_mutex_unlock(&mutex);
        }
        
        printf("Creating reply message with %d\n", rc);
        
        /* Create a reply from the message. */
        reply = dbus_message_new_method_return(msg);

        /* Add the arguments to the reply. */
        dbus_message_iter_init_append(reply, &args);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &rc)) { 
                fprintf(stderr, "Out Of Memory!\n"); 
                exit(1);
        }
        
        /*
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &level)) { 
                fprintf(stderr, "Out Of Memory!\n"); 
                exit(1);
        }
        */

        /* Send the reply && flush the connection. */
        if (!dbus_connection_send(conn, reply, &serial)) {
                fprintf(stderr, "Out Of Memory!\n"); 
                exit(1);
        }
        
        dbus_connection_flush(conn);

        /* Free the reply. */
        dbus_message_unref(reply);
}

/*
 * Expose a method call and wait for it to be called.
 */
void listen () {
        DBusMessage* msg;
        DBusMessageIter args;
        DBusConnection* conn;
        DBusError err;
        int ret;
        char* param;
        
        printf("Inside listen server.\n");
        
        /* Initialize error. */
        dbus_error_init(&err);

        /* Connect to the bus and check for errors. */
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        
        if (dbus_error_is_set(&err)) { 
                fprintf(stderr, "Connection Error (%s)\n", err.message); 
                dbus_error_free(&err); 
        }
        
        if (NULL == conn) {
                fprintf(stderr, "Connection Null\n"); 
                exit(1); 
        }

        /* Request our name on the bus and check for errors. */
        ret = dbus_bus_request_name(conn, "appman.method.server",
                                DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
                                
        if (dbus_error_is_set(&err)) { 
                fprintf(stderr, "Name Error (%s)\n", err.message); 
                dbus_error_free(&err);
        }
        
        if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
                fprintf(stderr, "Not Primary Owner (%d)\n", ret);
                exit(1); 
        }

        printf("Entering listen loop.\n");
        
        while (1) {
                /* Non-blocking read of the next available message. */
                dbus_connection_read_write(conn, 0);
                msg = dbus_connection_pop_message(conn);

                if (msg == NULL) { 
                        sleep(1); 
                        continue; 
                }
                
                printf("D-bus message is catched.\n");
                
                /* Check this is a method call for the right interface & method. */
                if (dbus_message_is_method_call(msg, "appman.method.Type", "runapp")) {
                        reply(msg, conn);
                } else {
                        printf("Anything different came from dbus.\n");
                }

                /* Free the message. */
                dbus_message_unref(msg);
        }

        /* Close the connection. */
        //dbus_connection_close(conn);

        dbus_connection_unref(conn);
}

/*
 * Request handling with dbus.
 */
void *request_handler(void *targs) {
        sigset_t set;
        int s;
        thread_info *tinfo = targs;
        
        printf("Starting thread.\n");
        
        /* Mask signal handling for threads other than main thread. */
        sigemptyset(&set);
        sigaddset(&set,SIGCHLD);
        s = pthread_sigmask(SIG_BLOCK,&set,NULL);
        if (s != 0)
                handle_error_en(s, "pthread_sigmask");
        
        printf("Thread listening d-bus methods\n");
        /* Listen and answer D-Bus method requests to run applications. */
        listen();
        
        pthread_exit(NULL);
}

/*
 * Handle termination status of child process.
 */
void handle_status(int pid, int status) {
        /* Child terminated normally. */
        if (WIFEXITED(status)) {
                printf("terminated normally.\n");
        } 
        /* Child terminated by signal. */
        else if (WIFSIGNALED(status)) {
                printf("terminated by signal.\n");
        }
        /* Child produced a core dump. */
        else if (WCOREDUMP(status)) {
                printf("produced a core dump.\n");
        }
        /* Child stopped by signal. */
        else if (WIFSTOPPED(status)) {
                printf("stopped by signal.\n");
        }
        /* Other conditions. */
        else {
                printf("anything then handled reasons.");
        }
}

/*
 * Signal handling for main process.
 */
void signal_handler(int signo, siginfo_t *info, void *p) {
        int status;
        int rc;
        
        printf("signal %d received:\n"
                "si_errno %d\n"    /* An errno value */
                "si_code %s\n"     /* Signal code */
                ,signo,
                info->si_errno,
                reasonstr(signo, info->si_code));

        if (signo == SIGCHLD) {
                printf(
                        "si_pid %d\n"      /* Sending process ID */
                        "si_uid %d\n",     /* Real user ID of sending process */
                        "si_status %d\n"   /* Exit value or signal */
                        "si_utime %d\n"    /* User time consumed */
                        "si_stime %d\n",   /* System time consumed */
                        info->si_pid,
                        info->si_uid,
                        info->si_status,
                        info->si_utime,
                        info->si_stime);
                /* 
                 * Multiple child processes could terminate 
                 * while one is in the process being reaped.
                 * Loop ensures that any zombies which existed 
                 * prior to invocation of the handler function 
                 * will be reaped.
                 */
                while ((rc = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
                        handle_status(rc, status);
                }
        }
}

/*
 *
 *
 */
int main (int argc, char *argv[]) {
        thread_info *tinfo;
        int rc;
        int i;
        void *res;
        struct sigaction act;
        int status;
        
        /* Memory allocation for thread arguments */
        tinfo = malloc(sizeof(thread_info) * NUMBER_OF_THREADS);
        if (tinfo == NULL) {
                handle_error("malloc for thread_info");
        }
        
        printf("Calling get_applist\n");
        
        /* Fill application list, once per lifetime. */
        rc = get_applist();
        if (rc) {
                handle_error("get_applist");
        }
        
        printf("Register signal handler\n");
        
        /* Register signal handler. */
        memset(&act, 0, sizeof(struct sigaction));
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = signal_handler;
        sigaction(SIGCHLD, &act, NULL);
        
        printf("Thread creation\n");
        
        /* Thread creation */
        for (i = 0; i < NUMBER_OF_THREADS; i++) {
                rc = pthread_create(&tinfo[i].thread, NULL, 
                                    request_handler, (void*)&tinfo[i]);
                if (rc) {
                        handle_error_en(rc, "pthread_create");
                }
        }
        /* Time for thread initializations. */
        sleep(1);
        
        printf("Entering main process while loop\n");
        
        /*
         * Infinite loop of process waits. If there are processes 
         * to wait, wait any of them. If not, wait on the given condition 
         * variable. When signaled, redo.
         */
        while (1) {
                printf("Looping in main loop\n");
                rc = waitpid((pid_t)(-1), &status, 0);
                printf("Returned waitpid from main loop:%d\n", rc);
                if (rc <= 0) {
                        if (errno == ECHILD) {
                                printf("No child exist, waiting condition.\n");
                                /* Does not have child, wait on condition. */
                                rc = pthread_mutex_lock(&mutex);
                                rc = pthread_cond_wait(&child_exists, &mutex);
                                rc = pthread_mutex_unlock(&mutex);
                                printf("Condition unlocked.\n");
                                continue;
                        } else if (errno == EINVAL) {
                                handle_error_en(errno, "waitpid:invalid options");
                        }
                          
                } else {
                        handle_status(rc, status);
                }
        }
        
        printf("Joining with threads\n");
        
        /* Join with threads */
        for (i = 0; i < NUMBER_OF_THREADS; i++) {
                rc = pthread_join(tinfo[i].thread, &res);
                
                if (rc) {
                        handle_error_en(rc, "pthread_join");
                }
                
                /* Check return value if needed. */
                free(res);
        }
        
        free(tinfo);
        exit(EXIT_SUCCESS);
}
