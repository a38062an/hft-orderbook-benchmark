## Keynote Screencast Plan — HFT Order Book Benchmarking
**Target: 7–9 minutes | Audience: Third-year CS students**

---

### Slide 1 — Hook / Title (0:00–0:25)
**Content:** Title card. "High-Frequency Trading Order Book Benchmarking in C++20"

**Image:** A dramatic real-time stock ticker or exchange trading floor photo (find a royalty-free one on Unsplash — search "stock exchange trading floor"). Dark background preferred.

**Animation:** Fade in title word by word. Subtitle and your name slide up after.

**Voiceover:** *"In high-frequency trading, firms compete to execute orders in microseconds. The data structure sitting at the heart of every exchange — the order book — can be the difference between profit and loss. This project asks: which design is actually fastest, and does it even matter end-to-end?"*

---

### Slide 2 — What is a Limit Order Book? (0:25–1:00)
**Content:** Show a clean visual of a LOB — bids on one side, asks on the other, spread in the middle. Use your Figure 2.1 from the dissertation (or recreate it cleanly in Keynote).

**Image:** Recreate the bid/ask table as a styled Keynote table — green for bids, red for asks. Animate rows appearing one by one.

**Animation:** Rows build from the spread outward — best bid appears first, then deeper levels. Arrow pointing to spread appears last.

**Voiceover:** *"An order book is just a live record of every buy and sell order waiting to be matched. Buyers queue up on the bid side, sellers on the ask. The gap between them is the spread. When a buyer and seller agree on price — a trade executes. The matching engine processes this thousands of times per second."*

---

### Slide 3 — The Problem + Research Gap (1:00–1:35)
**Content:** Two columns — "What we know" vs "What's missing." Keep it punchy, 3 bullets each.

**Image:** No photo needed here — use a clean split layout with a subtle dark gradient. Add a small icon (padlock or question mark) beside the "gap" column.

**Animation:** Left column builds first, then right column fades in with a highlight box around the gap.

**Voiceover:** *"There's plenty of academic work on HFT strategies. But the actual implementation details — which data structures, how they behave end-to-end — are commercially sensitive. Firms don't publish this. This project fills that gap with a reproducible, decomposition-driven benchmark."*

---

### Slide 4 — System Architecture Overview (1:35–2:15)
**Content:** Your hexagonal architecture diagram (Figure 3.2). Simplify it visually — label the 4 key components: Order Generator, TCP Gateway, Matching Engine, Order Book.

**Image:** Use your own diagram but clean it up. In Keynote, use coloured boxes with arrows. Make the Order Book box glow or pulse slightly to draw attention.

**Animation:** Components appear one at a time left-to-right as you narrate each one. Arrows draw themselves between components (use "draw" line animation).

**Voiceover:** *"The system I built is a fully modular C++20 exchange. Orders flow from a synthetic generator, through a real TCP gateway parsing FIX protocol messages, into a lock-free queue, and finally to the matching engine. The key insight is that the order book plugs in as a swappable component — so I can benchmark five completely different designs under identical conditions."*

---

### Slide 5 — The 5 Implementations (2:15–2:50)
**Content:** One row per implementation with a name, one-line description, and a visual icon or mini-diagram. Array, Map, Vector, Hybrid, Pool.

**Image:** For Array — show a flat row of coloured boxes (contiguous). For Map — show a small tree node diagram. Keep icons minimal and consistent.

**Animation:** Each row slides in one at a time. Highlight Array in a different colour (gold/yellow) since it's your star.

**Voiceover:** *"Five implementations. The baseline Map uses a red-black tree — flexible but pointer-heavy. The Array directly indexes prices into a contiguous block of memory — O(1) access, cache-friendly. The Vector maintains sorted order. The Hybrid tiers hot and cold prices. The Pool pre-allocates memory upfront to eliminate runtime allocation."*

---

### Slide 6 — Two-Mode Philosophy (2:50–3:15)
**Content:** Two large icons/panels side by side. Left: "Direct Mode — pure algorithm." Right: "Gateway Mode — full system." Include a one-line description of what each measures.

**Animation:** Left panel builds, then right panel builds. A "vs" badge animates in between.

**Voiceover:** *"The benchmark runs in two modes. Direct mode strips away all networking — it measures the raw speed of the data structure itself. Gateway mode adds the full TCP stack, FIX parsing, and queue handoff. This lets me ask: do engine-level gains survive in the real world?"*

---

### Slide 7 — CLI / Terminal Demo (3:15–3:50)
**Content:** Screen recording of your benchmark running. Show the terminal — ideally the Python driver kicking off a benchmark run, output appearing, CSV being written.

**What to show specifically:**
- The command you run (e.g. `python run_benchmarks.py --mode direct --scenario mixed`)
- Output lines scrolling (order counts, timing output)
- The CSV file being produced (a quick `ls` or file open)

**Animation:** None needed — raw terminal recording. Put a thin rounded border around the terminal window in Keynote to frame it nicely.

**Voiceover:** *"Here's the system running. The Python driver sweeps across all five implementations and seven scenarios, collecting 30 replicate runs per configuration. Every result is written to a structured CSV for analysis."*

---

### Slide 8 — Direct Mode Results: Latency Heatmap (3:50–4:30)
**Content:** Your Figure 5.3 — the direct-mode mean latency heatmap. This is your headline result.

**Image:** Use the actual figure from your dissertation. If you can export it as a high-res PNG, great. Crop tightly.

**Animation:** Slide the heatmap in. Then use Keynote's "spotlight" or a coloured overlay rectangle to highlight the Array column first, then the Vector's dense_full cell (the dramatic 15,340 ns cell).

**Voiceover:** *"In direct mode, the Array wins in every single scenario — staying between 206 and 224 nanoseconds regardless of book depth. That's the O(1) direct index at work. The Vector collapses catastrophically in a full book — 15,000 nanoseconds — because sorted insertions require shifting thousands of elements. This is cache misses made visible."*

---

### Slide 9 — Why Array Wins: Cache Intuition (4:30–5:00)
**Content:** Simple visual of contiguous memory (array) vs fragmented heap (map/pointer chasing). You have Figure 2.4 — use that, or redraw it cleanly.

**Image:** Two rows: top row is coloured boxes touching each other (array, "cache-friendly"). Bottom row is boxes scattered with dotted pointer arrows between them (map, "pointer chasing"). Label "L1 Cache hit" above array, "Cache miss → RAM stall" above map.

**Animation:** Array boxes light up green sequentially. Map pointer arrows appear one at a time with a red flash and a "stall" label.

**Voiceover:** *"The reason is mechanical sympathy. Arrays are contiguous — the CPU's prefetcher loads the next elements automatically into L1 cache. Trees scatter nodes across the heap. Every pointer dereference is a potential cache miss, forcing a 100+ nanosecond stall waiting for RAM. Linux perf confirmed this — map incurred 1.4 million L1 misses versus 843k for array in the mixed scenario."*

---

### Slide 10 — H2: Memory Pooling (5:00–5:25)
**Content:** A focused comparison: Pool vs Map vs Array in the mixed scenario. Pull the key numbers from Table 5.6. Show insert latency specifically (298 ns pool vs 420 ns map vs 46 ns array).

**Animation:** Three bars animate up. Pool bar is coloured differently. A bracket annotates the pool-vs-map improvement. A ceiling line shows where Array sits far below.

**Voiceover:** *"Memory pooling genuinely helps. The Pool beats Map on inserts — 298 vs 420 nanoseconds — by eliminating runtime heap allocation. But it can't overcome its own branching logic during matching: 121 ns vs 59 ns for map. Pooling is an optimisation, not a magic fix."*

---

### Slide 11 — Gateway Mode: The Masking Effect (5:25–6:10)
**Content:** Table 5.8 — the gateway decomposition. Show the three columns: Net, Queue, Engine. Make the Engine column tiny and visually dwarfed.

**Image:** Use a stacked bar chart built in Keynote (or import your Figure 5.10). Colour the Network slice in a dominant blue, queue in orange, engine in a tiny sliver of green.

**Animation:** Bar builds from left — network fills up almost the entire bar. Then queue. Then a tiny engine sliver appears last. Text label: "Engine: 62 ns" appears with an arrow pointing at the sliver.

**Voiceover:** *"In gateway mode, everything changes. For the Array implementation, the network stack alone consumes 2,014,045 nanoseconds out of a 2,100,661 total. The matching engine? 62 nanoseconds. In direct mode, Array and Map are 400 ns apart with p less than 0.0001. In gateway mode, that separation disappears — p equals 0.21, statistically indistinguishable. The network noise is louder than the signal."*

---

### Slide 12 — MPSC Contention (6:10–6:35)
**Content:** Your Figure 5.12 — throughput scaling. Keep it brief. One key takeaway: queue grows, engine stays flat.

**Animation:** Show the graph with the Array line highlighted. Circle the point where throughput starts dropping.

**Voiceover:** *"Under multi-producer contention, throughput drops as producers compete for the queue. But the engine processing time stays flat at 68–74 nanoseconds across all producer counts. The bottleneck is always the queue, never the algorithm itself — further evidence that system overhead dominates."*

---

### Slide 13 — Practical Takeaways (6:35–7:10)
**Content:** Three clean bullet points. Keep each to one line.
1. Use direct-indexed arrays for latency-critical matching
2. Deploy memory pooling only for allocation-heavy scenarios
3. The real gains are in the transport layer — kernel bypass is the next step

**Animation:** Each bullet appears with a tick icon animating in. Brief pause between each.

**Voiceover:** *"Three takeaways. First — if your workload allows fixed price ranges, the array is the clear choice. Second — memory pooling is a targeted optimisation, not a universal one. Third — and this is the systems insight — if you want end-to-end gains, the matching engine is not where to look. Kernel bypass technologies like DPDK are the real frontier."*

---

### Slide 14 — Closing / Future Work (7:10–7:45)
**Content:** Two future directions, and a closing line. DPDK kernel bypass + FPGA hardware acceleration.

**Image:** A subtle image of a server rack or chip (Unsplash, royalty-free, search "server hardware"). Overlay your text on a dark semi-transparent panel.

**Animation:** Title fades in. The two future directions slide up. Final closing statement fades in last.

**Voiceover:** *"The framework is built to extend. The next logical step is integrating kernel bypass to remove the OS from the hot path entirely — then the engine-level differences I found in direct mode might actually show up end-to-end. The Array's O(1) structure is also a natural candidate for FPGA offloading. Order book performance is a system-level problem — this project provides the evidence base to know where to look."*

---

## Timing Summary

| Slide | Topic | Duration |
|---|---|---|
| 1 | Hook / Title | 0:25 |
| 2 | What is a LOB | 0:35 |
| 3 | Problem + Gap | 0:35 |
| 4 | Architecture | 0:40 |
| 5 | 5 Implementations | 0:35 |
| 6 | Two-Mode Philosophy | 0:25 |
| 7 | CLI Demo | 0:35 |
| 8 | Direct Mode Results | 0:40 |
| 9 | Why Array Wins | 0:30 |
| 10 | H2: Memory Pooling | 0:25 |
| 11 | Gateway / Masking | 0:45 |
| 12 | MPSC Contention | 0:25 |
| 13 | Takeaways | 0:35 |
| 14 | Closing | 0:35 |
| **Total** | | **~7:45** |
