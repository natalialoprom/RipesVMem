#include "vmemsim.h"

#include "processorhandler.h"

#include <algorithm>

namespace Ripes {

// ── Construction ─────────────────────────────────────────────────────────────

VMemSim::VMemSim(QObject *parent) : QObject(parent) {
  updateConfiguration();
}

// ── Address decomposition ────────────────────────────────────────────────────

uint32_t VMemSim::extractVPN(AInt va) const {
  const uint32_t vpnMask = (1u << m_vpnBits) - 1u;
  return static_cast<uint32_t>((va >> m_pageOffsetBits) & vpnMask);
}

uint32_t VMemSim::extractOffset(AInt va) const {
  const uint32_t offsetMask = (1u << m_pageOffsetBits) - 1u;
  return static_cast<uint32_t>(va & offsetMask);
}

// ── TLB helpers ──────────────────────────────────────────────────────────────

int VMemSim::tlbLookup(uint32_t vpn) const {
  for (int i = 0; i < static_cast<int>(m_tlb.size()); ++i) {
    if (m_tlb[i].valid && m_tlb[i].vpn == vpn)
      return i;
  }
  return -1;
}

void VMemSim::tlbUpdateLRU(int idx) {
  const unsigned prevLRU = m_tlb[idx].lru;
  for (auto &e : m_tlb) {
    if (e.valid && e.lru < prevLRU)
      ++e.lru;
  }
  m_tlb[idx].lru = 0;
}

void VMemSim::tlbInsert(uint32_t vpn, uint32_t pfn) {
  // Prefer an invalid slot first.
  for (int i = 0; i < static_cast<int>(m_tlb.size()); ++i) {
    if (!m_tlb[i].valid) {
      m_tlb[i] = {vpn, pfn, true, static_cast<unsigned>(m_tlb.size() - 1)};
      tlbUpdateLRU(i);
      return;
    }
  }
  // All slots valid — evict LRU (highest lru value).
  int lruIdx = 0;
  unsigned maxLRU = 0;
  for (int i = 0; i < static_cast<int>(m_tlb.size()); ++i) {
    if (m_tlb[i].lru >= maxLRU) {
      maxLRU = m_tlb[i].lru;
      lruIdx = i;
    }
  }
  m_tlb[lruIdx] = {vpn, pfn, true, static_cast<unsigned>(m_tlb.size() - 1)};
  tlbUpdateLRU(lruIdx);
}

// ── Page table helpers ───────────────────────────────────────────────────────

int VMemSim::ptLookup(uint32_t vpn) const {
  for (int i = 0; i < static_cast<int>(m_pageTable.size()); ++i) {
    if (m_pageTable[i].valid && m_pageTable[i].vpn == vpn)
      return i;
  }
  return -1;
}

void VMemSim::ptUpdateLRU(int idx) {
  const unsigned prevLRU = m_pageTable[idx].lru;
  for (auto &e : m_pageTable) {
    if (e.valid && e.lru < prevLRU)
      ++e.lru;
  }
  m_pageTable[idx].lru = 0;
}

uint32_t VMemSim::ptAllocate(uint32_t vpn) {
  const int frameCount = 1 << m_pfnBits;

  // Try round-robin allocation over physical frames.
  // Walk up to frameCount slots looking for a frame not currently mapped.
  for (int tries = 0; tries < frameCount; ++tries) {
    uint32_t candidate = m_nextFrame;
    m_nextFrame = (m_nextFrame + 1) % frameCount;

    bool inUse = false;
    for (const auto &e : m_pageTable) {
      if (e.valid && e.pfn == candidate) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      // Insert into page table.
      // If there is room (an invalid slot), use it.
      for (int i = 0; i < static_cast<int>(m_pageTable.size()); ++i) {
        if (!m_pageTable[i].valid) {
          m_pageTable[i] = {vpn, candidate, true,
                            static_cast<unsigned>(m_pageTable.size() - 1)};
          ptUpdateLRU(i);
          return candidate;
        }
      }
      // Page table is full — evict LRU entry.
      int lruIdx = 0;
      unsigned maxLRU = 0;
      for (int i = 0; i < static_cast<int>(m_pageTable.size()); ++i) {
        if (m_pageTable[i].lru >= maxLRU) {
          maxLRU = m_pageTable[i].lru;
          lruIdx = i;
        }
      }
      // Invalidate the evicted entry from TLB too.
      const uint32_t evictedVPN = m_pageTable[lruIdx].vpn;
      for (auto &te : m_tlb) {
        if (te.valid && te.vpn == evictedVPN)
          te.valid = false;
      }
      m_pageTable[lruIdx] = {vpn, candidate, true,
                             static_cast<unsigned>(m_pageTable.size() - 1)};
      ptUpdateLRU(lruIdx);
      return candidate;
    }
  }

  // All frames in use — evict LRU page table entry.
  int lruIdx = 0;
  unsigned maxLRU = 0;
  for (int i = 0; i < static_cast<int>(m_pageTable.size()); ++i) {
    if (m_pageTable[i].valid && m_pageTable[i].lru >= maxLRU) {
      maxLRU = m_pageTable[i].lru;
      lruIdx = i;
    }
  }
  const uint32_t reusedFrame = m_pageTable[lruIdx].pfn;
  const uint32_t evictedVPN = m_pageTable[lruIdx].vpn;
  for (auto &te : m_tlb) {
    if (te.valid && te.vpn == evictedVPN)
      te.valid = false;
  }
  m_pageTable[lruIdx] = {vpn, reusedFrame, true,
                         static_cast<unsigned>(m_pageTable.size() - 1)};
  ptUpdateLRU(lruIdx);
  return reusedFrame;
}

// ── Cycle cost ───────────────────────────────────────────────────────────────

int VMemSim::computeCycles(bool tlbHit, bool /*tlbMiss*/, bool pageFault) const {
  if (m_tlbEnabled) {
    if (tlbHit)
      return m_tlbCycles;
    // TLB miss: TLB access + page table walk + optional disk
    return m_tlbCycles + m_mainMemCycles + (pageFault ? m_diskCycles : 0);
  }
  // No TLB: page table walk + optional disk
  return m_mainMemCycles + (pageFault ? m_diskCycles : 0);
}

// ── Mean translation time ────────────────────────────────────────────────────

void VMemSim::updateMeanTranslationTime() {
  if (m_accessTrace.totalAccesses == 0) {
    m_accessTrace.meanTranslationTime = 0.0;
    return;
  }

  const double hr = m_accessTrace.tlbHitRate();

  if (m_tlbEnabled) {
    // Conditional page-fault rate among TLB misses
    double pfConditional = 0.0;
    if (m_accessTrace.tlbMisses > 0) {
      pfConditional = static_cast<double>(m_accessTrace.pageFaults) /
                      m_accessTrace.tlbMisses;
    }
    m_accessTrace.meanTranslationTime =
        hr * m_tlbCycles +
        (1.0 - hr) * (m_tlbCycles + m_mainMemCycles +
                      pfConditional * m_diskCycles);
  } else {
    const double pfr = m_accessTrace.pageFaultRate();
    m_accessTrace.meanTranslationTime =
        m_mainMemCycles + pfr * m_diskCycles;
  }
}

// ── Core access ──────────────────────────────────────────────────────────────

void VMemSim::access(AInt virtualAddress, MemoryAccess::Type type) {
  // Save snapshot for undo before mutating any state.
  VMemTrace trace;
  trace.tlbSnapshot = m_tlb;
  trace.ptSnapshot = m_pageTable;
  trace.traceSnapshot = m_accessTrace;
  trace.nextFrame = m_nextFrame;
  trace.transaction.virtualAddress = virtualAddress;

  const uint32_t vpn = extractVPN(virtualAddress);
  const uint32_t offset = extractOffset(virtualAddress);

  bool tlbHit = false;
  bool tlbMiss = false;
  bool pageFault = false;
  uint32_t pfn = 0;

  // Step 1 – TLB lookup (if enabled).
  if (m_tlbEnabled) {
    const int tlbIdx = tlbLookup(vpn);
    if (tlbIdx >= 0) {
      // TLB hit.
      tlbHit = true;
      pfn = m_tlb[tlbIdx].pfn;
      tlbUpdateLRU(tlbIdx);
    } else {
      tlbMiss = true;
    }
  }

  // Step 2 – Page table lookup (on TLB miss or when TLB is disabled).
  if (!tlbHit) {
    const int ptIdx = ptLookup(vpn);
    if (ptIdx >= 0) {
      // Page table hit.
      pfn = m_pageTable[ptIdx].pfn;
      ptUpdateLRU(ptIdx);
    } else {
      // Page fault — allocate a frame.
      pageFault = true;
      pfn = ptAllocate(vpn);
    }
    // Update TLB with the resolved mapping.
    if (m_tlbEnabled) {
      tlbInsert(vpn, pfn);
    }
  }

  // Step 3 – Build physical address.
  const AInt physAddr =
      (static_cast<AInt>(pfn) << m_pageOffsetBits) | offset;

  const int cost = computeCycles(tlbHit, tlbMiss, pageFault);

  // Step 4 – Record transaction.
  VMemTransaction tx;
  tx.virtualAddress = virtualAddress;
  tx.vpn = vpn;
  tx.pageOffset = offset;
  tx.pfn = pfn;
  tx.physicalAddress = physAddr;
  tx.tlbHit = tlbHit;
  tx.tlbMiss = tlbMiss;
  tx.pageFault = pageFault;
  tx.type = type;
  tx.cyclesCost = cost;

  trace.transaction = tx;

  // Update statistics.
  ++m_accessTrace.totalAccesses;
  if (m_tlbEnabled) {
    if (tlbHit)
      ++m_accessTrace.tlbHits;
    else {
      ++m_accessTrace.tlbMisses;
      if (pageFault)
        ++m_accessTrace.pageFaults;
      else
        ++m_accessTrace.pageTableHits;
    }
  } else {
    if (pageFault)
      ++m_accessTrace.pageFaults;
    else
      ++m_accessTrace.pageTableHits;
  }

  updateMeanTranslationTime();

  m_transactionHistory.push_back(tx);
  pushTrace(trace);

  if (!ProcessorHandler::isRunning()) {
    emit dataChanged(tx);
    emit statsChanged();
  }
}

// ── Reset ────────────────────────────────────────────────────────────────────

void VMemSim::reset() {
  m_tlb.assign(m_tlbEntries, TLBEntry{});
  m_pageTable.assign(1 << m_vpnBits, PageTableEntry{});
  m_accessTrace = VMemAccessTrace{};
  m_transactionHistory.clear();
  m_traceStack.clear();
  m_nextFrame = 0;

  emit statsChanged();
}

// ── Undo ─────────────────────────────────────────────────────────────────────

void VMemSim::undo() {
  if (m_traceStack.empty())
    return;

  const VMemTrace trace = popTrace();

  m_tlb = trace.tlbSnapshot;
  m_pageTable = trace.ptSnapshot;
  m_accessTrace = trace.traceSnapshot;
  m_nextFrame = trace.nextFrame;

  if (!m_transactionHistory.empty())
    m_transactionHistory.pop_back();

  emit statsChanged();

  if (!m_traceStack.empty()) {
    emit dataChanged(m_traceStack.front().transaction);
  } else {
    emit dataChanged(VMemTransaction{});
  }
}

// ── Trace stack ──────────────────────────────────────────────────────────────

void VMemSim::pushTrace(const VMemTrace &trace) {
  m_traceStack.push_front(trace);
  if (m_traceStack.size() >
      vsrtl::core::ClockedComponent::reverseStackSize()) {
    m_traceStack.pop_back();
  }
}

VMemSim::VMemTrace VMemSim::popTrace() {
  Q_ASSERT(!m_traceStack.empty());
  auto val = m_traceStack.front();
  m_traceStack.pop_front();
  return val;
}

// ── Last transaction ─────────────────────────────────────────────────────────

VMemSim::VMemTransaction VMemSim::getLastTransaction() const {
  if (m_transactionHistory.empty())
    return VMemTransaction{};
  return m_transactionHistory.back();
}

// ── Configuration ────────────────────────────────────────────────────────────

void VMemSim::updateConfiguration() {
  reset();
  emit configurationChanged();
}

void VMemSim::setPageOffsetBits(int bits) {
  m_pageOffsetBits = bits;
  updateConfiguration();
}

void VMemSim::setVpnBits(int bits) {
  m_vpnBits = bits;
  updateConfiguration();
}

void VMemSim::setPfnBits(int bits) {
  m_pfnBits = bits;
  updateConfiguration();
}

void VMemSim::setMainMemCycles(int cycles) {
  m_mainMemCycles = cycles;
  updateMeanTranslationTime();
  emit configurationChanged();
}

void VMemSim::setDiskCycles(int cycles) {
  m_diskCycles = cycles;
  updateMeanTranslationTime();
  emit configurationChanged();
}

void VMemSim::setTLBEnabled(bool enabled) {
  m_tlbEnabled = enabled;
  updateConfiguration();
}

void VMemSim::setTLBEntries(int n) {
  m_tlbEntries = n;
  updateConfiguration();
}

void VMemSim::setTLBCycles(int cycles) {
  m_tlbCycles = cycles;
  updateMeanTranslationTime();
  emit configurationChanged();
}

} // namespace Ripes
