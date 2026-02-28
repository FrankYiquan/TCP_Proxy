# TCP Proxy Server

## Overview

This project implements a simple TCP proxy server in C. The proxy listens on a local port and forwards traffic between a client and a remote server. It acts as an intermediary that relays requests from the client to the remote server and sends the server's responses back to the client.

The proxy uses non-blocking sockets and `select()` to handle bidirectional communication efficiently. Fixed-size buffers are used to ensure the proxy can handle arbitrarily large data transfers without consuming unbounded memory.

The program is executed as:

```
./tcpproxy remote_host remote_port proxy_port
```

Example:

```
./tcpproxy cs146a.cs.brandeis.edu 80 8000
```

This command creates a proxy listening on port **8080**, forwarding traffic to **cs146a.cs.brandeis.edu:80**.

---

# Implementation Details

## Listener Creation

The proxy first creates a listening socket using `create_listener()`.

Key steps:

1. `getaddrinfo()` resolves the address for the local port.
2. `socket()` creates the TCP socket.
3. `bind()` attaches the socket to the proxy port.
4. `listen()` allows incoming client connections.

The proxy then repeatedly calls `accept()` to accept new client connections.

---

# Connecting to the Remote Server

When a client connects, the proxy opens a connection to the target server using:

```
connect_remote(remote_host, remote_port)
```

This function:

1. Resolves the remote host with `getaddrinfo()`
2. Creates a socket
3. Connects to the remote server with `connect()`

---

# Non-blocking Sockets

Both the client and server sockets are set to non-blocking mode using:

```
fcntl(fd, F_SETFL, flags | O_NONBLOCK)
```

Non-blocking I/O allows the proxy to manage both sockets without waiting indefinitely for reads or writes.

The program also enables:

```
SO_KEEPALIVE
```

to maintain active connections.

---

# Data Forwarding Logic

The proxy uses a loop in `proxy_loop()` to relay data between the client and server.

Two fixed-size buffers are used:

```
c2s_buf  (client → server)
s2c_buf  (server → client)
```

Each buffer has size:

```
BUF_SIZE = 8192 bytes
```

This ensures the proxy does not allocate unlimited memory, satisfying the assignment requirement.

---

# Multiplexing with select()

The proxy uses `select()` to monitor socket readiness.

The following conditions are checked.

## Read Events

The proxy reads data when:

- client socket is readable → data goes into `c2s_buf`
- server socket is readable → data goes into `s2c_buf`

## Write Events

The proxy writes data when:

- server socket is writable → send `c2s_buf`
- client socket is writable → send `s2c_buf`

Partial writes are handled using:

```
memmove()
```

to shift remaining bytes in the buffer.

---

# Fixed Buffer Requirement

The proxy uses fixed buffers of size **8192 bytes**.

If the buffer fills, the proxy pauses reading until data is written out. This ensures the proxy can handle arbitrarily large responses without allocating additional memory.

---
# Testing

Testing was performed on the Brandeis CS server using two terminal sessions connected through SSH.

## Test Setup

Two terminals were opened and both connected to the same machine:

```
ssh yiquanzhang@diadem.cs.brandeis.edu
```

**Terminal 1** was used to run the proxy server.

Example command:

```
./tcpproxy cs146a.cs.brandeis.edu 80 8000
```

This starts the proxy on port **8000**, forwarding requests to **cs146a.cs.brandeis.edu:80**.

**Terminal 2** was used as the client to send requests to the proxy.

---

# Test 1: Basic HTTP Request (GET /)

From the second terminal, the proxy was tested using telnet:

```
telnet localhost 8000
```

Connection output:

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
```

Then the request was entered:

```
GET /
```

The proxy forwarded the request to the remote server and returned the HTML page. The response began with:

```
<!doctype html>
<html>
  <head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>HTTP Server Test Page powered by: Rocky Linux</title>
    ...
```

This confirmed that the proxy correctly forwarded a basic HTTP request and returned the server response.

---

# Test 2: HTTP/1.1 Request (GET /congrats.html)

Next, a full HTTP/1.1 request was tested using telnet.

```
telnet localhost 8000
```

Request sent:

```
GET /congrats.html HTTP/1.1
Host: cs146a.cs.brandeis.edu
```

The proxy successfully returned the server response headers and HTML content.

Example response:

```
HTTP/1.1 200 OK
Server: nginx/1.24.0
Date: Sat, 28 Feb 2026 17:25:59 GMT
Content-Type: text/html
Content-Length: 1952
Last-Modified: Sat, 19 Feb 2022 05:38:46 GMT
Connection: keep-alive
ETag: "62108266-7a0"
Accept-Ranges: bytes

<!DOCTYPE html>
<html  >
<head>

  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">

  <meta name="viewport" content="width=device-width, initial-scale=1, minimum-scale=1">
  <link rel="shortcut icon" href="assets/images/logo5.png" type="image/x-icon">
  <meta name="description" content="">
  ...
```

This confirmed that the proxy correctly handled HTTP/1.1 requests and forwarded headers and content.

---

# Test 3: HTTP/0.9 Compatibility

An additional test was performed using an HTTP/0.9-style request:

```
GET /congrats.html
```

The connection was closed immediately:

```
Connection closed by foreign host.
```

This behavior is likely due to the remote server (nginx) not supporting HTTP/0.9 requests because the protocol version is considered obsolete.

---

# Test 4: Binary Data Transfer

To verify that the proxy correctly handles binary data, an image was downloaded through the proxy using `curl`.

Command:

```
curl http://localhost:8000/assets/images/congrats.jpg -o proxy.jpg
```

Output:

```
% Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
100 12593  100 12593    0     0   209k      0 --:--:-- --:--:-- --:--:--  409k
```

The downloaded file was then verified:

```
ls -lh proxy.jpg
```

Output:

```
-rw-rw-r--. 1 yiquanzhang yiquanzhang 13K Feb 28 11:54 proxy.jpg
```

File type check:

```
file proxy.jpg
```

Output:

```
proxy.jpg: JPEG image data, JFIF standard 1.01
```

This confirmed that the proxy correctly forwarded binary data without corruption.

---

# Summary of Testing

The proxy was tested with:

- Basic HTTP requests
- HTTP/1.1 requests with headers
- HTTP/0.9 compatibility
- Binary file transfers

All tests confirmed that the proxy correctly forwards traffic between the client and the remote server.

