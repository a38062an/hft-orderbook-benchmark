# HFT Orderbook Benchmark

## Overview

This project is a systematic benchmarking of order book data structures and reader/writer synchronization strategies for low-latency trading systems.

## Structure

- `src/core`: Core abstractions (Order, Trade, IOrderBook)
- `src/orderbooks`: Order book implementations (Map, SkipList, Array)
- `src/sync`: Synchronization wrappers (Coarse Lock, RCU, etc.)
- `src/network`: TCP Gateway and FIX Parser
- `src/utils`: Instrumentation and utilities

## Building

```bash
mkdir build && cd build
cmake ..
make -j
```

## Running

### Start the matching engine server

```bash
cd build/src
./hft_exchange_server
```

### Send orders from a client

```bash
# C++ client (sends 1M orders by default)
cd build/src
./tcp_order_sender [num_orders]

# Python client
python3 clients/tcp_order_sender.py
```

### Order Generation Details

The `tcp_order_sender` generates random orders with:

- **Side**: Random (1=Buy, 2=Sell) - 50/50 distribution
- **Price**: Random between 90-110 (uniform distribution)
- **Quantity**: Random between 1-100 shares
- **Order Type**: Limit orders (FIX tag 40=2)
- **Format**: FIX 4.2 protocol messages

Orders are generated with overlapping price ranges to create matching opportunities:

- Buy orders at prices 90-110
- Sell orders at prices 90-110
- Orders with price overlap will match (e.g., Buy@105 matches Sell@100)

### Output

After stopping the server (Ctrl+C), you'll see:

- **Matching Statistics**: Orders processed, trades executed, volume traded, match rate
- **Latency Statistics**: P50, P99, Max, Mean processing time in CPU cycles
