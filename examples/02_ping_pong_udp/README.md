# 02_ping_pong_udp

Answers every `std_msgs/Header` on `/c2000/ping` with one on `/c2000/pong`
(best effort both ways), so that the round-trip time between host PC and board can be measured. The pong carries the ping's `frame_id` and, as its stamp, the
board's clock synchronized with the agent (`rmw_uros_sync_session()` at
connection and every 10 s).

## Set-up

As for [01_publisher_udp](../01_publisher_udp/README.md#set-up): network card,
firewall and agent, then load `cpu1_bootstrap` (CPU1) and `02_ping_pong_udp`
(CM).

## Measure

```
docker compose -f tools/agent/docker-compose.yml run --rm ros2 python3 /tools/rtt_histogram.py --count 1000 --rate 100
```

[tools/rtt_histogram.py](../../tools/rtt_histogram.py) prints the lost pings,
the round-trip time (min, median, p95, p99, max), a histogram and the clock
offset between board and PC. Histogram example:

```
> docker compose -f tools/agent/docker-compose.yml run --rm ros2 python3 /tools/rtt_histogram.py
Container microros-f2838x-ros2-run-650bb4398b08 Creating
Container microros-f2838x-ros2-run-650bb4398b08 Created
pings: 1000 sent, 1000 answered, 0 lost (0.0 %)
round trip, ms: min 1.747  median 2.621  p95 4.943  p99 8.317  max 23.961
histogram, ms:
     1.747 -    2.158 | ############                             | 104
     2.158 -    2.568 | ######################################## | 336
     2.568 -    2.979 | #####################################    | 309
     2.979 -    3.389 | ############                             | 102
     3.389 -    3.800 | #######                                  | 58
     3.800 -    4.211 | ##                                       | 20
     4.211 -    4.621 | #                                        | 12
     4.621 -    5.032 | #                                        | 9
     5.032 -    5.443 | #                                        | 8
     5.443 -    5.853 | #                                        | 6
     5.853 -    6.264 |                                          | 4
     6.264 -    6.675 | #                                        | 7
     6.675 -    7.085 |                                          | 2
     7.085 -    7.496 |                                          | 3
     7.496 -    7.907 | #                                        | 5
     7.907 -    8.317 | #                                        | 5
     above      8.317 | #                                        | 10
clock offset board - PC, ms: median -0.353  min -2.378  max 7.334  (1000 pongs)
```

To keep the samples as CSV:

```
docker compose -f tools/agent/docker-compose.yml run --rm -T ros2 python3 /tools/rtt_histogram.py --csv > rtt.csv
```

LED D1 and `microros_f2838x_stats` behave as in [01_publisher_udp](../01_publisher_udp/README.md).
