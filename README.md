# Sesame server

This is an implementation of an HTTP server that listens for requests to
temporarily toggle specified pins. Uses two external libraries, specified in
the code.

## External dependencies

- [Ethercard library](https://github.com/jcw/ethercard) for communicating
through Ethernet
- [A tailored version of Cryptosuite](https://github.com/manutenfruits/Arduino-SHA-256)
used for authentication

## Authentication

This authentication procedure is greatly based on the work by Robert Lord in
his post [Hashes, Nonces, and Replay Attacks on the Arduino](https://lord.io/blog/2014/arduino-encryption/)
as well as [Unlocking Hacker School with a Text](https://lord.io/blog/2014/unlocking-hacker-school/)
but re-purposed to use the [Ethercard library](https://github.com/jcw/ethercard)
and multiple doors.

In order to open the door, a POST request needs to be made to the following URL

`http://<SERVER_IP>/<NONCE>/<DOOR_NR>/<HASH>`

- `SERVER_IP`: the hostname or IP of our Arduino server
- `NONCE`: a positive integer that is higher than the last used nonce
- `DOOR_NR`: the door number we want to open (0 for the first one)
- `HASH`: the computed HMAC-SHA-256 of the nonce using an established password as key

Possible responses are:

- **200 OK**: after correctly opening the requested door.
- **400 Bad Request**: if the used nonce is smaller than the last one used correctly.
Returns in the body the next available nonce.
- **401 Unauthorized**: if the nonce is correct, but the hash is not (bad password).
- **404 Not Found**: if the number of door given is higher than the maximum doors registered.
- **418 I'm a teapot**: if the request sent is larger than the buffer (600 bytes).
- **420 Enhance Your Calm**: if a request is sent while the door is being signaled.

## TODO

- Develop a better authentication mechanism, since attacks are still possible if
somebody sniffs the traffic. If the device is restarted then the nonce counter is
reset to 0 and a past request can be used.
- Investigate what happens if the Arduino stays awake for too long.
