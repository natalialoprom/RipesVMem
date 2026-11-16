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

  VMemSim::VMemTransaction dataTx, instrTx;
  bool hasData = false, hasInstr = false;

  if (dataAccess.type == MemoryAccess::Write) {
    m_vmemSim->access(dataAccess.address, MemoryAccess::Write);
    dataTx = m_vmemSim->getLastTransaction();
    hasData = true;
  } else if (dataAccess.type == MemoryAccess::Read) {
    m_vmemSim->access(dataAccess.address, MemoryAccess::Read);
    dataTx = m_vmemSim->getLastTransaction();
    hasData = true;
  }

  if (instrAccess.type == MemoryAccess::Read) {
    m_vmemSim->access(instrAccess.address, MemoryAccess::Read);
    instrTx = m_vmemSim->getLastTransaction();
    hasInstr = true;
  }

  m_vmemSim->emitCyclePair(dataTx, hasData, instrTx, hasInstr);
}

} // namespace Ripes
