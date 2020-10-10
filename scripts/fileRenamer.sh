#!/bin/bash

echo "Will rename all files in the current directory:"
pwd
echo "so that all non-ASCII characters (like Umlauts) are changed into underscores."
echo "Continue? (Ctrl+C to abort)"
read || exit 1
for i in * ; do
  mv "$i" $(echo "$i" | sed -e 's/[^A-Za-z0-9._- ]/_/g' -e 's/[äöüÄÖÜß]/_/g')
done
echo "Done."
