#pragma once

#include <memory>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include "vmemsim.h"

namespace Ripes {

class VMemConfigWidget : public QWidget {
  Q_OBJECT

public:
  explicit VMemConfigWidget(std::shared_ptr<VMemSim> vmemSim,
                            QWidget *parent = nullptr);

signals:
  void configurationChanged();

private slots:
  void handleConfigurationChanged();

private:
  void setupLayout();
  void connectSignals();
  void updateDerivedLabels();
  void updateAddressStructure();

  std::shared_ptr<VMemSim> m_vmemSim;

  // Memory layout parameters
  QSpinBox *m_pageOffsetBits = nullptr;
  QSpinBox *m_vpnBits        = nullptr;
  QSpinBox *m_pfnBits        = nullptr;
  QSpinBox *m_mainMemCycles  = nullptr;
  QSpinBox *m_diskCycles     = nullptr;

  // Derived-value labels
  QLabel *m_pageSizeLabel  = nullptr;
  QLabel *m_pageCountLabel = nullptr;
  QLabel *m_frameCountLabel = nullptr;

  // TLB controls
  QCheckBox *m_tlbEnabled   = nullptr;
  QGroupBox *m_tlbGroup     = nullptr;
  QSpinBox  *m_tlbEntries   = nullptr;
  QSpinBox  *m_tlbCycles    = nullptr;

  // Address structure panel
  QLabel *m_vaStructLabel  = nullptr;
  QLabel *m_paStructLabel  = nullptr;

  QPushButton *m_resetButton = nullptr;
};

} // namespace Ripes
