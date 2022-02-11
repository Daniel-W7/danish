#!/bin/bash

ps -ef | grep danish | grep -v grep | awk '{print $2}' | sed -e "s/^/kill -9 /g" | sh -
