#include "vmemtab.h"

#include <QAction>
#include <QIcon>
#include <QSplitter>
#include <QVBoxLayout>

#include "processorhandler.h"
#include "vmemsim/vmemconfigwidget.h"
#include "vmemsim/vmemshim.h"
#include "vmemsim/vmemstatswidget.h"
#include "vmemsim/vmemtablewidget.h"
#include "vmemsim/vmemtransactionwidget.h"

namespace Ripes {

VMemTab::VMemTab(QToolBar *toolbar, QWidget *parent)
    : RipesTab(toolbar, parent) {

  // Shim owns the VMemSim instance
  auto *shim = new VMemShim(this);
  auto vmemSim = shim->getVMemSim();

  // ── Toolbar ────────────────────────────────────────────────────────────────
  auto *resetAction = new QAction(QIcon(":/icons/reset.svg"), "Reset", this);
  resetAction->setToolTip("Reset virtual memory simulation");
  connect(resetAction, &QAction::triggered, vmemSim.get(), &VMemSim::reset);
  toolbar->addAction(resetAction);

  // ── Widgets ────────────────────────────────────────────────────────────────
  auto *configWidget      = new VMemConfigWidget(vmemSim, this);
  auto *statsWidget       = new VMemStatsWidget(vmemSim, this);
  auto *transactionWidget = new VMemTransactionWidget(vmemSim, this);
  auto *tableWidget       = new VMemTableWidget(vmemSim, this);

  // ── Left panel: config (top) + stats (bottom) ──────────────────────────────
  auto *leftSplitter = new QSplitter(Qt::Vertical, this);
  leftSplitter->addWidget(configWidget);
  leftSplitter->addWidget(statsWidget);
  leftSplitter->setStretchFactor(0, 1);
  leftSplitter->setStretchFactor(1, 0);

  // ── Right panel: transaction (top) + table (bottom) ───────────────────────
  auto *rightSplitter = new QSplitter(Qt::Vertical, this);
  rightSplitter->addWidget(transactionWidget);
  rightSplitter->addWidget(tableWidget);
  rightSplitter->setSizes({200, 10000});

  // ── Horizontal splitter: left (~300 px) | right (fills remaining) ─────────
  auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
  mainSplitter->addWidget(leftSplitter);
  mainSplitter->addWidget(rightSplitter);
  mainSplitter->setSizes({300, 10000});
  mainSplitter->setStretchFactor(0, 0);
  mainSplitter->setStretchFactor(1, 1);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(mainSplitter);

  // Disable the tab while the processor is running (same as CacheTab)
  connect(ProcessorHandler::get(), &ProcessorHandler::runStarted, this,
          [this] { setEnabled(false); });
  connect(ProcessorHandler::get(), &ProcessorHandler::runFinished, this,
          [this] { setEnabled(true); });
}

} // namespace Ripes
