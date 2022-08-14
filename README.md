# Library Util

## Summary

The Util Library is used as a base for applications and libraries. 

The C++ library consists of the following components.

- **Crypto** Functions. Mainly MD5 hash checksum function.
- **CSV** Writer. Simplifies the usage of creating comma separated data files.
- **Gnuplot**. Implements a wrapper around the third-party gnuplot application.
- **Listen** Functions. User interface against the Listen functionality. 
- **Logging** Functions. Basic log function to file or console.
- **XML** Functions. Simple wrapper around the third-party EXPAT library.
- **String** Functions. Various string manipulations that is missing in the standard C++.
- **Time and Date** Functions. Various time and date functions

The following companion applications exist.

- **Service Daemon**. Windows service that start and supervise other executables (daemons).
- **Service Explorer**. Support GUI for configure Windows service daemons.
- **Listen Daemon**. Simple daemon executable that supervise listen servers.
- **Listen Viewer**. GUI application that show listen messages.

## Installation

Installation kit for [UtilLib v1.0.0](https://github.com/ihedvall/utillib/releases/download/v1.0.0/utillib.exe).

## Documentation 
The documentation can be found on GitHub Pages: [UtilLib](https://ihedvall.github.io/utillib)

## Building the project

The project uses CMAKE for building. The following third-party libraries are used and
needs to be downloaded and built.

- Boost Library. Set the 'Boost_ROOT' variable to the Boost root path.
- Expat Library. Set the 'EXPAT_ROOT' variable to the expat root path.
- OpenSSL Library. Set the 'OPENSSL_ROOT' variable to the OpenSSL root path.
- ZLIB Library. Set the 'ZLIB_ROOT' variable to the ZLIB root path.
- WxWidgets Library. Is required if the GUI applications should be built.
- Doxygen's application. Is required if the documentation should be built.
- Google Test Library. Is required for running and build the unit tests.

### License

The project uses the MIT license. See external LICENSE file in project root.

