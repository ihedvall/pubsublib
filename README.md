# Library Publish/Subscribe

## Summary

The Library contains interfaces against publish/subscribe protocols as MQTT,AQMP 
and Apache Kafka. 

The library currently only support MQTT clients. The library is under development. 

## Documentation 

Under development.

## Building the project

The project uses CMAKE for building. The following third-party libraries are used and
needs to be downloaded and built.

- Util Library. Set the 'Boost_ROOT' variable to the Boost root path.
- Eclipse Paho MQTT C Library. 
- OpenSSL Library. Set the 'OPENSSL_ROOT' variable to the OpenSSL root path.
- WxWidgets Library. Is required if the GUI applications should be built.
- Doxygen's application. Is required if the documentation should be built.
- Google Test Library. Is required for running and build the unit tests.

### License

The project uses the MIT license. See external LICENSE file in project root.

