# About

This is my own personal website, currently hosted at [Wisdurm.fi](https://wisdurm.fi)    
This site is built using [CrowCPP](https://crowcpp.org/master/)  
**This site is built for my personal use. It is not very well made, but it does just barely work.
CrowCPP was used just because "C++ web framework funny"** 

# Setup

First make sure dependencies are installed.  

Linux:  
```sudo apt install git cmake build-essential libsqlite3-dev libasio-dev```   
OpenBSD:  
```doas pkg_add git cmake sqlite3 asio```

For the next steps, clone and cd into this repository.   

Then, compile the program with:  
```cmake --S . -B build```  
```cmake --build build```  

Create the motd.txt file

**TODO:**
Other stuff, and verify instructions are correct lol
(Linux packages *may* be wrong)

# Licenses

The license for this site is [BSD-2-Clause](LICENSE)  
This license only covers this repository, and not any other repositories included as submodules (eg. infinite canvas).  
You can find the licenses for specific sub modules in their repository.
