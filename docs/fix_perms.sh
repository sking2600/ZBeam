#!/bin/bash
while true; do
    if [ -e /dev/ttyACM0 ]; then
        sudo chmod 666 /dev/ttyACM0
        echo "Fixed permissions on /dev/ttyACM0"
    else
        echo "Waiting for /dev/ttyACM0..."
    fi
    sleep 10
done
