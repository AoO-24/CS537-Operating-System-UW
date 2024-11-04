#!/bin/bash

# Kill all python server instances
kill -KILL $(ps aux | grep -i 'python3 -m http.server' | awk '{print $2}') 2> /dev/null

# Kill all proxyserver instances
killall proxyserver 2> /dev/null

exit 0
