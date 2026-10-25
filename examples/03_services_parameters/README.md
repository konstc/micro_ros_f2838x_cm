# 03_services_parameters

Publishes a counter (`std_msgs/Int32`) on `/c2000/counter` at the rate given by
the parameter `rate_hz` (rclc parameter server; 1 to 200 Hz, 10 Hz at start).
The service `/c2000/reset` (`std_srvs/Trigger`) resets the counter.

## Set-up

As for [01_publisher_udp](../01_publisher_udp/README.md#set-up): network card,
firewall and agent, then load `cpu1_bootstrap` (CPU1) and
`03_services_parameters` (CM).

## Check

In two shells in the `ros2` container
(`docker compose -f tools/agent/docker-compose.yml run --rm ros2 bash`):

```
ros2 topic hz /c2000/counter
```

```
ros2 param set /c2000_node rate_hz 100
ros2 param describe /c2000_node rate_hz
ros2 service call /c2000/reset std_srvs/srv/Trigger
```

`ros2 topic hz` changes from about 10 Hz to about 100 Hz; values outside 1 to 200
are refused. After the reset, `ros2 topic echo /c2000/counter` counts from 0.
LED D1 and `microros_f2838x_stats` behave as in [01_publisher_udp](../01_publisher_udp/README.md).
