#include "vmemstatswidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Ripes {

// ── Small helper: a form row widget holding label + value ─────────────────────

static QWidget *makeFormRow(const QString &labelText, QLabel *&valueLabel,
                            QFormLayout *form) {
  valueLabel = new QLabel("—");
  // Wrap in a QWidget so we can show/hide the entire row cleanly
  auto *row    = new QWidget;
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(new QLabel(labelText));
  layout->addStretch();
  layout->addWidget(valueLabel);
  form->addRow(row);
  return row;
}

// ── Construction ──────────────────────────────────────────────────────────────

VMemStatsWidget::VMemStatsWidget(std::shared_ptr<VMemSim> vmemSim,
                                 QWidget *parent)
    : QWidget(parent), m_vmemSim(std::move(vmemSim)) {

  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  auto *group = new QGroupBox("Virtual memory statistics:");
  auto *form  = new QFormLayout(group);
  form->setLabelAlignment(Qt::AlignLeft);

  makeFormRow("Total accesses:",        m_totalAccesses,  form);
  m_tlbHitRateRow  = makeFormRow("TLB hit rate:",  m_tlbHitRate,   form);
  m_tlbMissRateRow = makeFormRow("TLB miss rate:", m_tlbMissRate,  form);
  makeFormRow("Page fault rate:",       m_pageFaultRate,  form);
  makeFormRow("Mean translation time:", m_meanTranslTime, form);

  outerLayout->addWidget(group);
  outerLayout->addStretch();

  connect(m_vmemSim.get(), &VMemSim::statsChanged,
          this, &VMemStatsWidget::statsChanged);
  connect(m_vmemSim.get(), &VMemSim::configurationChanged,
          this, &VMemStatsWidget::configurationChanged);

  configurationChanged();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void VMemStatsWidget::statsChanged() {
  const auto &trace = m_vmemSim->getAccessTrace();

  m_totalAccesses->setText(QString::number(trace.totalAccesses));

  if (m_vmemSim->getTLBEnabled()) {
    m_tlbHitRate->setText(
        QString("%1%").arg(trace.tlbHitRate() * 100.0, 0, 'f', 2));
    const double missRate = trace.totalAccesses > 0
        ? static_cast<double>(trace.tlbMisses) / trace.totalAccesses
        : 0.0;
    m_tlbMissRate->setText(
        QString("%1%").arg(missRate * 100.0, 0, 'f', 2));
  }

  m_pageFaultRate->setText(
      QString("%1%").arg(trace.pageFaultRate() * 100.0, 0, 'f', 2));
  m_meanTranslTime->setText(
      QString("%1 cycles").arg(trace.meanTranslationTime, 0, 'f', 2));
}

void VMemStatsWidget::configurationChanged() {
  const bool tlbOn = m_vmemSim->getTLBEnabled();
  m_tlbHitRateRow->setVisible(tlbOn);
  m_tlbMissRateRow->setVisible(tlbOn);
  // Refresh displayed numbers after a config change resets the sim stats
  statsChanged();
}

} // namespace Ripes
