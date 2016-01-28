#include "appmand.h"

/* Thread that grabs mutex, must be the same thread that release mutex. */
pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* Means there exists children to wait. */
pthread_cond_t child_exists = PTHREAD_COND_INITIALIZER;

const char *reasonstr(int signal, int code) {
    if (signal == SIGCHLD) {
        switch(code) {
        case CLD_EXITED:
            return("CLD_EXITED");
        case CLD_KILLED:
            return("CLD_KILLED");
        case CLD_DUMPED:
            return("CLD_DUMPED");
        case CLD_TRAPPED:
            return("CLD_TRAPPED");
        case CLD_STOPPED:
            return("CLD_STOPPED");
        case CLD_CONTINUED:
            return("CLD_CONTINUED");
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
	    char field_names[7][50] = {
	        "id",
	        "path",
            "name",
            "group",
            "prettyname",
            "color",
            "iconpath"
	    };
	    char* fields[7];
	    int mandatory_field_count = 2;
	    int optional_field_count = 5;
	    cJSON *child;
	    int i;
	    unsigned int id;
	    
	    /* First read mandatory fields. */
	    i = 0;
	    while (mandatory_field_count > 0) {
	        child = cJSON_GetObjectItem(root, field_names[i]);
	        if (child != NULL) {
	            if (i == 0)
	                id = child->valueint;
	            else
	                fields[i] = child->valuestring;
	            
	        } else {
	            printf("Can not find cjson object %s\n", field_names[i]);
                exit(1);	    
	        }
	        
	        i++;
	        mandatory_field_count--;
	    }
	    
	    /* Then read optional fields. */ 
	    while (optional_field_count > 0) {
	        child = cJSON_GetObjectItem(root, field_names[i]);
	        if (child != NULL)
	            fields[i] = child->valuestring;
	        else
	            printf("Can not find cjson object %s\n", field_names[i]);
	    
	        i++;
	        optional_field_count--;
	    }
	    
	    // TODO: Assign default values if exists.
	    
	    APPLIST[index].id = id;
        APPLIST[index].path = fields[1];
		APPLIST[index].name = fields[2];
		APPLIST[index].group = fields[3];
        APPLIST[index].prettyname = fields[4];
        APPLIST[index].iconpath = fields[5];
        APPLIST[index].color = fields[6];
        
        number_of_applications++;
        
#ifdef DEBUG
        printf("Application(%d): %s, on path %s\n", APPLIST[index].id,
                APPLIST[index].prettyname, APPLIST[index].path);
        fflush(stdout);
#endif /* DEBUG */

		/* Fix pointer problem and free cjson object. */
		/* cJSON_Delete(root); */
	}
}

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

    if (!d) {
        fprintf(stderr, "Cannot open directory '%s': %s\n",
                MANIFEST_DIR, strerror (errno));
        exit (EXIT_FAILURE);
    }

    while ((entry = readdir(d)) != NULL && app_index < MAX_NUMBER_APPLICATIONS) {

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

int run_app (int appid) {
    int rc;
    int i;
    int appindex;

    /* Search for the appid in APPLIST. */
    for (i = 0; i < MAX_NUMBER_APPLICATIONS; i++) {
        if (APPLIST[i].id == appid) {
            appindex = i;
            break;
        }
    }

#ifdef DEBUG
    printf("Running application id: %d\n", appid);
    fflush(stdout);
#endif /* DEBUG */

    pid_t pid = fork();

    if (pid == 0) {
        /* TODO: Set application group settings. */

#ifdef DEBUG
        printf("Child process is going to execute %s\n",
                APPLIST[appindex].name);
        fflush(stdout);
#endif /* DEBUG */

        rc = execl(APPLIST[appindex].path,
                APPLIST[appindex].name, (char*)NULL);
        if (rc == -1) {
            handle_error("execl");
        }
    } else if (pid < 0) {
        return -1;
    }

    return 0;
}

void reply_runapp (DBusMessage* msg, DBusConnection* conn) {
    DBusMessage* reply;
    DBusMessageIter args;
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

#ifdef DEBUG
    printf("Method called with %s\n", param);
    fflush(stdout);
#endif /* DEBUG */

    /* Run the corresponding application */
    rc = run_app(atoi(param));
    if (rc == 0) {
        /* Signal the condition variable that application has executed. */
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&child_exists);
        pthread_mutex_unlock(&mutex);
    }

#ifdef DEBUG
    printf("Creating reply message with %d\n", rc);
    fflush(stdout);
#endif /* DEBUG */

    /* Create a reply from the message. */
    reply = dbus_message_new_method_return(msg);

    /* Add the arguments to the reply. */
    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &rc)) {
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

void reply_listapps (DBusMessage* msg, DBusConnection* conn) {
    DBusMessage* reply;
    DBusMessageIter args, struct_i, array_i;
    dbus_uint32_t serial = 0;
    unsigned int i = 0;

    /* Create a reply from the message. */
    reply = dbus_message_new_method_return(msg);

    /* Add the arguments to the reply. */
    dbus_message_iter_init_append(reply, &args);
    
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, 
                                        &number_of_applications)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }
    
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(usss)", &array_i);
    for (i = 0; i < number_of_applications; ++i) {
        dbus_message_iter_open_container(&array_i, DBUS_TYPE_STRUCT, NULL, &struct_i);
        
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_UINT32, 
                                       &(APPLIST[i].id));
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_STRING, 
                                       &(APPLIST[i].prettyname));
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_STRING, 
                                       &(APPLIST[i].iconpath));
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_STRING, 
                                       &(APPLIST[i].color));
        
        dbus_message_iter_close_container(&array_i, &struct_i);
        
        printf("#(%d): %s, %s, %s supplied\n", APPLIST[i].id, APPLIST[i].prettyname,
                                            APPLIST[i].iconpath, APPLIST[i].color);
    }
    dbus_message_iter_close_container(&args, &array_i);

    /* Send the reply && flush the connection. */
    if (!dbus_connection_send(conn, reply, &serial)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }

    dbus_connection_flush(conn);

    /* Free the reply. */
    dbus_message_unref(reply);
}

void listen () {
    DBusMessage* msg;
    DBusConnection* conn;
    DBusError err;
    int ret;

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

    while (1) {
        /* Non-blocking read of the next available message. */
        dbus_connection_read_write(conn, 0);
        msg = dbus_connection_pop_message(conn);

        if (msg == NULL) {
            sleep(1);
            continue;
        }
        
        printf("** Coming message info **\n");
        printf("Sender: %s\n", dbus_message_get_sender(msg));
        printf("Type: %d\n", dbus_message_get_type(msg));
        printf("Path: %s\n", dbus_message_get_path(msg));
        printf("Interface: %s\n", dbus_message_get_interface(msg));
        printf("Member: %s\n", dbus_message_get_member(msg));
        printf("Destination: %s\n", dbus_message_get_destination(msg));
        printf("Signature: %s\n", dbus_message_get_signature(msg));
        
        /* Check this is a method call for the right interface & method. */
        if (dbus_message_is_method_call(msg, "appman.method.Type", "runapp")) {
            reply_runapp(msg, conn);
        } else if (dbus_message_is_method_call(msg, "appman.method.Type", "listapps")) {
            reply_listapps(msg, conn);
        } else {
            printf("Coming message is not a method call.\n");
        }

        /* Free the message. */
        dbus_message_unref(msg);
    }

    /* Close the connection. */
    //dbus_connection_close(conn);

    dbus_connection_unref(conn);
}

void *request_handler(void *targs) {
    sigset_t set;
    int s;
    // thread_info *tinfo = targs;

    /* Mask signal handling for threads other than main thread. */
    sigemptyset(&set);
    sigaddset(&set,SIGCHLD);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s != 0)
        handle_error_en(s, "pthread_sigmask");

    /* Listen and answer D-Bus method requests to run applications. */
    listen();

    pthread_exit(NULL);
}

void status_handler(int pid, int status) {
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
        printf("anything then handled reasons.\n");
    }
}

void signal_handler(int signo, siginfo_t *info, void *p) {
    int status;
    int rc;

#ifdef DEBUG
    printf("signal %d received:\n"
            "si_errno %d\n"    /* An errno value */
            "si_code %s\n"     /* Signal code */
            ,signo,
            info->si_errno,
            reasonstr(signo, info->si_code));
            fflush(stdout);
#endif /* DEBUG */

    if (signo == SIGCHLD) {
#ifdef DEBUG
        printf( "si_pid %d\n"      /* Sending process ID */
                "si_uid %d\n",     /* Real user ID of sending process */
                "si_status %d\n"   /* Exit value or signal */
                "si_utime %d\n"    /* User time consumed */
                "si_stime %d\n",   /* System time consumed */
                info->si_pid,
                info->si_uid,
                info->si_status,
                info->si_utime,
                info->si_stime);
        fflush(stdout);
#endif /* DEBUG */

        /*
         * Multiple child processes could terminate
         * while one is in the process being reaped.
         * Loop ensures that any zombies which existed
         * prior to invocation of the handler function
         * will be reaped.
         */
        while ((rc = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
            status_handler(rc, status);
        }
    }
}

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

    /* Fill application list, once per lifetime. */
    rc = get_applist();
    if (rc) {
        handle_error("get_applist");
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
                            request_handler, (void*)&tinfo[i]);
        if (rc) {
            handle_error_en(rc, "pthread_create");
        }
    }
    /* Time for thread initializations. */
    sleep(1);

    /*
     * Infinite loop of process waits. If there are processes
     * to wait, wait any of them. If not, wait on the given condition
     * variable. When signaled, redo.
     */
    while (1) {
        rc = waitpid((pid_t)(-1), &status, 0);

#ifdef DEBUG
        printf("Returned waitpid from main loop:%d\n", rc);
        fflush(stdout);
#endif /* DEBUG */

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
            status_handler(rc, status);
        }
    }

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
