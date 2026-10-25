# 01_publisher_udp

Publishes a counter (`std_msgs/Int32`) on `/c2000/counter` at 100 Hz over
Ethernet/UDP to a micro-ROS agent.

## Set-up

1. Connect the controlCARD's Ethernet port (J4) to a network card of the host
   PC and assign the static address `192.168.10.1/24`. The board uses
   `192.168.10.2` and expects the agent at `192.168.10.1:8888`; change
   [examples/common/udp_config.c](../common/udp_config.c) for other addresses
   (it applies to all UDP examples).
2. Set-up firewall on host PC (Windows):

   ```
   Set-NetConnectionProfile -InterfaceAlias "Ethernet" -NetworkCategory Private
   netsh advfirewall firewall add rule name="micro-ROS agent UDP 8888" dir=in action=allow protocol=UDP localport=8888
   ```

3. Start the agent (Docker Desktop running):

   ```
   docker compose -f tools/agent/docker-compose.yml up agent-udp
   ```

4. Build and load `cpu1_bootstrap` (CPU1) and `01_publisher_udp` (CM) as in
   the main README.

## Check

```
ping 192.168.10.2
docker compose -f tools/agent/docker-compose.yml run --rm ros2 ros2 topic hz /c2000/counter
```

LED D1: 1 Hz blink while waiting for the agent, on while connected, 5 Hz
blink if the initialization failed. In the debugger, `microros_f2838x_stats`
shows the session state, reconnects, UDP counters, heap and stack use.
