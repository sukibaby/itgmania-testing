// Writes the song loading progress to standard output.

#ifndef LOADING_WINDOW_STDIO_H
#define LOADING_WINDOW_STDIO_H

#include <iostream>
#include <string>

#include "LoadingWindow.h"
#include "RageSurface.h"

class LoadingWindow_stdio : public LoadingWindow {
 public:
  LoadingWindow_stdio() {}
  ~LoadingWindow_stdio() override {}

  std::string Init() override;
  void SetText(std::string str) override;

  void SetProgress(const int progress) override;
  void SetTotalWork(const int totalWork) override;
  void SetIndeterminate(bool indeterminate) override;

  // Unused, but necessary.
  void SetIcon(const RageSurface* pIcon) override {}
  void SetSplash(const RageSurface* pSplash) override {}
};

#define USE_LOADING_WINDOW_STDIO
#endif