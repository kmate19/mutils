#include "mutils/mutils.hpp"

int main() {
  auto timer = mutils::Timer{};
  DEFER(timer.printElapsed("Total execution time"));
  DEFER(LOG("Exiting main function"));
  LOG("This is a log message with value: {}", 42);
  auto file = mutils::readFile("non_existent_file.txt");
  file.has_value() ? LOG("File read successfully, size: {} bytes", file->size())
                   : void();

  auto file2 = mutils::readFileToString("CMakeLists.txt");
  if (!file2.has_value()) {
    return -1;
  }

  LOG("File read successfully, size: {} bytes", file2->size());
  LOG_DBG("This is a debug message with file2:\n {}", *file2);

  for (const auto &line : mutils::lines(*file2)) {
    LOG_DBG("Line: {}", line);
  }

  return 0;
}
