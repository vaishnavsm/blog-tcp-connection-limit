# Making TCP Connections With Socket Reuse

**Note:** This repo is meant to be used and read alongside [this article](https://vaishnavsm.com).

## Components

### Listener

The listener is a simple c TCP server that just echos any TCP data it is sent from any client.
It uses non-blocking sockets (setting the `SO_RCVTIMEO`, `SO_SNDTIMEO`, and `O_NONBLOCK` options on the socket) and uses `poll` to poll for events on the listen socket and connection file descriptors.

The listener is also started with the `SO_REUSEADDR` and `SO_REUSEPORT` options.
`SO_REUSEPORT` is not really relevant to the listener in the article, as we do not try and spin up multiple servers listening on the same port. However, it is possible to run multiple instances of this listener on the same port given they use different listen addresses. This includes one listener on `0.0.0.0` and another on `127.0.0.1`, for example.

### Connector

The connector is a simple c TCP client that connects to a TCP server and sends a "heartbeat" message every 5 seconds in a blocking manner.

Unlike normal TCP clients, the connector specifically `bind`s to a given port before making the TCP connection using `connect`. This is just so we can control the port the connection is made from, and does not impact the socket behaviour.

The connector is started with the `SO_REUSEADDR` and `SO_REUSEPORT` options. This is what allows multiple instances of the connector to share `(source ip, source port)`.

## Building

This project has only been tested on POSIX systems, including Mac (Intel and ARM) and Linux.
I don't think this works on windows, as several TCP options have a different shape and use `DWORD` based values, which have not been set up.

Install the GNU C compiler (`gcc`) as well as GNU Make.
Then run `make`
```bash
make
# or
make all
```

## Running

### Listener
```bash
# ./out/listener <bind port = 3000> <bind ip = 0.0.0.0>

# bind on 0.0.0.0:3000
./out/listener

# bind on 0.0.0.0:4444
./out/listener 4444

# bind on 127.0.0.1:4545
./out/listener 4545 127.0.0.1
```

### Connector
```bash
# print help
./out/connector

# ./out/connector <bind ip> <bind port> <server ip> <server port> <data to send = "hi">

# connect from 127.0.0.1:3001 to 127.0.0.1:3000 sending "hello" as heartbeat
./out/connector 127.0.0.1 3001 127.0.0.1 3000 "hello"
```

## Experiments you may want to try
1. Create two instances of the server that use different bind ips but the same port, and connect to it from two instances of the connector that share the same bind ip and bind port.
1. Create one server instance listening on all IPs, and connect to it from two instances of the connector that share the same bind IP and bind port, but uses two different server (destination) IPs. These can be the loopback and LAN IPs.
1. Create two instances of the server that use different bind ports but the same ip, and connect to it from two instances of the connector that share the same bind ip and bind port.

**A helpful note on IPs**

You don't need access to multiple machines to test multiple IPs. You can find multiple ips on your local machine on different interfaces. You can use `ifconfig` to find these. The easiest pair would be to use the loopback (localhost) IP `127.0.0.1` as well as the IP your computer uses to connect to the LAN/internet. This is usually in the `192.168.0.0` subnet. Running `ifconfig | grep 'inet'` could help you identify these interfaces.
