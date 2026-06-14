#pragma once

#include <memory>

#include <QLabel>
#include <QWidget>

#include "vmemsim.h"

namespace Ripes {

class VMemTransactionWidget : public QWidget {
  Q_OBJECT

public:
  explicit VMemTransactionWidget(std::shared_ptr<VMemSim> vmemSim,
                                 QWidget *parent = nullptr);

private slots:
  void dataChanged(VMemSim::VMemTransaction transaction);

private:
  void clearFields();

  std::shared_ptr<VMemSim> m_vmemSim;

  QLabel *m_virtualAddr    = nullptr;
  QLabel *m_vpn            = nullptr;
  QLabel *m_pageOffset     = nullptr;
  QLabel *m_tlbResult      = nullptr;
  QLabel *m_ptResult       = nullptr;
  QLabel *m_pfn            = nullptr;
  QLabel *m_physicalAddr   = nullptr;
  QLabel *m_translCost     = nullptr;
  QLabel *m_accessType     = nullptr;
};

} // namespace Ripes
