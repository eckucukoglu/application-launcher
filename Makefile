appmand:
	$(CC) -o appmand cJSON.c appmand.c -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -ldbus-1 -lm -lpthread -DDEBUG
clean:
	rm appmand
