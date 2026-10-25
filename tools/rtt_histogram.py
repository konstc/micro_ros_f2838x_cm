#!/usr/bin/env python3
"""Round-trip time to the board, for example 02_ping_pong_udp.

Publishes std_msgs/Header on /c2000/ping and matches the board's answers on
/c2000/pong (best effort both ways) by frame_id, a sequence number. Prints the
number of lost pings, round-trip time percentiles, a text histogram and the
clock offset between board and PC: the pong's stamp is the board's time
synchronized with the agent, compared with the midpoint of the ping's send
and receive times.

Runs in the ros2 service of tools/agent/docker-compose.yml, which mounts
tools/ at /tools:

  docker compose -f tools/agent/docker-compose.yml run --rm ros2 \\
      python3 /tools/rtt_histogram.py --count 1000 --rate 100

With --csv the samples go to standard output and the report to standard
error; add -T to "run" so that the output can be redirected:

  docker compose -f tools/agent/docker-compose.yml run --rm -T ros2 \\
      python3 /tools/rtt_histogram.py --csv > rtt.csv
"""

import argparse
import csv
import math
import sys
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import Header

PING_TOPIC = "/c2000/ping"
PONG_TOPIC = "/c2000/pong"
SETTLE_S = 1.0          # after discovery, before the first ping
HISTOGRAM_BINS = 16
BAR_WIDTH = 40


class PingPong(Node):
    def __init__(self):
        super().__init__("rtt_histogram")
        qos = QoSProfile(reliability=ReliabilityPolicy.BEST_EFFORT,
                         history=HistoryPolicy.KEEP_LAST, depth=100)
        self.publisher = self.create_publisher(Header, PING_TOPIC, qos)
        self.create_subscription(Header, PONG_TOPIC, self.on_pong, qos)
        self.pending = {}   # sequence -> (monotonic ns, epoch ns) when sent
        self.samples = []   # (sequence, rtt ns, board stamp ns, offset ns or None)

    def found_board(self):
        return (self.publisher.get_subscription_count() > 0 and
                self.count_publishers(PONG_TOPIC) > 0)

    def ping(self, sequence):
        message = Header()
        message.frame_id = str(sequence)
        epoch = time.time_ns()
        message.stamp.sec, message.stamp.nanosec = divmod(epoch, 1_000_000_000)
        self.pending[sequence] = (time.monotonic_ns(), epoch)
        self.publisher.publish(message)

    def on_pong(self, message):
        received_mono = time.monotonic_ns()
        received_epoch = time.time_ns()
        try:
            sequence = int(message.frame_id)
        except ValueError:
            return
        sent = self.pending.pop(sequence, None)
        if sent is None:
            return      # not one of ours, or a duplicate
        board = message.stamp.sec * 1_000_000_000 + message.stamp.nanosec
        offset = board - (sent[1] + received_epoch) // 2 if board else None
        self.samples.append((sequence, received_mono - sent[0], board, offset))

def spin_until(node, deadline):
    while (remaining := deadline - time.monotonic()) > 0:
        rclpy.spin_once(node, timeout_sec=remaining)

def percentile(values, fraction):
    """Nearest-rank percentile of sorted values."""
    rank = math.ceil(fraction * len(values))
    return values[min(max(rank, 1), len(values)) - 1]

def print_histogram(rtts, out):
    low, high = rtts[0], percentile(rtts, 0.99)
    high = max(high, low + 0.001)
    width = (high - low) / HISTOGRAM_BINS
    counts = [0] * HISTOGRAM_BINS
    above = 0
    for value in rtts:
        if value > high:
            above += 1
        else:
            counts[min(int((value - low) / width), HISTOGRAM_BINS - 1)] += 1
    peak = max(counts + [above])

    def line(label, count):
        bar = "#" * round(BAR_WIDTH * count / peak)
        print(f"  {label} | {bar:<{BAR_WIDTH}} | {count}", file=out)

    print("histogram, ms:", file=out)
    for index, count in enumerate(counts):
        line(f"{low + index * width:8.3f} - {low + (index + 1) * width:8.3f}", count)
    if above:
        line(f"{'above':>8}   {high:8.3f}", above)

def print_report(node, sent, out):
    answered = len(node.samples)
    lost = sent - answered
    print(f"pings: {sent} sent, {answered} answered, {lost} lost "
          f"({100.0 * lost / sent:.1f} %)", file=out)
    if not answered:
        return
    rtts = sorted(sample[1] / 1e6 for sample in node.samples)
    print(f"round trip, ms: min {rtts[0]:.3f}  median {percentile(rtts, 0.5):.3f}  "
          f"p95 {percentile(rtts, 0.95):.3f}  p99 {percentile(rtts, 0.99):.3f}  "
          f"max {rtts[-1]:.3f}", file=out)
    print_histogram(rtts, out)
    offsets = sorted(sample[3] / 1e6 for sample in node.samples if sample[3] is not None)
    if offsets:
        print(f"clock offset board - PC, ms: median {percentile(offsets, 0.5):.3f}  "
              f"min {offsets[0]:.3f}  max {offsets[-1]:.3f}  ({len(offsets)} pongs)",
              file=out)
    else:
        print("clock offset: no pong carried a synchronized time "
              "(the board's rmw_uros_sync_session() failed)", file=out)

def write_csv(node):
    writer = csv.writer(sys.stdout)
    writer.writerow(["sequence", "rtt_us", "board_stamp_ns", "offset_us"])
    for sequence, rtt, board, offset in sorted(node.samples):
        writer.writerow([sequence, f"{rtt / 1e3:.1f}", board,
                         "" if offset is None else f"{offset / 1e3:.1f}"])

def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--count", type=int, default=1000, help="pings to send (default 1000)")
    parser.add_argument("--rate", type=float, default=100.0,
                        help="pings per second (default 100)")
    parser.add_argument("--wait", type=float, default=1.0,
                        help="seconds to wait for late pongs (default 1)")
    parser.add_argument("--discovery-timeout", type=float, default=15.0,
                        help="seconds to wait for the board's topics (default 15)")
    parser.add_argument("--csv", action="store_true",
                        help="write the samples as CSV to standard output, "
                             "the report to standard error")
    args = parser.parse_args()
    out = sys.stderr if args.csv else sys.stdout

    rclpy.init()
    node = PingPong()
    try:
        deadline = time.monotonic() + args.discovery_timeout
        while not node.found_board():
            if time.monotonic() > deadline:
                print(f"no board: {PING_TOPIC} has no subscriber or {PONG_TOPIC} "
                      "no publisher", file=out)
                return 1
            rclpy.spin_once(node, timeout_sec=0.1)
        spin_until(node, time.monotonic() + SETTLE_S)

        next_ping = time.monotonic()
        for sequence in range(args.count):
            node.ping(sequence)
            next_ping += 1.0 / args.rate
            spin_until(node, next_ping)
        deadline = time.monotonic() + args.wait
        while node.pending and time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=deadline - time.monotonic())

        print_report(node, args.count, out)
        if args.csv:
            write_csv(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0

if __name__ == "__main__":
    sys.exit(main())
