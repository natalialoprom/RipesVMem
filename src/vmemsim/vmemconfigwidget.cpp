#include "vmemconfigwidget.h"

#include <cmath>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Ripes {

VMemConfigWidget::VMemConfigWidget(std::shared_ptr<VMemSim> vmemSim,
                                   QWidget *parent)
    : QWidget(parent), m_vmemSim(std::move(vmemSim)) {
  setupLayout();
  connectSignals();
  handleConfigurationChanged();
}

// ── Layout ────────────────────────────────────────────────────────────────────

void VMemConfigWidget::setupLayout() {
  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  // ── Main configuration group ──────────────────────────────────────────────
  auto *cfgGroup  = new QGroupBox("Virtual memory configuration:");
  auto *cfgGrid   = new QGridLayout(cfgGroup);
  int row = 0;

  // Page offset bits
  m_pageOffsetBits = new QSpinBox;
  m_pageOffsetBits->setRange(1, 16);
  m_pageOffsetBits->setValue(m_vmemSim->getPageOffsetBits());
  m_pageSizeLabel = new QLabel;
  cfgGrid->addWidget(new QLabel("Page offset bits:"),  row, 0);
  cfgGrid->addWidget(m_pageOffsetBits,                 row, 1);
  cfgGrid->addWidget(m_pageSizeLabel,                  row, 2);
  ++row;

  // VPN bits
  m_vpnBits = new QSpinBox;
  m_vpnBits->setRange(1, 16);
  m_vpnBits->setValue(m_vmemSim->getVpnBits());
  m_pageCountLabel = new QLabel;
  cfgGrid->addWidget(new QLabel("VPN bits:"),   row, 0);
  cfgGrid->addWidget(m_vpnBits,                 row, 1);
  cfgGrid->addWidget(m_pageCountLabel,          row, 2);
  ++row;

  // PFN bits
  m_pfnBits = new QSpinBox;
  m_pfnBits->setRange(1, 16);
  m_pfnBits->setValue(m_vmemSim->getPfnBits());
  m_frameCountLabel = new QLabel;
  cfgGrid->addWidget(new QLabel("PFN bits:"),    row, 0);
  cfgGrid->addWidget(m_pfnBits,                  row, 1);
  cfgGrid->addWidget(m_frameCountLabel,          row, 2);
  ++row;

  // Main memory / page-table access time
  m_mainMemCycles = new QSpinBox;
  m_mainMemCycles->setRange(1, 100000);
  m_mainMemCycles->setValue(m_vmemSim->getMainMemCycles());
  cfgGrid->addWidget(new QLabel("Main memory / PT access time (cycles):"), row, 0);
  cfgGrid->addWidget(m_mainMemCycles, row, 1);
  ++row;

  // Page-fault penalty
  m_diskCycles = new QSpinBox;
  m_diskCycles->setRange(1, 1000000);
  m_diskCycles->setValue(m_vmemSim->getDiskCycles());
  cfgGrid->addWidget(new QLabel("Page fault penalty (cycles):"), row, 0);
  cfgGrid->addWidget(m_diskCycles, row, 1);
  ++row;

  // TLB enable checkbox
  m_tlbEnabled = new QCheckBox("Enable TLB");
  m_tlbEnabled->setChecked(m_vmemSim->getTLBEnabled());
  cfgGrid->addWidget(m_tlbEnabled, row, 0, 1, 3);
  ++row;

  // ── TLB sub-group ─────────────────────────────────────────────────────────
  m_tlbGroup        = new QGroupBox("TLB configuration:");
  auto *tlbGrid     = new QGridLayout(m_tlbGroup);

  m_tlbEntries = new QSpinBox;
  m_tlbEntries->setRange(1, 256);
  m_tlbEntries->setValue(m_vmemSim->getTLBEntries());
  tlbGrid->addWidget(new QLabel("TLB entries:"), 0, 0);
  tlbGrid->addWidget(m_tlbEntries,               0, 1);

  m_tlbCycles = new QSpinBox;
  m_tlbCycles->setRange(1, 100);
  m_tlbCycles->setValue(m_vmemSim->getTLBCycles());
  tlbGrid->addWidget(new QLabel("TLB access time (cycles):"), 1, 0);
  tlbGrid->addWidget(m_tlbCycles,                             1, 1);

  cfgGrid->addWidget(m_tlbGroup, row, 0, 1, 3);
  ++row;

  outerLayout->addWidget(cfgGroup);

  // ── Address structure panel ───────────────────────────────────────────────
  auto *addrGroup  = new QGroupBox("Address structure:");
  auto *addrLayout = new QVBoxLayout(addrGroup);

  m_vaStructLabel = new QLabel;
  m_paStructLabel = new QLabel;
  m_vaStructLabel->setFont(QFont("Monospace"));
  m_paStructLabel->setFont(QFont("Monospace"));
  addrLayout->addWidget(m_vaStructLabel);
  addrLayout->addWidget(m_paStructLabel);

  outerLayout->addWidget(addrGroup);

  // ── Reset button ──────────────────────────────────────────────────────────
  m_resetButton = new QPushButton("Reset");
  outerLayout->addWidget(m_resetButton);

  outerLayout->addStretch();
}

// ── Signal wiring ─────────────────────────────────────────────────────────────

void VMemConfigWidget::connectSignals() {
  // Spinbox → VMemSim setters
  connect(m_pageOffsetBits, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setPageOffsetBits);
  connect(m_vpnBits, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setVpnBits);
  connect(m_pfnBits, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setPfnBits);
  connect(m_mainMemCycles, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setMainMemCycles);
  connect(m_diskCycles, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setDiskCycles);
  connect(m_tlbEnabled, &QCheckBox::toggled,
          m_vmemSim.get(), &VMemSim::setTLBEnabled);
  connect(m_tlbEntries, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setTLBEntries);
  connect(m_tlbCycles, QOverload<int>::of(&QSpinBox::valueChanged),
          m_vmemSim.get(), &VMemSim::setTLBCycles);

  // TLB checkbox enables/disables TLB sub-group
  connect(m_tlbEnabled, &QCheckBox::toggled,
          m_tlbGroup, &QGroupBox::setEnabled);

  // VMemSim notifies us when configuration changes (e.g. from external setters)
  connect(m_vmemSim.get(), &VMemSim::configurationChanged,
          this, &VMemConfigWidget::handleConfigurationChanged);
  connect(m_vmemSim.get(), &VMemSim::configurationChanged,
          this, [this] { emit configurationChanged(); });

  // Reset button
  connect(m_resetButton, &QPushButton::clicked,
          m_vmemSim.get(), &VMemSim::reset);
}

// ── Sync helpers ──────────────────────────────────────────────────────────────

void VMemConfigWidget::handleConfigurationChanged() {
  // Block signals so we don't re-enter VMemSim setters while syncing the UI
  const auto widgets = {
      static_cast<QWidget *>(m_pageOffsetBits),
      static_cast<QWidget *>(m_vpnBits),
      static_cast<QWidget *>(m_pfnBits),
      static_cast<QWidget *>(m_mainMemCycles),
      static_cast<QWidget *>(m_diskCycles),
      static_cast<QWidget *>(m_tlbEnabled),
      static_cast<QWidget *>(m_tlbEntries),
      static_cast<QWidget *>(m_tlbCycles),
  };
  for (auto *w : widgets)
    w->blockSignals(true);

  m_pageOffsetBits->setValue(m_vmemSim->getPageOffsetBits());
  m_vpnBits->setValue(m_vmemSim->getVpnBits());
  m_pfnBits->setValue(m_vmemSim->getPfnBits());
  m_mainMemCycles->setValue(m_vmemSim->getMainMemCycles());
  m_diskCycles->setValue(m_vmemSim->getDiskCycles());
  m_tlbEnabled->setChecked(m_vmemSim->getTLBEnabled());
  m_tlbEntries->setValue(m_vmemSim->getTLBEntries());
  m_tlbCycles->setValue(m_vmemSim->getTLBCycles());
  m_tlbGroup->setEnabled(m_vmemSim->getTLBEnabled());

  for (auto *w : widgets)
    w->blockSignals(false);

  updateDerivedLabels();
  updateAddressStructure();
}

void VMemConfigWidget::updateDerivedLabels() {
  const int offsetBits = m_vmemSim->getPageOffsetBits();
  const int vpnBits    = m_vmemSim->getVpnBits();
  const int pfnBits    = m_vmemSim->getPfnBits();

  const long pageSize   = 1L << offsetBits;
  const long pageCount  = 1L << vpnBits;
  const long frameCount = 1L << pfnBits;

  m_pageSizeLabel->setText(QString("Page size: %1 bytes").arg(pageSize));
  m_pageCountLabel->setText(QString("Pages: %1").arg(pageCount));
  m_frameCountLabel->setText(QString("Frames: %1").arg(frameCount));
}

void VMemConfigWidget::updateAddressStructure() {
  const int offsetBits = m_vmemSim->getPageOffsetBits();
  const int vpnBits    = m_vmemSim->getVpnBits();
  const int pfnBits    = m_vmemSim->getPfnBits();

  m_vaStructLabel->setText(
      QString("Virtual:  [ VPN: %1b | Offset: %2b ]")
          .arg(vpnBits)
          .arg(offsetBits));
  m_paStructLabel->setText(
      QString("Physical: [ PFN: %1b | Offset: %2b ]")
          .arg(pfnBits)
          .arg(offsetBits));
}

} // namespace Ripes
