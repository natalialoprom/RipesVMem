#include "vmemtablewidget.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QVBoxLayout>

namespace Ripes {

// ── Colours ───────────────────────────────────────────────────────────────────
static const QColor CLR_VALID   {144, 238, 144};   // light green
static const QColor CLR_INVALID {211, 211, 211};   // light grey
static const QColor CLR_HIT     {255, 255, 100};   // yellow

// ── Construction ──────────────────────────────────────────────────────────────

VMemTableWidget::VMemTableWidget(std::shared_ptr<VMemSim> vmemSim,
                                 QWidget *parent)
    : QWidget(parent), m_vmemSim(std::move(vmemSim)) {

  auto *outerLayout = new QHBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  // TLB container (shown / hidden based on TLB enabled)
  m_tlbContainer    = new QWidget;
  auto *tlbLayout   = new QVBoxLayout(m_tlbContainer);
  tlbLayout->setContentsMargins(0, 0, 0, 0);
  auto *tlbBox      = new QGroupBox("TLB");
  auto *tlbBoxLayout = new QVBoxLayout(tlbBox);
  m_tlbTable        = new QTableWidget;
  m_tlbTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tlbTable->setSelectionMode(QAbstractItemView::NoSelection);
  m_tlbTable->setColumnCount(5);
  m_tlbTable->setHorizontalHeaderLabels({"Index", "Valid", "VPN", "PFN", "LRU"});
  m_tlbTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_tlbTable->verticalHeader()->setVisible(false);
  tlbBoxLayout->addWidget(m_tlbTable);
  tlbLayout->addWidget(tlbBox);
  outerLayout->addWidget(m_tlbContainer);

  // Page table
  auto *ptBox       = new QGroupBox("Page Table");
  auto *ptBoxLayout = new QVBoxLayout(ptBox);
  m_ptTable         = new QTableWidget;
  m_ptTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_ptTable->setSelectionMode(QAbstractItemView::NoSelection);
  m_ptTable->setColumnCount(5);
  m_ptTable->setHorizontalHeaderLabels({"Index", "Valid", "VPN", "PFN", "LRU"});
  m_ptTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_ptTable->verticalHeader()->setVisible(false);
  ptBoxLayout->addWidget(m_ptTable);
  outerLayout->addWidget(ptBox);

  connect(m_vmemSim.get(), &VMemSim::dataChanged,
          this, &VMemTableWidget::dataChanged);
  connect(m_vmemSim.get(), &VMemSim::configurationChanged,
          this, &VMemTableWidget::configurationChanged);

  configurationChanged();
}

// ── Rebuild (called when dimensions change) ───────────────────────────────────

void VMemTableWidget::rebuildTables() {
  m_tlbTable->clearContents();
  m_tlbTable->setRowCount(m_vmemSim->getTLBEntries());

  m_ptTable->clearContents();
  m_ptTable->setRowCount(m_vmemSim->getPageCount());

  populateTLBTable();
  populatePageTable();
}

// ── Population helpers ────────────────────────────────────────────────────────

static QTableWidgetItem *makeItem(const QString &text) {
  auto *item = new QTableWidgetItem(text);
  item->setTextAlignment(Qt::AlignCenter);
  return item;
}

static QString lruStr(unsigned lru) {
  return lru == static_cast<unsigned>(-1) ? "—" : QString::number(lru);
}

void VMemTableWidget::populateTLBTable(int highlightRow) {
  const auto &entries = m_vmemSim->getTLBState();
  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    const auto &e = entries[i];
    m_tlbTable->setItem(i, 0, makeItem(QString::number(i)));
    m_tlbTable->setItem(i, 1, makeItem(e.valid ? "1" : "0"));
    m_tlbTable->setItem(i, 2, makeItem(QString("0x%1").arg(e.vpn, 0, 16)));
    m_tlbTable->setItem(i, 3, makeItem(QString("0x%1").arg(e.pfn, 0, 16)));
    m_tlbTable->setItem(i, 4, makeItem(lruStr(e.lru)));

    const QColor &color = (i == highlightRow) ? CLR_HIT
                          : e.valid            ? CLR_VALID
                                               : CLR_INVALID;
    setRowColor(m_tlbTable, i, color);
  }
}

void VMemTableWidget::populatePageTable(int highlightRow) {
  const auto &entries = m_vmemSim->getPageTableState();
  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    const auto &e = entries[i];
    m_ptTable->setItem(i, 0, makeItem(QString::number(i)));
    m_ptTable->setItem(i, 1, makeItem(e.valid ? "1" : "0"));
    m_ptTable->setItem(i, 2, makeItem(QString("0x%1").arg(e.vpn, 0, 16)));
    m_ptTable->setItem(i, 3, makeItem(QString("0x%1").arg(e.pfn, 0, 16)));
    m_ptTable->setItem(i, 4, makeItem(lruStr(e.lru)));

    const QColor &color = (i == highlightRow) ? CLR_HIT
                          : e.valid            ? CLR_VALID
                                               : CLR_INVALID;
    setRowColor(m_ptTable, i, color);
  }
}

void VMemTableWidget::setRowColor(QTableWidget *table, int row,
                                  const QColor &color) {
  for (int col = 0; col < table->columnCount(); ++col) {
    if (auto *item = table->item(row, col))
      item->setBackground(color);
  }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void VMemTableWidget::configurationChanged() {
  m_tlbContainer->setVisible(m_vmemSim->getTLBEnabled());
  rebuildTables();
}

void VMemTableWidget::dataChanged(VMemSim::VMemTransaction transaction) {
  // Find which TLB row holds this VPN (if TLB enabled and we had a hit)
  int tlbHighlight = -1;
  if (m_vmemSim->getTLBEnabled()) {
    const auto &tlb = m_vmemSim->getTLBState();
    for (int i = 0; i < static_cast<int>(tlb.size()); ++i) {
      if (tlb[i].valid && tlb[i].vpn == transaction.vpn) {
        tlbHighlight = i;
        break;
      }
    }
  }

  // Find which page table row holds this VPN
  int ptHighlight = -1;
  const auto &pt = m_vmemSim->getPageTableState();
  for (int i = 0; i < static_cast<int>(pt.size()); ++i) {
    if (pt[i].valid && pt[i].vpn == transaction.vpn) {
      ptHighlight = i;
      break;
    }
  }

  populateTLBTable(tlbHighlight);
  populatePageTable(ptHighlight);

  // Newly created entries (page fault → new PT entry; TLB miss → new TLB
  // entry) flash yellow for 600 ms then settle to green, distinguishing
  // allocation from a regular hit which stays yellow indefinitely.
  if (transaction.pageFault && ptHighlight >= 0) {
    QTimer::singleShot(600, this, [this, ptHighlight] {
      if (ptHighlight < m_ptTable->rowCount())
        setRowColor(m_ptTable, ptHighlight, CLR_VALID);
    });
  }
  if (m_vmemSim->getTLBEnabled() && transaction.tlbMiss && tlbHighlight >= 0) {
    QTimer::singleShot(600, this, [this, tlbHighlight] {
      if (tlbHighlight < m_tlbTable->rowCount())
        setRowColor(m_tlbTable, tlbHighlight, CLR_VALID);
    });
  }
}

} // namespace Ripes
