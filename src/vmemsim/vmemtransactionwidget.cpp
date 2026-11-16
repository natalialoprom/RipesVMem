#include "vmemtransactionwidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Ripes {

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString toBinary(uint32_t value, int bits) {
  QString s;
  s.reserve(bits);
  for (int i = bits - 1; i >= 0; --i)
    s += ((value >> i) & 1) ? '1' : '0';
  return s;
}

static QString bitField(uint32_t value, int bits) {
  return QString("%1 (0x%2)")
      .arg(toBinary(value, bits))
      .arg(value, 0, 16);
}

// ── Construction ──────────────────────────────────────────────────────────────

static QGroupBox *makePanel(const QString &title,
                             VMemTransactionWidget::TxFields &f) {
  // TxFields is not a nested class — but we're in the .cpp so it's fine.
  auto *group = new QGroupBox(title);
  auto *form  = new QFormLayout(group);
  form->setLabelAlignment(Qt::AlignRight);

  const auto addRow = [&](const QString &label, QLabel *&field) {
    field = new QLabel;
    field->setFont(QFont("Monospace"));
    form->addRow(label, field);
  };

  addRow("Virtual address:",  f.virtualAddr);
  addRow("VPN:",              f.vpn);
  addRow("Page offset:",      f.pageOffset);
  addRow("TLB result:",       f.tlbResult);
  addRow("PT result:",        f.ptResult);
  addRow("PFN:",              f.pfn);
  addRow("Physical address:", f.physicalAddr);
  addRow("Translation cost:", f.translCost);
  addRow("Access type:",      f.accessType);

  return group;
}

VMemTransactionWidget::VMemTransactionWidget(std::shared_ptr<VMemSim> vmemSim,
                                             QWidget *parent)
    : QWidget(parent), m_vmemSim(std::move(vmemSim)) {

  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  auto *row = new QHBoxLayout;
  row->addWidget(makePanel("Data access:", m_dataFields));
  row->addWidget(makePanel("Instruction access:", m_instrFields));
  outerLayout->addLayout(row);
  outerLayout->addStretch();

  connect(m_vmemSim.get(), &VMemSim::cycleChanged,
          this, &VMemTransactionWidget::onCycleChanged);

  clearFields(m_dataFields);
  clearFields(m_instrFields);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void VMemTransactionWidget::onCycleChanged(VMemSim::VMemCyclePair pair) {
  if (pair.hasData)
    populateFields(m_dataFields, pair.data);
  else
    clearFields(m_dataFields);

  if (pair.hasInstr)
    populateFields(m_instrFields, pair.instr);
  else
    clearFields(m_instrFields);
}

void VMemTransactionWidget::populateFields(TxFields &f,
                                           const VMemSim::VMemTransaction &t) {
  const int offsetBits = m_vmemSim->getPageOffsetBits();
  const int vpnBits    = m_vmemSim->getVpnBits();
  const int pfnBits    = m_vmemSim->getPfnBits();

  f.virtualAddr->setText(QString("0x%1").arg(t.virtualAddress, 0, 16));
  f.vpn->setText(bitField(t.vpn, vpnBits));
  f.pageOffset->setText(bitField(t.pageOffset, offsetBits));

  if (!m_vmemSim->getTLBEnabled()) {
    f.tlbResult->setText("N/A");
  } else {
    f.tlbResult->setText(t.tlbHit ? "HIT" : "MISS");
  }

  f.ptResult->setText(t.pageFault ? "PAGE FAULT" : "HIT");
  f.pfn->setText(bitField(t.pfn, pfnBits));
  f.physicalAddr->setText(QString("0x%1").arg(t.physicalAddress, 0, 16));
  f.translCost->setText(QString("%1 cycles").arg(t.cyclesCost));

  switch (t.type) {
  case MemoryAccess::Read:  f.accessType->setText("Read");  break;
  case MemoryAccess::Write: f.accessType->setText("Write"); break;
  default:                  f.accessType->setText("—");     break;
  }
}

void VMemTransactionWidget::clearFields(TxFields &f) {
  const QString dash = "—";
  f.virtualAddr->setText(dash);
  f.vpn->setText(dash);
  f.pageOffset->setText(dash);
  f.tlbResult->setText(dash);
  f.ptResult->setText(dash);
  f.pfn->setText(dash);
  f.physicalAddr->setText(dash);
  f.translCost->setText(dash);
  f.accessType->setText(dash);
}

} // namespace Ripes
