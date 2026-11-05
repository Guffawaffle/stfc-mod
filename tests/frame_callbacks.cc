// The subscriber registry is independent of native hooks, Unity and Config.
#include "frame_callbacks.h"

#include <array>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
FrameCallbacks* current = nullptr;
struct Observations {
  std::array<int, 32> order{};
  std::size_t         count            = 0;
  bool                new_registration = true, duplicate_registration = true, null_registration = true;
  bool                reporter_registration = true;
  unsigned            reports               = 0;
  std::string         name;
  std::exception_ptr  error;
} observed;

void Check(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
void Reset(FrameCallbacks& registry)
{
  observed = {};
  current  = &registry;
}
void Record(int value)
{
  Check(observed.count < observed.order.size(), "recursive dispatch exhausted the observation buffer");
  observed.order[observed.count++] = value;
}
void Order(std::initializer_list<int> expected)
{
  Check(observed.count == expected.size(), "unexpected callback count");
  std::size_t index = 0;
  for (int value : expected)
    Check(observed.order[index++] == value, "callback order changed");
}
void First()
{ Record(1); }
void Second()
{ Record(2); }
void Third()
{ Record(3); }
void ThrowStandard()
{ throw std::runtime_error("callback failure"); }
void ThrowUnknown()
{ throw 17; }
void Report(const char* name, std::exception_ptr error)
{
  ++observed.reports;
  observed.name  = name;
  observed.error = error;
}
void ThrowingReporter(const char* name, std::exception_ptr error)
{
  Report(name, error);
  observed.reporter_registration = current->Register(Third, "third");
  current->Dispatch();
  throw std::logic_error("reporter failure");
}
void RegisterDuringDispatch()
{
  Record(4);
  observed.new_registration       = current->Register(Third, "third");
  observed.duplicate_registration = current->Register(First, "duplicate first");
  observed.null_registration      = current->Register(nullptr, "null");
}
void Reenter()
{
  Record(5);
  current->Dispatch();
  observed.new_registration = current->Register(Third, "third");
  Record(6);
}

void RegistrationAndOrder()
{
  FrameCallbacks registry;
  Reset(registry);
  Check(registry.RegistrationCount() == 0, "new registry has subscribers");
  registry.Dispatch();
  Check(!registry.Register(nullptr, "null"), "null callback accepted");
  Check(registry.Register(First, "first") && registry.Register(Second, "second"), "registration failed");
  Check(registry.Register(First, "another name"), "duplicate registration was not idempotent");
  Check(registry.RegistrationCount() == 2, "duplicate registration added a subscriber");
  registry.Dispatch();
  registry.Dispatch();
  Order({1, 2, 1, 2});
}

void DispatchRejectsRegistration()
{
  FrameCallbacks registry;
  Reset(registry);
  registry.Register(RegisterDuringDispatch, "registering callback");
  registry.Register(First, "first");
  registry.Dispatch();
  Order({4, 1});
  Check(!observed.new_registration && !observed.duplicate_registration && !observed.null_registration,
        "registration during dispatch was accepted");
  Check(registry.RegistrationCount() == 2, "dispatch mutated the subscriber list");
  Check(registry.Register(Third, "third"), "dispatch state was not released");
  registry.Dispatch();
  Order({4, 1, 4, 1, 3});
}

void RecursiveDispatch()
{
  FrameCallbacks registry;
  Reset(registry);
  registry.Register(Reenter, "recursive callback");
  registry.Register(Second, "second");
  registry.Dispatch();
  Order({5, 6, 2});
  Check(!observed.new_registration, "nested dispatch released the outer dispatch guard");
  Check(registry.Register(Third, "third"), "nested dispatch stranded the guard");
}

void ExceptionsAndOwnedName()
{
  FrameCallbacks registry(Report);
  Reset(registry);
  std::string name = "original callback name";
  registry.Register(ThrowStandard, name.c_str());
  name.assign("changed caller-owned name");
  Check(registry.Register(ThrowStandard, "duplicate name"), "duplicate throwing callback rejected");
  registry.Register(Second, "second");
  registry.Dispatch();
  Order({2});
  Check(observed.reports == 1 && observed.name == "original callback name", "diagnostic name was not retained");
  Check(bool(observed.error), "callback exception was not supplied to the reporter");
  try {
    std::rethrow_exception(observed.error);
  } catch (const std::runtime_error& error) {
    Check(std::string(error.what()) == "callback failure", "reported exception changed");
  }
  Check(registry.Register(Third, "third"), "callback exception stranded the guard");
  registry.Dispatch();
  Order({2, 2, 3});
  Check(observed.reports == 2, "later dispatch did not report its callback exception");
}

void ReporterFailure()
{
  FrameCallbacks registry(ThrowingReporter);
  Reset(registry);
  registry.Register(ThrowStandard, "throwing callback");
  registry.Register(Second, "second");
  registry.Dispatch();
  Order({2});
  Check(observed.reports == 1 && !observed.reporter_registration,
        "reporter recursion or registration was not suppressed");
  Check(registry.Register(Third, "third"), "reporter exception stranded the guard");
  registry.Dispatch();
  Order({2, 2, 3});
  Check(observed.reports == 2, "reporter failure prevented the next dispatch");
}

void UnknownAndUnreportedExceptions()
{
  FrameCallbacks registry(Report);
  Reset(registry);
  registry.Register(ThrowUnknown, nullptr);
  registry.Register(Second, "second");
  registry.Dispatch();
  Order({2});
  Check(observed.reports == 1 && !observed.name.empty(), "unnamed callback failure was not reported");
  Check(bool(observed.error), "non-standard exception was not supplied to the reporter");
  bool caught = false;
  try {
    std::rethrow_exception(observed.error);
  } catch (int value) {
    caught = value == 17;
  }
  Check(caught, "non-standard exception was not preserved");

  FrameCallbacks silent;
  Reset(silent);
  silent.Register(ThrowUnknown, "no reporter");
  silent.Register(Second, "second");
  silent.Dispatch();
  Order({2});
  Check(silent.Register(Third, "third"), "unreported exception stranded the guard");
}
} // namespace

int main()
{
  try {
    RegistrationAndOrder();
    DispatchRejectsRegistration();
    RecursiveDispatch();
    ExceptionsAndOwnedName();
    ReporterFailure();
    UnknownAndUnreportedExceptions();
    std::cout << "Frame callbacks passed: stable registration/order, mutation and recursion rejection, "
                 "owned names, exception reporting and guard recovery\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Frame callbacks failed: " << error.what() << '\n';
    return 1;
  }
}
