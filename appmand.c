#include <stdio.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define NUMBER_OF_THREADS 1

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
struct thread_info {
        pthread_t thread;

} thread_info;

/*
 * Application structure.
 * path         : execution path.
 * group        : cgroup gproup name.
 * perms        : tbd
 */
struct application {
        char* path;
        char* group;
        char* perms;
} application;

/*
 * Application list.
 */
application *applist;

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

/*
 * Read application list from /etc/appmand/*.mf.
 */
int get_applist() {
        
        
        return 0;
}

/*
 * Request handling with dbus.
 */
void *request_handler(void *targs) {
        thread_info *tinfo = targs;
        
        /* TODO: Read application manifests from /etc/appmand */
           
        /* TODO: Listen and answer D-Bus method requests to run applications. */
        
        pthread_exit(NULL);
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
 * Reply for dbus messages.
 */
void reply (DBusMessage* msg, DBusConnection* conn) {
        DBusMessage* reply;
        DBusMessageIter args;
        int stat = 0;
        dbus_uint32_t level = 21614;
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
        
        
        /* Signal the condition variable that application has executed. */
        rc = pthread_mutex_lock(&mutex);
        rc = pthread_cond_signal(&child_exists);
        rc = pthread_mutex_unlock(&mutex);
        
        /* Create a reply from the message. */
        reply = dbus_message_new_method_return(msg);

        /* Add the arguments to the reply. */
        dbus_message_iter_init_append(reply, &args);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &stat)) { 
                fprintf(stderr, "Out Of Memory!\n"); 
                exit(1);
        }
        
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &level)) { 
                fprintf(stderr, "Out Of Memory!\n"); 
                exit(1);
        }

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
        DBusMessage* reply;
        DBusMessageIter args;
        DBusConnection* conn;
        DBusError err;
        int ret;
        char* param;

        /* Initialize error. */
        dbus_error_init(&err);

        /* Connect to the bus and check for errors. */
        conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
        
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

        while (true) {
                /* Non-blocking read of the next available message. */
                dbus_connection_read_write(conn, 0);
                msg = dbus_connection_pop_message(conn);

                if (msg == NULL) { 
                        sleep(1); 
                        continue; 
                }

                /* Check this is a method call for the right interface & method. */
                if (dbus_message_is_method_call(msg, "appman.method.Type", "RunApp")) {
                        /* TODO: Should seperate reply and requested command */
                        reply(msg, conn);
                }

                /* Free the message. */
                dbus_message_unref(msg);
        }

        /* Close the connection. */
        //dbus_connection_close(conn);

        dbus_connection_unref(conn);
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
        
        /* Register signal handler. */
        memset(&act, 0, sizeof(struct sigaction));
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = signal_handler;
        sigaction(SIGCHLD, &act, NULL);
        
        /* Thread creation */
        for (i = 0; i < NUMBER_OF_THREADS; i++) {
                rc = pthread_create(&tinfo[i].thread, NULL, 
                                    &request_handler, (void*)&tinfo[i]);
                if (rc) {
                        handle_error_en(rc, "pthread_create");
                }
        }
        /* Time for thread initializations. */
        sleep(3);
        
        /*
         * Infinite loop of process waits. If there are processes 
         * to wait, wait any of them. If not, wait on the given condition 
         * variable. When signaled, redo.
         */
        while (1) {
                rc = waitpid((pid_t)(-1), &status, 0);
                if (rc <= 0) {
                        if (errno == ECHILD) {
                                /* Does not have child, wait on condition. */
                                rc = pthread_mutex_lock(&mutex);
                                rc = pthread_cond_wait(&child_exists, &mutex);
                                rc = pthread_mutex_unlock(&mutex);
                                continue;
                        } else if (errno == EINVAL) {
                                handle_error_en(errno, "waitpid:invalid options");
                        }
                          
                } else {
                        handle_termination(rc, status);
                }
        }
        
        /* Join with threads */
        for (i = 0; i < NUMBER_OF_THREADS; i++) {
                rc = pthread_join(tinfo[i].thread, &res);
                
                if (rc) {
                        handle_error_en(rc, "pthread_join");
                }
                
                /* Check return value if needed */
                free(res);
        }
        
        free(tinfo);
        exit(EXIT_SUCCESS);
}
