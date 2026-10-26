#!/bin/zsh
# Doppelklick im Finder startet die Steuer-App.
cd "$(dirname "$0")"
unset ELECTRON_RUN_AS_NODE   # sonst startet Electron als reines Node.js
exec /opt/homebrew/bin/electron .
