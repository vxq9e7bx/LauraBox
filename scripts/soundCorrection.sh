#!/bin/bash

echo "Will apply high-pass filter to all files in the current directory:"
pwd
echo "Continue? (Ctrl+C to abort)"
read || exit 1
mkdir -p original
for i in * ; do
  if [ ! -f "$i" ]; then
    continue
  fi
  echo $i
  if [ -f "original/$i" ]; then
    echo "Already converted, skipping."
  else
    mv "$i" "original/$i"
    sox "original/$i" "$i" vol 0.1 treble 20 1000
  fi
done
echo "Done."

