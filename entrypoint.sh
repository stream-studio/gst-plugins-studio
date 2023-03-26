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
Symbols
Symbol outline not available for this file
To inspect a symbol, try clicking on the symbol directly in the code view.
Code navigation supports a limited number of languages. See which languages are supported.