#!/bin/bash
set -e

command="$@"

case $1 in
	test)
            mkdir .ci
            cd .ci
            meson ..
            ninja test
            rm -R ../.ci
		;;
esac