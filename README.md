# utcs
A service that returns the current UTC time.

**utcs** listens for connection requests from the `PORT` port. It can only handle at most one request at a time. When one connection is successfully established, it will return a hello message to the client. Then every random time, **utcs** will send the current UTC time to the client. The value range of the send interval is `[1, SLEEP_SEC_MAX]` seconds.