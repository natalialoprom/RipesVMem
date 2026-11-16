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

  struct TxFields {
    QLabel *virtualAddr  = nullptr;
    QLabel *vpn          = nullptr;
    QLabel *pageOffset   = nullptr;
    QLabel *tlbResult    = nullptr;
    QLabel *ptResult     = nullptr;
    QLabel *pfn          = nullptr;
    QLabel *physicalAddr = nullptr;
    QLabel *translCost   = nullptr;
    QLabel *accessType   = nullptr;
  };

private slots:
  void onCycleChanged(VMemSim::VMemCyclePair pair);

private:
  void populateFields(TxFields &f, const VMemSim::VMemTransaction &t);
  void clearFields(TxFields &f);

  std::shared_ptr<VMemSim> m_vmemSim;

  TxFields m_dataFields;
  TxFields m_instrFields;
};

} // namespace Ripes
