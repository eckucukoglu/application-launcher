#include <stdio.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <errno.h>

#define NUMBER_OF_THREADS 1

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0);
        
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0);

struct thread_info {
        pthread_t thread;

} thread_info;

struct application {
        char* path;
        char* group;
        char* permissions;
} application;

void *waitProcess(void *targs);

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
                                    &waitProcess, &tinfo[i]);
                if (rc) {
                        handle_error_en(rc, "pthread_create");
                }
        }
        
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


/*
 * Waits and handles return conditions of executed applications.
 * 
 */
void *waitProcess(void *targs) {
        thread_info *tinfo = targs;
        
        /* TODO: Wait condition variable to know that 
                 child process created. If there exist child processes
                 to be waited, wait all. */
        
        
        pthread_exit(NULL);
}

/*
 * Expose a method call and wait for it to be called.
 */
void listen () {
        
        /* TODO: Reply to method call. */
}

/*
 * Reply
 */
void reply () {
        
        pid_t pID;
        
        pID = fork();
        
        if (pID == 0) {
                /* TODO: Child process does execution */
                
        } else if (pID > 0) {
                /* TODO: Parent process */
                
        } else {
                handle_error("fork");        
        } 

}
