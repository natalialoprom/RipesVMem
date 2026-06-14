#pragma once

#include <deque>
#include <vector>

#include <QObject>

#include "VSRTL/core/vsrtl_register.h"
#include "isa/isa_types.h"
#include "processors/interface/ripesprocessor.h"

namespace Ripes {

class VMemSim : public QObject {
  Q_OBJECT

public:
  // ── Data structures ────────────────────────────────────────────────────────

  struct TLBEntry {
    uint32_t vpn = 0;
    uint32_t pfn = 0;
    bool valid = false;
    unsigned lru = static_cast<unsigned>(-1);
  };

  struct PageTableEntry {
    uint32_t vpn = 0;
    uint32_t pfn = 0;
    bool valid = false;
    unsigned lru = static_cast<unsigned>(-1);
  };

  struct VMemTransaction {
    AInt virtualAddress = 0;
    uint32_t vpn = 0;
    uint32_t pageOffset = 0;
    uint32_t pfn = 0;
    AInt physicalAddress = 0;
    bool tlbHit = false;
    bool tlbMiss = false;
    bool pageFault = false;
    MemoryAccess::Type type = MemoryAccess::None;
    int cyclesCost = 0;
  };

  struct VMemAccessTrace {
    int totalAccesses = 0;
    int tlbHits = 0;
    int tlbMisses = 0;
    int pageFaults = 0;
    int pageTableHits = 0;

    double tlbHitRate() const {
      if (totalAccesses == 0)
        return 0.0;
      return static_cast<double>(tlbHits) / totalAccesses;
    }

    double pageFaultRate() const {
      if (totalAccesses == 0)
        return 0.0;
      return static_cast<double>(pageFaults) / totalAccesses;
    }

    // Computed after construction — needs sim parameters, so defined in .cpp
    // as a free function; kept here as a method that is filled by VMemSim.
    double meanTranslationTime = 0.0;
  };

  // ── Construction ───────────────────────────────────────────────────────────

  explicit VMemSim(QObject *parent = nullptr);

  // ── Core interface ─────────────────────────────────────────────────────────

  void access(AInt virtualAddress, MemoryAccess::Type type);
  void reset();
  void undo();

  // ── Accessors ──────────────────────────────────────────────────────────────

  const VMemAccessTrace &getAccessTrace() const { return m_accessTrace; }
  const std::vector<VMemTransaction> &getTransactionHistory() const {
    return m_transactionHistory;
  }
  const std::vector<TLBEntry> &getTLBState() const { return m_tlb; }
  const std::vector<PageTableEntry> &getPageTableState() const {
    return m_pageTable;
  }
  VMemTransaction getLastTransaction() const;

  // ── Parameter getters ──────────────────────────────────────────────────────

  int getPageOffsetBits() const { return m_pageOffsetBits; }
  int getVpnBits() const { return m_vpnBits; }
  int getPfnBits() const { return m_pfnBits; }
  int getMainMemCycles() const { return m_mainMemCycles; }
  int getDiskCycles() const { return m_diskCycles; }
  bool getTLBEnabled() const { return m_tlbEnabled; }
  int getTLBEntries() const { return m_tlbEntries; }
  int getTLBCycles() const { return m_tlbCycles; }

  int getPageCount() const { return 1 << m_vpnBits; }
  int getFrameCount() const { return 1 << m_pfnBits; }

public slots:
  void setPageOffsetBits(int bits);
  void setVpnBits(int bits);
  void setPfnBits(int bits);
  void setMainMemCycles(int cycles);
  void setDiskCycles(int cycles);
  void setTLBEnabled(bool enabled);
  void setTLBEntries(int n);
  void setTLBCycles(int cycles);

signals:
  void configurationChanged();
  void dataChanged(VMemSim::VMemTransaction transaction);
  void statsChanged();

private:
  // ── Configuration ──────────────────────────────────────────────────────────

  int m_pageOffsetBits = 4;   // 16-byte pages
  int m_vpnBits = 4;          // 16 virtual pages
  int m_pfnBits = 3;          // 8 physical frames
  int m_mainMemCycles = 20;
  int m_diskCycles = 2000;
  bool m_tlbEnabled = true;
  int m_tlbEntries = 4;
  int m_tlbCycles = 1;

  // ── State ──────────────────────────────────────────────────────────────────

  std::vector<TLBEntry> m_tlb;
  std::vector<PageTableEntry> m_pageTable;
  VMemAccessTrace m_accessTrace;
  std::vector<VMemTransaction> m_transactionHistory;

  // Next physical frame to allocate (round-robin)
  uint32_t m_nextFrame = 0;

  // ── Undo support ───────────────────────────────────────────────────────────

  struct VMemTrace {
    VMemTransaction transaction;
    // Snapshots of structures BEFORE the access so we can roll back.
    std::vector<TLBEntry> tlbSnapshot;
    std::vector<PageTableEntry> ptSnapshot;
    VMemAccessTrace traceSnapshot;
    uint32_t nextFrame = 0; // m_nextFrame before the access
  };

  std::deque<VMemTrace> m_traceStack;

  // ── Helpers ────────────────────────────────────────────────────────────────

  uint32_t extractVPN(AInt va) const;
  uint32_t extractOffset(AInt va) const;

  // TLB operations
  int tlbLookup(uint32_t vpn) const;           // returns index or -1
  void tlbInsert(uint32_t vpn, uint32_t pfn);  // LRU eviction
  void tlbUpdateLRU(int idx);

  // Page table operations
  int ptLookup(uint32_t vpn) const;            // returns index or -1
  uint32_t ptAllocate(uint32_t vpn);           // evicts LRU if full, returns pfn
  void ptUpdateLRU(int idx);

  int computeCycles(bool tlbHit, bool tlbMiss, bool pageFault) const;
  void updateMeanTranslationTime();

  void updateConfiguration();
  void pushTrace(const VMemTrace &trace);
  VMemTrace popTrace();
};

} // namespace Ripes

Q_DECLARE_METATYPE(Ripes::VMemSim::VMemTransaction)
