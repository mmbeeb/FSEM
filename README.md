# FSEM


* an emulator specifically for the Acorn Level 3 File Server (Version 1.25)
* for use with BeebEm (Version 4.16)
* uses the AUN protocol
* runs in a Linux console window

As well as the compiled program, the following are required in the same folder:

* a copy of the Level 3 Version 1.25 file server program "FS3v125"*
* a suitably formatted SCSI disk image named "scsi1.dat"*

 _* see below_

# Econet Station Numbers/IP Ports

FSEM uses a simple method to determine the station number/IP port:

IP Port = Station Number + Base

# Modifying BeebEm's Econet.cfg file

Using the FSEM defaults of Base = 10000, and File Server Station Number = 254, an example configuration would be:

#Fileserver (station number 254) and 4 stations on the local PC:

*  0 254 127.0.0.1 10254 
 
#Stations:

*  0 101 127.0.0.1 10101 
*  0 102 127.0.0.1 10102 
*  0 103 127.0.0.1 10103 
*  0 104 127.0.0.1 10104 

# Downloading the Level 3 Version 1.25 File Server.

Version 1.25 is a patched version by J.G.Harston.

It has the Y2K bug fix, and also reads the Master's RTC rather than the dongle.

It can be downloaded from JGH's site:

* Google "mdfs level 3"
* open "File Servers - MDFS::Apps.Networking.FServers - mdfs.net"
* download "Level3FS.zip"
* extract "FS3v125"

# Creating a File Server disk image using BeebEm (Version 4.16)

Select SCSI Hard Drive in the Hardware, and the BBC Model : Master 128 in Options.

Load the disk "L3FS_ISW.adl" and select ADFS, e.g. Press A and BREAK.

Chain "WFSINIT" and enter the following when prompted:

* Drive number: 1
* Disc name: Choose a name
* Next Drive: Leave blank
* Date (dd/mm/yy): Enter date

Now create a password file:
  
* Password file (Y/N): Y
* User name 1 : Choose a name (Note create as many users as you want.)
* User name X : Leave blank
  
When prompted to copy the master directories press N.

When the disc initialised, close BeebEm and copy the file \BeebEm\DiscIms\scsi1.dat.
