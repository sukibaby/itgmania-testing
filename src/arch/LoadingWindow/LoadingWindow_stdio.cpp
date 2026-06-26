#include "LoadingWindow_stdio.h"

#include <iostream>
#include <string>

std::string LoadingWindow_stdio::Init() {
  std::cout << "[Loading Songs] Progress: 0%" << std::endl;
  return "";
}

void LoadingWindow_stdio::SetText(std::string str) {}

void LoadingWindow_stdio::SetProgress(const int progress) {
  LoadingWindow::SetProgress(progress);

  if (m_totalWork > 0) {
    int pct = static_cast<int>((m_progress * 100.0) / m_totalWork);
    std::cout << "[Loading Songs] Progress: " << pct << "%" << std::endl;
  }
}

void LoadingWindow_stdio::SetTotalWork(const int totalWork) {
  LoadingWindow::SetTotalWork(totalWork);
}

void LoadingWindow_stdio::SetIndeterminate(bool indeterminate) {
  LoadingWindow::SetIndeterminate(indeterminate);
}