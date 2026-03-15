# System Architecture

## Design Philosophy

The system is designed for **low latency** and **determinism**.

1. **Zero Allocations on Hot Path**: Once the system warms up, no memory is allocated during order processing.
2. **Lock-Free Communication**: The network thread communicates with the matching engine via a Single-Producer Single-Consumer (SPSC) ring buffer.
3. **Core Affinity**: (Planned) Threads will be pinned to specific CPU cores to maximize cache locality.

## Component Interaction

### 1. Network Layer (`TCPOrderGateway`)

* **Role**: Accepts TCP connections and reads raw bytes.
* **Thread**: Runs on a dedicated `IO Thread`.
* **Logic**:
  * Reads into a pre-allocated buffer.
  * Calls `FIXParser` to extract fields.
  * Constructs an `Order` object.
  * Pushes `Order` to `LockFreeQueue`.
  * *Crucial*: Does NOT block. If the queue is full, it yields (or drops in a real system).

### 2. The Bridge (`LockFreeQueue`)

* **Role**: Safe data transfer between threads.
* **Implementation**: A fixed-size `std::array` with atomic head/tail indices.
* **Memory Ordering**: Uses `memory_order_acquire` / `memory_order_release` to ensure the matching engine sees the order data only after it's fully written.

### 3. Matching Engine (`main.cpp` loop)

* **Role**: Processes orders and manages the book.
* **Thread**: Runs on a dedicated `Matching Thread` (isolated from OS noise).
* **Logic**:
  * Pops `Order` from queue.
  * Start Timer (`rdtsc`).
  * Calls `OrderBook::addOrder()`.
  * Calls `OrderBook::match()`.
  * Stop Timer (`rdtsc`).
  * Records latency.

### 4. Order Book (`IOrderBook`)

* **Role**: Maintains the state of Bids and Asks.
* **Current Implementation**: `MapOrderBook` (Baseline).
  * Uses `std::map<Price, List<Order>>`.
  * *Pros*: Simple, correct, ordered.
  * *Cons*: Pointer chasing, cache misses, dynamic allocation (node creation).
* **Future Implementations**:
  * `SkipListOrderBook`: Better locality, probabilistic balancing.
  * `ArrayOrderBook`: O(1) access, pre-allocated, best for dense price levels.

## Benchmarking Methodology

The system supports two distinct benchmarking paths to isolate performance characteristics.

### Direct Mode (Algorithmic)

- **Path**: `ScenarioGenerator -> BenchmarkRunner -> OrderBook`
* **Synchronization**: None (Synchronous local calls).
* **Timing**: `getCurrentTimeNs()` (nanoseconds via `std::chrono`) to measure sub-microsecond algorithmic latency.
* **Goal**: Measure pure logic speed excluding system noise.

### Gateway Mode (Systemic)

- **Path**: `MockClient -> TCP/FIX -> ExchangeGateway -> LockFreeQueue -> MatchingEngine`
* **Synchronization**: **SyncBarrier (Tag 596)**. The client waits for a specific processed count before requesting stats.
* **Timing**: `std::chrono` (nanoseconds) using **TransactionTime (Tag 60)** for True E2E tracking.
* **Goal**: Measure real-world systemic latency, including network I/O and queuing delays.

```mermaid
graph TD

    subgraph DirectPath["1. Direct Path (Algorithmic)"]
        BenchmarkObj[Benchmark Runner] -->|Local Call| Engine[Matching Engine]
        Engine -->|RDTSC Timing| LocalMetrics[Local Metrics]
    end

    subgraph GatewayPath["2. Gateway Path (Systemic)"]
        Client[Mock Client] -->|FIX over TCP| Server[Exchange Server]
        Server -->|SPSC Queue| ME[Matching Engine]
        ME -->|Tag 60 E2E Timing| ServerMetrics[Server Metrics]
        Client -.->|Tag 596 Sync Request| Server
    end
```

## Data Flow Diagram (Gateway Mode)

```mermaid
sequenceDiagram
    participant Client
    participant NetworkThread
    participant RingBuffer
    participant MatchingThread
    participant OrderBook

    Client->>NetworkThread: TCP Packet (FIX)
    NetworkThread->>NetworkThread: Parse FIX -> Order
    NetworkThread->>RingBuffer: Push(Order)
    RingBuffer->>MatchingThread: Pop(Order)
    MatchingThread->>OrderBook: addOrder(Order)
    MatchingThread->>OrderBook: match()
    OrderBook-->>MatchingThread: vector<Trade>
```
