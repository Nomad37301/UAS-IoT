#include "BlackboxLogger.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace aether;

int main() {
  fs::FS storage;
  BlackboxLogger logger(storage);
  assert(logger.append(0, "TEST", "before begin") == LogError::NotReady);
  assert(logger.begin(0x1234) == LogError::None && storage.files.empty());
  assert(logger.append(42, "QUOTE_TEST", "quote \" path \\\n\t") == LogError::None);
  assert(logger.nextSequence() == 1 && storage.faults.flushes == 1);
  assert(storage.bytes(kBlackboxCurrent).back() == '\n');

  // Worst-case escaping tetap satu baris lengkap dalam buffer.
  char controls[97];
  for (size_t i = 0; i < 96; ++i) controls[i] = '\x01';
  controls[96] = '\0';
  assert(logger.append(UINT32_MAX, "CONTROL_TEST", controls) == LogError::None);
  const auto samples = storage.bytes(kBlackboxCurrent);
  std::ofstream out(".build/blackbox-json-samples.jsonl", std::ios::binary);
  out << samples;
  out.close();
  assert(out.good());  // Fixture diperiksa lagi memakai parser JSON Python.

  assert(logger.append(0, "bad\"event", "text") == LogError::InvalidEvent);
  assert(logger.append(0, "", "text") == LogError::InvalidEvent);
  assert(logger.append(0, nullptr, "text") == LogError::InvalidEvent);
  const std::string longEvent(25, 'A'), longMessage(97, 'x');
  assert(logger.append(0, longEvent.c_str(), "text") == LogError::InvalidEvent);
  assert(logger.append(0, "TEST", longMessage.c_str()) == LogError::MessageTooLong);
  assert(logger.append(0, "TEST", nullptr) == LogError::MessageTooLong);
  assert(storage.bytes(kBlackboxCurrent) == samples && logger.ready());

  // Mount ulang mempertahankan isi; seq baru dibedakan session baru.
  logger.end();
  assert(logger.append(0, "TEST", "off") == LogError::NotReady);
  assert(logger.begin(0x5678) == LogError::None);
  assert(logger.append(0, "NEW_SESSION", "after restart") == LogError::None);
  assert(storage.bytes(kBlackboxCurrent).substr(0, samples.size()) == samples);

  // Tepat pada batas file tidak merotasi, byte berikutnya merotasi.
  fs::FS measured;
  BlackboxLogger measure(measured);
  measure.begin(1);
  measure.append(1, "T", "M");
  const size_t lineLength = measured.bytes(kBlackboxCurrent).size();
  fs::FS bounded;
  bounded.seed(kBlackboxCurrent, std::string(kBlackboxFileLimit - lineLength - 1, 'x') + "\n");
  BlackboxLogger rotating(bounded);
  assert(rotating.begin(1) == LogError::None);
  assert(rotating.append(1, "T", "M") == LogError::None);
  assert(bounded.bytes(kBlackboxCurrent).size() == kBlackboxFileLimit);
  assert(!bounded.exists(kBlackboxPrevious));
  assert(rotating.append(2, "T", "M") == LogError::None);
  assert(bounded.bytes(kBlackboxPrevious).size() == kBlackboxFileLimit);
  assert(bounded.bytes(kBlackboxCurrent).size() < kBlackboxFileLimit);
  bounded.seed(kBlackboxCurrent, std::string(kBlackboxFileLimit - 1, 'y') + "\n");
  rotating.begin(2);
  rotating.append(3, "T", "M");
  assert(bounded.files.size() == 2 && bounded.bytes(kBlackboxPrevious).front() == 'y');

  // Catatan terputus dipisahkan; tidak disambung dengan JSON baru.
  fs::FS torn;
  torn.seed(kBlackboxCurrent, "{\"partial");
  torn.seed(kBlackboxPrevious, "older\n");
  BlackboxLogger recovering(torn);
  assert(recovering.begin(3) == LogError::None);
  assert(torn.bytes(kBlackboxPrevious) == "older\n");  // Begin tidak memutasi flash.
  assert(recovering.append(1, "RECOVERED", "new line") == LogError::None);
  assert(torn.bytes(kBlackboxPrevious) == "{\"partial");
  assert(torn.bytes(kBlackboxCurrent).find("RECOVERED") != std::string::npos);

  fs::FS partial;
  BlackboxLogger writer(partial);
  writer.begin(4);
  partial.faults.writeLimit = 7;
  assert(writer.append(1, "TEST", "partial write") == LogError::WriteFailed);
  assert(!writer.ready() && writer.nextSequence() == 0);
  assert(partial.bytes(kBlackboxCurrent).size() == 7);
  partial.faults.writeLimit = static_cast<size_t>(-1);
  assert(writer.begin(5) == LogError::None);
  assert(writer.append(2, "RECOVERED", "after IO failure") == LogError::None);
  assert(partial.bytes(kBlackboxPrevious).size() == 7);

  fs::FS failed;
  failed.seed(kBlackboxCurrent, std::string(kBlackboxFileLimit - 1, 'z') + "\n");
  failed.seed(kBlackboxPrevious, "preserve\n");
  BlackboxLogger guarded(failed);
  guarded.begin(6);
  failed.faults.remove = true;
  assert(guarded.append(1, "T", "M") == LogError::RemoveFailed);
  assert(!guarded.ready() && failed.bytes(kBlackboxPrevious) == "preserve\n");
  failed.faults.remove = false;
  guarded.begin(7);
  failed.faults.rename = true;
  assert(guarded.append(1, "T", "M") == LogError::RenameFailed);
  assert(!guarded.ready() && failed.bytes(kBlackboxCurrent).size() == kBlackboxFileLimit);

  failed.faults.rename = false;
  failed.faults.open = true;
  assert(guarded.begin(8) == LogError::OpenFailed);
  failed.faults.open = false; failed.faults.seek = true;
  assert(guarded.begin(8) == LogError::ReadFailed);
  failed.faults.seek = false; failed.faults.read = true;
  assert(guarded.begin(8) == LogError::ReadFailed);
  failed.faults.read = false;
  failed.seed(kBlackboxPrevious, std::string(kBlackboxFileLimit + 1, 'x'));
  assert(guarded.begin(8) == LogError::OversizedFile);
  assert(failed.bytes(kBlackboxPrevious).size() == kBlackboxFileLimit + 1);
  std::cout << "PASS: JSON escaping, bounded rotation, persistence, torn tail, "
               "partial write, IO failures, no automatic formatting\n";
}
