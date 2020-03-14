# PoliteRAT
Remote Access Tool designed to interact with Reddit's API.

This has been an on and off project and since I've been investing more time into it after initial completion, I figured a GitHub page would be appropriate.

DISCLAMER:
  I truly have no idea what I am doing. This is a spaghetti minfield full of bad programming and logic bugs. Plus it only works for Linux.

# Roadmap
- Create a version for windows.
- Rewrite the file uploader in Python
- Increase the max message size to 10,000 bytes (At the moment, Regex seems to crash when processing large files)
- Create a handler in python, and incorperate the uploader into it.
- Create a "jitter" with the beacons, and allow the sleep timing to be adjusted
- Separate Reddit's API into a class/object/function or whatever to allow for modularity.
