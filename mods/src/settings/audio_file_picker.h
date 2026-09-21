#pragma once
#include <future>
#include <string>

// Completes with an absolute UTF-8 path, empty on cancel, or an exception on failure.
// Never waits for user input on the game thread.
std::future<std::string> OpenAudioFilePicker();
