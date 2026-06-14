#pragma once

#include <memory>

#include <QTableWidget>
#include <QWidget>

#include "vmemsim.h"

namespace Ripes {

class VMemTableWidget : public QWidget {
  Q_OBJECT

public:
  explicit VMemTableWidget(std::shared_ptr<VMemSim> vmemSim,
                           QWidget *parent = nullptr);

private slots:
  void dataChanged(VMemSim::VMemTransaction transaction);
  void configurationChanged();

private:
  void rebuildTables();
  void populateTLBTable(int highlightRow = -1);
  void populatePageTable(int highlightRow = -1);
  static void setRowColor(QTableWidget *table, int row, const QColor &color);

  std::shared_ptr<VMemSim> m_vmemSim;

  QWidget      *m_tlbContainer = nullptr;
  QTableWidget *m_tlbTable     = nullptr;
  QTableWidget *m_ptTable      = nullptr;
};

} // namespace Ripes
