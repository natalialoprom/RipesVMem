#include "vmemshim.h"

#include "processorhandler.h"

namespace Ripes {

VMemShim::VMemShim(QObject *parent)
    : QObject(parent), m_vmemSim(std::make_shared<VMemSim>()) {
  connect(ProcessorHandler::get(), &ProcessorHandler::processorReset, this,
          &VMemShim::processorReset);
  connect(ProcessorHandler::get(), &ProcessorHandler::processorClocked, this,
          &VMemShim::processorWasClocked, Qt::DirectConnection);
  connect(ProcessorHandler::get(), &ProcessorHandler::processorReversed, this,
          &VMemShim::processorReversed);

  processorReset();
}

void VMemShim::processorReset() {
  m_vmemSim->reset();
}

void VMemShim::processorReversed() {
  m_vmemSim->undo();
}

void VMemShim::processorWasClocked() {
  const auto dataAccess = ProcessorHandler::getProcessor()->dataMemAccess();
  const auto instrAccess = ProcessorHandler::getProcessor()->instrMemAccess();

  if (dataAccess.type == MemoryAccess::Write) {
    m_vmemSim->access(dataAccess.address, MemoryAccess::Write);
  } else if (dataAccess.type == MemoryAccess::Read) {
    m_vmemSim->access(dataAccess.address, MemoryAccess::Read);
  }

  if (instrAccess.type == MemoryAccess::Read) {
    m_vmemSim->access(instrAccess.address, MemoryAccess::Read);
  }
}

} // namespace Ripes
