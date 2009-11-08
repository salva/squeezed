This directory contains the files for the browser-user-interface.

There are three mechanisms for communication between browser and SqueezeD:
A Cookie to select a device, keyword parsing and a directory with special files.
Both method 2 and 3 use the cookie to determine which squeezebox-device to operate upon.



1) Cookie.
The browser can set the cookie "device=" to determine which squeezebox device is 
being controlled. A list of devices can be gotten through method 2.


2) Keyword parsing.

All files within the /html/ subdirectory are parsed by the server before
being sent by the browser. specific keywords are replaced by the
current status.

A list of general keywords:

#DEVICE.LIST#		- list of device names currently connected to the server
#DEVICE.ELAPSED#	- seconds since start of song

#PLAYLIST.LIST#		- current playlist, parsed using "playlistItem.html"
#PLAYLIST.TITLE#	- todo (use #TITLE# for now)
#PLAYLIST.ARTIST#	- todo

#TITLE#			- current song title
#ALBUM#			- current song album
#ARTIST#		- current song artist
#LENGTH#		- current song length, in seconds
#REPEAT#		- 0: no repeat, 1:repeat playlist


For directories, the dirlist.html and dirlistItem.html are used:

#BASE#			- current path
#FILELIST#		- generate a list of files using dirlistItem.html

#RELURL#		- relative http url of current file
#NAME#			- filename
#MIMETYPE#		- mime type (Based on extension)


3) Sending commands through /dynamic/

There is a special directory on the server which allows commands to be sent,
The cookie "device" is used to determine which device is affected by these commands.
the commands are sent by requesting one of the following files:

/dynamic/control&action=ACTION&value=VALUE
	ACTION can be one of:
		play,pause,next,prev,stop,volume
	volume accepts values from 0 to 100.

/dynamic/play&idx=IDX
	start playing song IDX in the playlist
/dynamic/play&url=URL
	replace playlist by URL (path or filename, relative to /data/)
/dynamic/add&url=URL
	add items to the playlist (path or filename, relative to /data/)

/dynamic/notify&url=URL
	returns the file URL (relative to /html directory),
	but only returns after the status of the player has changed.
	This is the key element 	

		


4) 