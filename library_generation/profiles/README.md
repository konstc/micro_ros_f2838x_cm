# Library profiles

A profile is a colcon `.meta` file: the entity limits and buffer sizes it sets are
compiled into `libmicroros.a`, so changing them means rebuilding the library.

| Profile | Nodes | Pub / Sub | Services / clients | History | Stream history | MTU, bytes |
| --- | --- | --- | --- | --- | --- | --- |
| small | 1 | 2 / 2 | 1 / 1 | 2 | 2 | 256 |
| default | 1 | 7 / 4 | 7 / 1 | 4 | 4 | 512 |
| large | 2 | 13 / 8 | 14 / 2 | 4 | 4 | 512 |

- The rclc parameter server uses 6 services (get, get types, set, set
  atomically, list, describe) and 1 optional publisher (parameter events).
  `default` therefore has 7 services (parameter server + one of the
  application's) and 7 publishers (6 + parameter events); `large` makes the
  same allowance for both nodes.
- `small` keeps 1 service and 1 client rather than 0: the rmw static pools are
  arrays sized by these limits.
- `RMW_UXRCE_STREAM_HISTORY` must be a power of two. Each reliable stream
  buffers MTU × stream history bytes per direction, and the static input
  buffer pool scales with `MAX_HISTORY`, so MTU and the two history values are
  the main RAM levers of the library.
- `RCUTILS_NO_PROCESS_SUPPORT` (rcutils since Lyrical; Jazzy ignores it)
  leaves out rcutils' process API, which needs `sys/wait.h`.
- `RMW_UXRCE_TRANSPORT=custom`, together with the XRCE client's custom
  transport and stream framing, lets the port register UART (framed) and UDP
  (packet) transports at run time.
