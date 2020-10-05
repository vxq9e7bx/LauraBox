#!/usr/bin/python3

# Change to playlist ID to download. Can be extracted from the URL: https://music.youtube.com/playlist?list=<PlaylistID>
PlaylistID="PLL-zy2w69Cx-u2nlxPxZTRlM2YNN8xktW"

from ytmusicapi import YTMusic
import os

api = YTMusic("headers_auth.json")
#l = api.search("Eddi & Dän singen Kinderlieder a cappella")

l = api.get_playlist(PlaylistID)

counter = 0
for track in l["tracks"] :
  videoId = track["videoId"]
  
  title = track["title"]
  safetitle="".join([c for c in title if c.isalnum() or c==' ']).rstrip()
  counter = counter+1
  filename = "%03d" % counter + " - " + safetitle + ".mp3"
  
  print(filename)
  if os.path.exists(filename):
    print("already exists...")
    continue
  
  os.system("rm -f tempdownload.*")
  os.system("youtube-dl https://music.youtube.com/watch?v="+videoId+" -o tempdownload")
  os.system("ffmpeg -i tempdownload.* -vn -b:a 192K '"+filename+"'")

