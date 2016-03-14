#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUMBER_APPLICATIONS 50
#define DEBUG_PREFIX "tester: "

typedef struct application {
    unsigned int id;
    char* prettyname;
    char* icon;
} application;

void assert_dbus_method_return (DBusMessage* msg);

void query_startapp(int app_id);
void query_listapps();
void query_login(int access_code);
void query_updateapps();
void query_lockscreen();
void query_removeapps(int* apps, int size);

unsigned int number_of_applications = 0;
application APPLIST[MAX_NUMBER_APPLICATIONS];
