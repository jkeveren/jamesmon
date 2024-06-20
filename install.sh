sudo cp bin/jamesmon /usr/local/bin/
# setcap after copy in case cloned location is not capable of setcap like sshfs or nfs.
sudo setcap "cap_perfmon=pe" /usr/local/bin/jamesmon
