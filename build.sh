#!/bin/sh

cc -Wall -Wextra -g -fsanitize=address,undefined -o main main.c
