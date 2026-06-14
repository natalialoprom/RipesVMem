#pragma once

#include <memory>

#include <QObject>

#include "vmemsim.h"

namespace Ripes {

class VMemShim : public QObject {
  Q_OBJECT

public:
  explicit VMemShim(QObject *parent = nullptr);

  std::shared_ptr<VMemSim> getVMemSim() const { return m_vmemSim; }

private:
  void processorWasClocked();
  void processorReset();
  void processorReversed();

  std::shared_ptr<VMemSim> m_vmemSim;
};

} // namespace Ripes
