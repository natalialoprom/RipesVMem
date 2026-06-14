#pragma once

#include <memory>

#include <QLabel>
#include <QWidget>

#include "vmemsim.h"

namespace Ripes {

class VMemStatsWidget : public QWidget {
  Q_OBJECT

public:
  explicit VMemStatsWidget(std::shared_ptr<VMemSim> vmemSim,
                           QWidget *parent = nullptr);

private slots:
  void statsChanged();
  void configurationChanged();

private:
  std::shared_ptr<VMemSim> m_vmemSim;

  QLabel *m_totalAccesses   = nullptr;
  QLabel *m_tlbHitRate      = nullptr;
  QLabel *m_tlbMissRate     = nullptr;
  QLabel *m_pageFaultRate   = nullptr;
  QLabel *m_meanTranslTime  = nullptr;

  // Rows that are shown/hidden based on TLB enabled
  QWidget *m_tlbHitRateRow  = nullptr;
  QWidget *m_tlbMissRateRow = nullptr;
};

} // namespace Ripes
