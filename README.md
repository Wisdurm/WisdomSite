# Setup
First make sure Git and Ninja are installed.  
```sudo apt install git ninja-build```  
Then install and setup vcpkg.  
Start by cloning the repository:  
```git clone https://github.com/microsoft/vcpkg.git```  
Then run the bootstrap script:  
```cd vcpkg; .\bootstrap-vcpkg.bat``` On Windows  
```cd vcpkg && ./bootstrap-vcpkg.sh``` On Linux  
Now all dependencies are setup.  
For the next steps, cd into this repository.   
Create ```CMakeUserPresets.json``` in the project folder,  
and paste the following:  
```
{
  "version": 2,
  "configurePresets": [
    {
      "name": "default",
      "inherits": "vcpkg",
      "environment": {
        "VCPKG_ROOT": "<path to vcpkg>"
      }
    }
  ]
}
```
Then run:  
```vcpkg install```  
Then finally, compile the program with:  
```cmake --preset=default```  
```cmake --build build```  

**TODO:**
Other stuff, and verify instructions are correct lol
