#include <stdio.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/wait.h>

#define NUMBER_OF_THREADS 1

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0);
        
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0);

/* 
 * Global mutex, recursive since thread that grabs the mutex 
 * must be the same thread that release the mutex.
 */
pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/*
 * Global condition variable, means there exists children to wait.
 */
pthread_cond_t child_exists = PTHREAD_COND_INITIALIZER;

struct thread_info {
        pthread_t thread;

} thread_info;

struct application {
        char* path;
        char* group;
        char* permissions;
} application;

/*
 * Infinite loop of process waits. If there are processes 
 * to wait, wait any. If not wait on the given condition 
 * variable, when signaled, redo.
 */
void *wait_process_loop(void *targs) {
        thread_info *tinfo = targs;
        int rc;
        
        while (1) {
                
                /* wait for any child. */
                
                /* handle return code then continue */
                
                /* in case of erronous wait, check condition */
                
                rc = pthread_mutex_lock(&mutex);
                rc = pthread_cond_wait(&child_exists, &mutex);
                rc = pthread_mutex_unlock(&mutex);
        }
        
        pthread_exit(NULL);
}

/*
 * Reply
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

int main (int argc, char *argv[]) {
        thread_info *tinfo;
        int rc;
        int i;
        void *res;
        
        /* Memory allocation for thread arguments */
        tinfo = malloc(sizeof(thread_info) * NUMBER_OF_THREADS);
        if (tinfo == NULL) {
                handle_error("malloc for thread_info");
        }
        
        /* Thread creation */
        for (i = 0; i < NUMBER_OF_THREADS; i++) {
                rc = pthread_create(&tinfo[i].thread, NULL, 
                                    &wait_process_loop, (void*)&tinfo[i]);
                if (rc) {
                        handle_error_en(rc, "pthread_create");
                }
        }
        
        sleep(3);
        
        /* TODO: Create condition variable between threads to alert that
                 new process created. */
        
        /* TODO: Read application manifests from /etc/appmand */
           
        /* TODO: Listen and answer D-Bus method requests to run applications. */
        
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
