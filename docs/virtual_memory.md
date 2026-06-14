# Virtual Memory Simulation Tab

This document describes the virtual memory simulation feature added to UCLM Ripes on the `caching` branch. The feature introduces a new **Virtual Memory** tab that simulates paginated address translation with an optional TLB, following the model used in Práctica 6 of the computer architecture lab course (reference simulator: SJM).

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Core Simulation — VMemSim](#core-simulation--vmemsim)
4. [Processor Integration — VMemShim](#processor-integration--vmemshim)
5. [UI Widgets](#ui-widgets)
   - [VMemConfigWidget](#vmemconfigwidget)
   - [VMemStatsWidget](#vmemstatswidget)
   - [VMemTransactionWidget](#vmemtransactionwidget)
   - [VMemTableWidget](#vmemtablewidget)
6. [Tab Layout — VMemTab](#tab-layout--vmemtab)
7. [Main Window Integration](#main-window-integration)
8. [Build System](#build-system)
9. [Address Translation Algorithm](#address-translation-algorithm)
10. [Mean Translation Time Formulas](#mean-translation-time-formulas)
11. [Undo / Step-back Support](#undo--step-back-support)

---

## Overview

The virtual memory tab lets students observe, step by step, how a RISC-V program's memory accesses are translated from virtual addresses to physical addresses. Every clock cycle the processor issues (instruction fetch + optional data read/write), the simulator intercepts those addresses, runs them through the configured TLB and page table, and updates the UI in real time.

Key capabilities:

- Fully configurable address space (page size, virtual/physical address bits)
- Optional fully-associative TLB with LRU replacement
- Fully-associative page table with LRU replacement and round-robin frame allocation
- Per-cycle translation breakdown (VPN, offset, TLB result, PT result, PFN, cycle cost)
- Aggregate statistics (hit rates, page fault rate, mean translation time)
- Live TLB and page table state tables with row highlighting
- Full undo support — stepping the processor backwards reverses all simulator state

---

## Architecture

The feature follows the same layered pattern as the existing cache simulation:

```
ProcessorHandler
      │  signals: processorClocked / processorReset / processorReversed
      ▼
  VMemShim          — translates processor events into VMemSim calls
      │  shared_ptr<VMemSim>
      ▼
  VMemSim           — simulation engine (TLB + page table + statistics)
      │  Qt signals: dataChanged / statsChanged / configurationChanged
      ▼
  UI widgets        — VMemConfigWidget, VMemStatsWidget,
                      VMemTransactionWidget, VMemTableWidget
```

All components live in `src/vmemsim/`. The top-level `VMemTab` in `src/vmemtab.cpp` assembles them into the tab panel.

---

## Core Simulation — VMemSim

**Files:** `src/vmemsim/vmemsim.h`, `src/vmemsim/vmemsim.cpp`

`VMemSim` is a `QObject` that owns all simulation state and exposes a clean interface to both the shim and the UI.

### Configuration parameters

| Parameter | Default | Meaning |
|---|---|---|
| `m_pageOffsetBits` | 4 | log₂(page size in bytes) — page size = 2^n bytes |
| `m_vpnBits` | 4 | VPN width — determines 2^n virtual pages |
| `m_pfnBits` | 3 | PFN width — determines 2^n physical frames |
| `m_mainMemCycles` | 20 | Cycles to walk the page table (main memory latency) |
| `m_diskCycles` | 2000 | Cycles penalty for a page fault (disk load) |
| `m_tlbEnabled` | true | Whether the TLB is active |
| `m_tlbEntries` | 4 | Number of TLB slots (fully associative) |
| `m_tlbCycles` | 1 | Cycles to probe the TLB |

Every setter calls `reset()` and emits `configurationChanged()`, which causes all UI widgets to rebuild their displayed state.

Setters that only affect cycle costs (`setMainMemCycles`, `setDiskCycles`, `setTLBCycles`) skip the reset and only recompute the mean translation time, so existing simulation state is preserved.

### Data structures

```cpp
struct TLBEntry {
    uint32_t vpn;    // virtual page number stored in this slot
    uint32_t pfn;    // mapped physical frame number
    bool valid;      // whether this slot holds a valid mapping
    unsigned lru;    // LRU counter: 0 = most recently used
};

struct PageTableEntry {
    uint32_t vpn;
    uint32_t pfn;
    bool valid;
    unsigned lru;
};

struct VMemTransaction {
    AInt virtualAddress;
    uint32_t vpn;
    uint32_t pageOffset;
    uint32_t pfn;
    AInt physicalAddress;
    bool tlbHit;       // only meaningful when TLB is enabled
    bool tlbMiss;
    bool pageFault;    // true when VPN was not in the page table
    MemoryAccess::Type type;
    int cyclesCost;    // translation cost for this single access
};

struct VMemAccessTrace {
    int totalAccesses;
    int tlbHits;
    int tlbMisses;
    int pageFaults;
    int pageTableHits;
    double meanTranslationTime;   // updated after every access
    double tlbHitRate() const;
    double pageFaultRate() const;
};
```

### Public interface

```cpp
void access(AInt virtualAddress, MemoryAccess::Type type);
void reset();
void undo();

const VMemAccessTrace &getAccessTrace() const;
const std::vector<VMemTransaction> &getTransactionHistory() const;
const std::vector<TLBEntry> &getTLBState() const;
const std::vector<PageTableEntry> &getPageTableState() const;
VMemTransaction getLastTransaction() const;
```

### Signals

| Signal | When emitted |
|---|---|
| `configurationChanged()` | Any parameter setter that triggers a reset |
| `dataChanged(VMemTransaction)` | After each `access()` call (skipped during fast-run mode) |
| `statsChanged()` | After each `access()`, `reset()`, or `undo()` |

`dataChanged` is suppressed while `ProcessorHandler::isRunning()` is true (fast-run mode) to avoid flooding the UI with thousands of updates per second. Stats and table state are still updated; the UI refreshes once when the run finishes.

---

## Processor Integration — VMemShim

**Files:** `src/vmemsim/vmemshim.h`, `src/vmemsim/vmemshim.cpp`

`VMemShim` owns the `VMemSim` instance (via `shared_ptr`) and bridges it to the processor lifecycle:

```cpp
class VMemShim : public QObject {
public:
    explicit VMemShim(QObject *parent = nullptr);
    std::shared_ptr<VMemSim> getVMemSim() const { return m_vmemSim; }

private:
    void processorWasClocked();   // → calls access() for data + instr
    void processorReset();        // → calls reset()
    void processorReversed();     // → calls undo()

    std::shared_ptr<VMemSim> m_vmemSim;
};
```

On each clock, `processorWasClocked()` reads both memory accesses from the processor and forwards each one to `VMemSim::access()`:

```cpp
void VMemShim::processorWasClocked() {
    const auto dataAccess  = ProcessorHandler::getProcessor()->dataMemAccess();
    const auto instrAccess = ProcessorHandler::getProcessor()->instrMemAccess();

    if (dataAccess.type == MemoryAccess::Write || dataAccess.type == MemoryAccess::Read)
        m_vmemSim->access(dataAccess.address, dataAccess.type);

    if (instrAccess.type == MemoryAccess::Read)
        m_vmemSim->access(instrAccess.address, MemoryAccess::Read);
}
```

The connection uses `Qt::DirectConnection` for `processorClocked` so the access is recorded in the same thread as the clock event, consistent with how `L1CacheShim` is connected.

---

## UI Widgets

All widgets receive a `shared_ptr<VMemSim>` in their constructor and connect to its signals to stay in sync.

### VMemConfigWidget

**Files:** `src/vmemsim/vmemconfigwidget.h`, `src/vmemsim/vmemconfigwidget.cpp`

Displays a `QGroupBox` with spinboxes for all 7 configuration parameters, plus derived read-only labels and an address structure panel.

**Controls:**

| Control | Range | Derived label |
|---|---|---|
| Page offset bits | 1–16 | `Page size: N bytes` |
| VPN bits | 1–16 | `Pages: N` |
| PFN bits | 1–16 | `Frames: N` |
| Main memory / PT access time | 1–100 000 cycles | — |
| Page fault penalty | 1–1 000 000 cycles | — |
| Enable TLB (checkbox) | — | enables/disables TLB group |
| TLB entries | 1–256 | — |
| TLB access time | 1–100 cycles | — |

**Address structure panel** (monospaced font, updates live):

```
Virtual:  [ VPN: 4b | Offset: 4b ]
Physical: [ PFN: 3b | Offset: 4b ]
```

**Signal flow:** Each spinbox connects directly to the matching `VMemSim` setter via `valueChanged`. The widget also listens to `VMemSim::configurationChanged()` and calls `blockSignals(true)` on all controls before syncing them back from the model, preventing re-entrant setter calls.

A **Reset** button in the config panel (and a duplicate toolbar action in `VMemTab`) both call `VMemSim::reset()`.

---

### VMemStatsWidget

**Files:** `src/vmemsim/vmemstatswidget.h`, `src/vmemsim/vmemstatswidget.cpp`

Displays a summary of the `VMemAccessTrace` in a form layout inside a `QGroupBox`:

| Field | Always shown |
|---|---|
| Total accesses | Yes |
| TLB hit rate | Only when TLB is enabled |
| TLB miss rate | Only when TLB is enabled |
| Page fault rate | Yes |
| Mean translation time | Yes |

The TLB rows are wrapped in a `QWidget` container so they can be hidden cleanly with `setVisible()` without disturbing the layout. They are shown/hidden in response to `configurationChanged()`.

Values update on every `statsChanged()` signal, formatted as percentages (2 decimal places) for rates and as `X.XX cycles` for mean translation time.

---

### VMemTransactionWidget

**Files:** `src/vmemsim/vmemtransactionwidget.h`, `src/vmemsim/vmemtransactionwidget.cpp`

Shows the full breakdown of the most recent address translation in a form layout (monospaced font throughout):

| Field | Format |
|---|---|
| Virtual address | `0xHHHH` |
| VPN | `BBBB (0xH)` — binary + hex |
| Page offset | `BBBB (0xH)` — binary + hex |
| TLB result | `HIT` / `MISS` / `N/A` |
| PT result | `HIT` / `PAGE FAULT` |
| PFN | `BBBB (0xH)` |
| Physical address | `0xHHHH` |
| Translation cost | `N cycles` |
| Access type | `Read` / `Write` / `—` |

All fields show `—` before any access has occurred. The widget connects to `VMemSim::dataChanged(VMemTransaction)` and updates every field on each new transaction.

---

### VMemTableWidget

**Files:** `src/vmemsim/vmemtablewidget.h`, `src/vmemsim/vmemtablewidget.cpp`

Renders two `QTableWidget`s side by side: one for the TLB and one for the page table.

**Columns (both tables):** `Index | Valid | VPN | PFN | LRU`

**Row colouring:**

| Colour | Meaning |
|---|---|
| Yellow (`#FFFF64`) | The row accessed in the most recent transaction |
| Light green (`#90EE90`) | Entry is valid |
| Light grey (`#D3D3D3`) | Entry is invalid |

**Flash animation:** When a new mapping is inserted (page fault → new page table entry, or TLB miss → new TLB entry), the newly added row is highlighted yellow for 600 ms via `QTimer::singleShot`, then transitions to green. This visually distinguishes a new allocation from a regular hit, which stays yellow indefinitely until the next transaction moves the highlight elsewhere.

**TLB visibility:** The TLB container widget is shown or hidden based on `m_vmemSim->getTLBEnabled()`, reacting to `configurationChanged()`. When the TLB is disabled, only the page table is shown.

**Row count:** Both tables are fully rebuilt (`rebuildTables()`) on every `configurationChanged()` to match the new `getTLBEntries()` and `getPageCount()` values.

---

## Tab Layout — VMemTab

**Files:** `src/vmemtab.h`, `src/vmemtab.cpp`

`VMemTab` inherits `RipesTab` and assembles all widgets into a three-panel splitter layout:

```
┌──────────────────────┬──────────────────────────────────────────────────┐
│  VMemConfigWidget    │  VMemTransactionWidget                            │
│  (top)               │  (top, ~200 px)                                   │
│                      ├──────────────────────────────────────────────────┤
│  VMemStatsWidget     │  VMemTableWidget                                  │
│  (bottom)            │  (bottom, fills remaining space)                  │
│  (~300 px wide)      │                                                   │
└──────────────────────┴──────────────────────────────────────────────────┘
```

- Left panel is a vertical `QSplitter` (config on top, stats below)
- Right panel is a vertical `QSplitter` (transaction on top, tables below)
- A horizontal `QSplitter` separates the two panels; the left is fixed at ~300 px and the right stretches to fill the window

A **Reset** toolbar action (using `:/icons/reset.svg`) calls `VMemSim::reset()`.

The tab disables itself while the processor is running (same pattern as `CacheTab`):

```cpp
connect(ProcessorHandler::get(), &ProcessorHandler::runStarted,
        this, [this] { setEnabled(false); });
connect(ProcessorHandler::get(), &ProcessorHandler::runFinished,
        this, [this] { setEnabled(true); });
```

---

## Main Window Integration

**Modified files:** `src/mainwindow.h`, `src/mainwindow.cpp`

`VMemTabID` was added to the tab enum in `mainwindow.h` immediately after `IOTabID`.

In the `MainWindow` constructor (`mainwindow.cpp`):

```cpp
auto *vmemToolbar = addToolBar("Virtual Memory");
vmemToolbar->setVisible(false);
auto *vmemTab = new VMemTab(vmemToolbar, this);
m_stackedTabs->insertWidget(VMemTabID, vmemTab);
m_tabWidgets[VMemTabID] = {vmemTab, vmemToolbar};
```

The tab appears in the left sidebar with a RAM/memory icon.

---

## Build System

**Modified file:** `src/CMakeLists.txt`

```cmake
add_subdirectory(vmemsim)
```

`src/vmemsim/CMakeLists.txt` lists all source files in the subdirectory and defines a static library target that `src/CMakeLists.txt` links against.

---

## Address Translation Algorithm

For each virtual address `va`, the translation proceeds as follows:

```
vpn    = (va >> pageOffsetBits) & ((1 << vpnBits) - 1)
offset = va & ((1 << pageOffsetBits) - 1)
```

**Step 1 — TLB lookup** (only if TLB is enabled):

- Scan all TLB slots for a valid entry with a matching VPN.
- **TLB hit:** retrieve PFN directly, update LRU counters. Skip step 2.
- **TLB miss:** proceed to step 2.

**Step 2 — Page table lookup** (on TLB miss or when TLB is disabled):

- Scan all page table entries for a valid entry with a matching VPN.
- **PT hit:** retrieve PFN, update LRU counters.
- **PT miss (page fault):** allocate a physical frame using round-robin over frame numbers, checking that the chosen frame is not currently mapped. If all frames are occupied, evict the LRU page table entry (and invalidate its TLB entry if one exists). Add the new mapping to the page table.
- If the TLB is enabled, insert the resolved VPN→PFN mapping into the TLB (with LRU eviction if all TLB slots are full).

**Step 3 — Physical address construction:**

```
physicalAddress = (pfn << pageOffsetBits) | offset
```

**Step 4 — Record transaction and update statistics.**

---

## Mean Translation Time Formulas

These match the formulas in Práctica 6 of the lab manual.

### Without TLB

```
t_translation = t_mainMem + pageFaultRate × t_disk
```

Where `pageFaultRate = pageFaults / totalAccesses`.

### With TLB

```
t_translation = tlbHitRate × t_TLB
              + (1 − tlbHitRate) × (t_TLB + t_mainMem + pfConditional × t_disk)
```

Where `pfConditional = pageFaults / tlbMisses` (page fault rate among TLB misses only, not total accesses).

Both formulas are computed by `VMemSim::updateMeanTranslationTime()` and stored in `VMemAccessTrace::meanTranslationTime` after every access.

---

## Undo / Step-back Support

`VMemSim` maintains a `std::deque<VMemTrace>` that acts as a bounded undo stack. Before mutating any state during `access()`, a full snapshot is pushed:

```cpp
struct VMemTrace {
    VMemTransaction transaction;
    std::vector<TLBEntry> tlbSnapshot;
    std::vector<PageTableEntry> ptSnapshot;
    VMemAccessTrace traceSnapshot;
    uint32_t nextFrame;   // round-robin frame allocator state
};
```

The deque is capped at `vsrtl::core::ClockedComponent::reverseStackSize()`, keeping it in sync with the processor's own undo buffer so neither can go back further than the other.

When `VMemShim::processorReversed()` fires (user clicks the step-back button), `VMemSim::undo()` pops the top snapshot and restores all four pieces of state atomically. The UI is updated via `statsChanged()` and `dataChanged()`.
