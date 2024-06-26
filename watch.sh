ls src/* | entr -crs "./build.sh && ./bin/jamesmon -f 60"

# If this doesn't work, use "sudo setcap "cap_perfmon=pe" bin/jamesmon" to give jamesmon the perfmon capability https://man7.org/linux/man-pages/man7/capabilities.7.html
# If the filesystem that you cloned this repo to is not capable of setcap, then you can temporarily change your kernel settings to not restrict access to perf information using the following:
# "echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid"
# This is described well in sections: "Arguments", and "perf_event related configuration files" of https://man7.org/linux/man-pages/man2/perf_event_open.2.html.
