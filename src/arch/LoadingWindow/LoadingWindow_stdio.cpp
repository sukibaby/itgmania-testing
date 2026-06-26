#include "LoadingWindow_stdio.h"

#include <iostream>
#include <string>

std::string LoadingWindow_stdio::Init() {
  std::cout << "[Loading]" << std::endl;
  return "";
}

void LoadingWindow_stdio::SetText(std::string str) {
  if (!str.empty()) {
    std::cout << str << std::endl;
  }
}

void LoadingWindow_stdio::SetProgress(const int progress) {
  LoadingWindow::SetProgress(progress);

  if (m_totalWork > 0) {
    int pct = static_cast<int>((m_progress * 100.0) / m_totalWork);
    std::cout << "[" << pct << "%]" << std::endl;
  }
}

void LoadingWindow_stdio::SetTotalWork(const int totalWork) {
  LoadingWindow::SetTotalWork(totalWork);
}

void LoadingWindow_stdio::SetIndeterminate(bool indeterminate) {
  LoadingWindow::SetIndeterminate(indeterminate);

  if (indeterminate) {
    std::cout << "[...]" << std::endl;
  }
}