Application launcher daemon
---------------------------

Application launcher validates, groups, lists and executes applications.
If request arrived, appmand invokes method that triggered by D-bus.
Application permissions are validated by manifest files that stored in 
manifest directory.

* Application list service by daemon for UI applications
```
listapps
```
* Application launch request
```
runapp <appid>
```


*.mf manifest files (in json structure) has name <appid>.mf that 
is a unique integer value, represent application.

Manifest files have to contain application path, group (for cgroup), permissions values. Moreover, for UI applications also
and application group respectively.

Manifest example:
```
{
  "id": 0,
  "path": "/usr/bin/gnome-calculator",
  "cgroup": "group01",
  "perms": [
    "camera",
    "bluetooth"
  ],
  "prettyname": "Calculator",
  "iconpath": "cal-iconpath",
  "color": "#00000"
}
```
* Note
libdbus-1-dev is required for dbus functionality.
