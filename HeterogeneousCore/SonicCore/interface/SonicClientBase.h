#ifndef HeterogeneousCore_SonicCore_SonicClientBase
#define HeterogeneousCore_SonicCore_SonicClientBase

#include "FWCore/Concurrency/interface/WaitingTaskWithArenaHolder.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <string>
#include <chrono>
#include <exception>

class SonicClientBase {
public:
  //destructor
  virtual ~SonicClientBase() {}

  void setDebugName(const std::string& debugName) { debugName_ = debugName; }

  //main operation
  virtual void predict(edm::WaitingTaskWithArenaHolder holder) = 0;

protected:
  virtual void predictImpl() = 0;

  void setStartTime() {
    if (debugName_.empty())
      return;
    t0_ = std::chrono::high_resolution_clock::now();
    setTime_ = true;
  }

  void finish(std::exception_ptr eptr = std::exception_ptr{}) {
    if (setTime_) {
      auto t1 = std::chrono::high_resolution_clock::now();
      edm::LogInfo(debugName_) << "Client time: "
                               << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0_).count();
    }
    holder_.doneWaiting(eptr);
  }

  //members
  edm::WaitingTaskWithArenaHolder holder_;

  //for logging/debugging
  std::string debugName_;
  std::chrono::time_point<std::chrono::high_resolution_clock> t0_;
  bool setTime_ = false;
};

#endif
