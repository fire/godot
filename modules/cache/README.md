# godot-cacheserv
Cache server module for godot engine

This module was made as part of the Google Summer of Code 2019 program. The purpose of this module is to allow Godot engine to shift from using OS based caching for any IO to its own caching, relying only on fast unbuffered IO on the OS side. This allows for ease of use within costrained environments such as game consoles and other limited hardware.

[Here][1] is a list of all the commits I've made during the project time period.

I've also made a [technical document][2] that attempts to explain how everything works.

# Current state

The module is currently in a working state but the design has many shortcomings that should be addressed. There are many chances for optimisation and the overall design may have to undergo more changed as well.

# Installation

This is a module for the Godot engine, and is intended to be used as a drop-in module at compile-time.

* First, retrieve the Godot engine source code from the Godot engine main [repo](https://github.com/godotengine/godot).
* Navigate to the modules folder: ```$ cd godot/modules```
* Clone the module's repo: ``` $ git clone https://github.com/WarpspeedSCP/godot-cacheserv cacheserv```

The module should now be present in the cacheserv subdirectory and will automatically be included when the engine is built through scons.

# Usage

This module exposes one type intended for general use: the `FileAccessCached` class. This class provides a FileAccess style 
frontend to the file cache server which does all the heavy file IO. `FileAccessCached` is available through both GDScript and C++. 

In addition, two unbuffered versions of the FileAccess class are provided, one for unix, and the other for windows. Of these, the unbuffered unix implementation is complete while the unbuffered windows version is not.

[1]: https://github.com/WarpspeedSCP/godot/commits?author=WarpspeedSCP
[2]: https://docs.google.com/document/d/1u5pnouYPkF44VpupJ3J_TUTM_RS5JVG2fOLJKAT9QU4
