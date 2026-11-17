#include "settings/file_picker_task.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

void Check(bool value)
{
  if (!value)
    throw std::runtime_error("file picker task ownership failed");
}
int main()
{
  using namespace std::chrono_literals;
  const auto         owner    = std::this_thread::get_id();
  auto               release  = std::make_shared<std::promise<void>>();
  auto               released = release->get_future().share();
  std::promise<void> entered, finished;
  auto               started = entered.get_future(), done = finished.get_future();
  const auto         before = std::chrono::steady_clock::now();
  {
    auto result = mod_settings::RunFilePickerTask([released, &entered, &finished, owner] {
      Check(std::this_thread::get_id() != owner);
      entered.set_value();
      if (released.wait_for(3s) != std::future_status::ready)
        throw std::runtime_error("bounded fixture release expired");
      finished.set_value();
      return std::string("owned result");
    });
    Check(started.wait_for(1s) == std::future_status::ready);
    Check(result.wait_for(0s) == std::future_status::timeout);
    // Destroy a pending future before releasing its worker: std::async would join.
  }
  Check(std::chrono::steady_clock::now() - before < 1s);
  release->set_value();
  Check(done.wait_for(1s) == std::future_status::ready);
  auto failed =
      mod_settings::RunFilePickerTask([]() -> std::string { throw std::runtime_error("dialog unavailable"); });
  Check(failed.wait_for(1s) == std::future_status::ready);
  bool caught = false;
  try {
    failed.get();
  } catch (const std::runtime_error&) {
    caught = true;
  }
  Check(caught);
  std::cout << "File picker task: owned worker, nonjoining future and exception delivery passed\n";
}
