#!/bin/bash
if [ -z "$1" -o -z "$2" ]; then
  echo "Usage:"
  echo "makeManifest.sh <directory> <cardId>"
  exit 1
fi
find "$1" -mindepth 1 -maxdepth 1 -type f > "$2.lst"
while read i ; do stat -c '%s %n' "$i" ; done < "$2.lst" > "$2.lst.manifest"
