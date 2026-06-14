#include "vmemtransactionwidget.h"

#include <QFormLayout>
#include <QGroupBox>
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

VMemTransactionWidget::VMemTransactionWidget(std::shared_ptr<VMemSim> vmemSim,
                                             QWidget *parent)
    : QWidget(parent), m_vmemSim(std::move(vmemSim)) {

  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  auto *group  = new QGroupBox("Last address translation:");
  auto *form   = new QFormLayout(group);
  form->setLabelAlignment(Qt::AlignRight);

  const auto addRow = [&](const QString &label, QLabel *&field) {
    field = new QLabel;
    field->setFont(QFont("Monospace"));
    form->addRow(label, field);
  };

  addRow("Virtual address:",  m_virtualAddr);
  addRow("VPN:",              m_vpn);
  addRow("Page offset:",      m_pageOffset);
  addRow("TLB result:",       m_tlbResult);
  addRow("PT result:",        m_ptResult);
  addRow("PFN:",              m_pfn);
  addRow("Physical address:", m_physicalAddr);
  addRow("Translation cost:", m_translCost);
  addRow("Access type:",      m_accessType);

  outerLayout->addWidget(group);
  outerLayout->addStretch();

  connect(m_vmemSim.get(), &VMemSim::dataChanged,
          this, &VMemTransactionWidget::dataChanged);

  clearFields();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void VMemTransactionWidget::dataChanged(VMemSim::VMemTransaction t) {
  const int offsetBits = m_vmemSim->getPageOffsetBits();
  const int vpnBits    = m_vmemSim->getVpnBits();
  const int pfnBits    = m_vmemSim->getPfnBits();

  m_virtualAddr->setText(QString("0x%1").arg(t.virtualAddress, 0, 16));
  m_vpn->setText(bitField(t.vpn, vpnBits));
  m_pageOffset->setText(bitField(t.pageOffset, offsetBits));

  if (!m_vmemSim->getTLBEnabled()) {
    m_tlbResult->setText("N/A");
  } else {
    m_tlbResult->setText(t.tlbHit ? "HIT" : "MISS");
  }

  if (t.pageFault) {
    m_ptResult->setText("PAGE FAULT");
  } else {
    m_ptResult->setText("HIT");
  }

  m_pfn->setText(bitField(t.pfn, pfnBits));
  m_physicalAddr->setText(QString("0x%1").arg(t.physicalAddress, 0, 16));
  m_translCost->setText(QString("%1 cycles").arg(t.cyclesCost));

  switch (t.type) {
  case MemoryAccess::Read:
    m_accessType->setText("Read");
    break;
  case MemoryAccess::Write:
    m_accessType->setText("Write");
    break;
  default:
    m_accessType->setText("—");
    break;
  }
}

void VMemTransactionWidget::clearFields() {
  const QString dash = "—";
  m_virtualAddr->setText(dash);
  m_vpn->setText(dash);
  m_pageOffset->setText(dash);
  m_tlbResult->setText(dash);
  m_ptResult->setText(dash);
  m_pfn->setText(dash);
  m_physicalAddr->setText(dash);
  m_translCost->setText(dash);
  m_accessType->setText(dash);
}

} // namespace Ripes
