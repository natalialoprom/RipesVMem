#pragma once

#include "ripestab.h"
#include <QWidget>

namespace Ripes {

class VMemTab : public RipesTab {
  Q_OBJECT

public:
  explicit VMemTab(QToolBar *toolbar, QWidget *parent = nullptr);
};

} // namespace Ripes
