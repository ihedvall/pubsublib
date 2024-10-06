# Library Publish/Subscribe (C++)

## Summary

The Library contains interfaces against publish/subscribe protocols as MQTT 
and Sparkplug B. 

The library hides the MQTT and the Sparkplug B protocols against the end-user code. 
The following public/subscribe clients are currently supported.
- **Plain MQTT clients**. MQTT 3.1.1 and MQTT 5.0 protocol with optional TLS encryption.
- **Sparkplug B Host**. The host is also called the SCADA host.
- **Sparkplug B Node**. The node implements a communication node.
- **Sparkplug B Device**. Implements a device within a communication node.

## Metrics
A metric is an object that holds an end-user value. The clients either publish or subscribe 
on a metric objects. Each metric is internally attached to a communication topic. The plain 
MQTT client normally on hold one metric while a Sparkplug client can have several metrics.

## Library Version
The library is in so-called beta mode. The end-user can start to use it but the library 
is currently not well tested against existing MQTT brokers or Sparkplug B systems. 

## Remaining Work
- **GitHub Actions and VCPKG support**.
- **Testing against existing system**.
- **User HTML Documentation**.
- **Sparkplug Simulator(s)**. This is mainly for testing but may be useful for an end-user. 
- **Sparkplug B Explorer**. Simple GUI showing the clients and metrics.
- **Redis Client**. Pub/sub interface to a Redis "database".  
- **Kafka Client**. Additional pub/sub protocols.

## Building the Library and Applications
The project uses CMAKE for building. The following third-party libraries are used and
needs to be downloaded and built. Note that the project is currently using pre-built 
libraries in cmake variable (COMP_DIR=path to pre-built).

- Util Library. Set the 'Boost_ROOT' variable to the Boost root path.
- Eclipse Paho MQTT C Library. 
- Google Protobuf. 
- OpenSSL Library. Set the 'OPENSSL_ROOT' variable to the OpenSSL root path.
- WxWidgets Library. Is required if the GUI applications should be built.
- Doxygen's application. Is required if the documentation should be built.
- Google Test Library. Is required for running and build the unit tests.

## License

The project uses the MIT license. See external LICENSE file in project root.

