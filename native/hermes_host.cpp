#include "hermes/Public/RuntimeConfig.h"
#include "hermes/VM/Runtime.h"
#include "napi/hermes_napi.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" napi_value blendx_module_init(napi_env env, napi_value exports);

namespace {

using Clock = std::chrono::steady_clock;
constexpr char kPayloadMagic[] = "BLENDXH1";
std::atomic<bool> interrupted{false};

void on_signal(int) {
  interrupted.store(true, std::memory_order_relaxed);
}

bool check(napi_env env, napi_status status, const char* operation) {
  if (status == napi_ok) return true;
  const napi_extended_error_info* info = nullptr;
  napi_get_last_error_info(env, &info);
  std::cerr << "BlendX Hermes host: " << operation;
  if (info && info->error_message) std::cerr << ": " << info->error_message;
  std::cerr << '\n';
  return false;
}

std::string value_string(napi_env env, napi_value value) {
  napi_value text;
  if (napi_coerce_to_string(env, value, &text) != napi_ok) return "<unprintable>";
  size_t length = 0;
  napi_get_value_string_utf8(env, text, nullptr, 0, &length);
  std::string result(length + 1, '\0');
  napi_get_value_string_utf8(env, text, result.data(), result.size(), &length);
  result.resize(length);
  return result;
}

bool print_exception(napi_env env) {
  bool pending = false;
  napi_is_exception_pending(env, &pending);
  if (!pending) return false;
  napi_value exception;
  napi_get_and_clear_last_exception(env, &exception);
  std::cerr << "Uncaught Hermes exception: " << value_string(env, exception) << '\n';
  bool has_stack = false;
  if (napi_has_named_property(env, exception, "stack", &has_stack) == napi_ok && has_stack) {
    napi_value stack;
    if (napi_get_named_property(env, exception, "stack", &stack) == napi_ok) {
      std::cerr << value_string(env, stack) << '\n';
    }
  }
  return true;
}

std::vector<uint8_t> read_file(const std::string& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) return {};
  const auto end = stream.tellg();
  if (end <= 0) return {};
  std::vector<uint8_t> bytes(static_cast<size_t>(end));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return stream ? bytes : std::vector<uint8_t>{};
}

uint64_t little_endian_u64(const uint8_t* bytes) {
  uint64_t value = 0;
  for (unsigned index = 0; index < 8; ++index) value |= uint64_t(bytes[index]) << (index * 8);
  return value;
}

std::vector<uint8_t> embedded_payload(const std::string& executable) {
  std::vector<uint8_t> bytes = read_file(executable);
  if (bytes.size() < 16) return {};
  const size_t footer = bytes.size() - 16;
  if (std::memcmp(bytes.data() + footer + 8, kPayloadMagic, 8) != 0) return {};
  const uint64_t payload_size = little_endian_u64(bytes.data() + footer);
  if (payload_size > footer) return {};
  const size_t start = footer - static_cast<size_t>(payload_size);
  return std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                              bytes.begin() + static_cast<std::ptrdiff_t>(footer));
}

class Host {
 public:
  Host(napi_env env, hermes::vm::Runtime& runtime, int argc, char** argv, int app_arg_start)
      : env_(env), runtime_(runtime), started_(Clock::now()), argc_(argc), argv_(argv),
        app_arg_start_(app_arg_start) {}

  ~Host() {
    for (auto& [_, timer] : timers_) napi_delete_reference(env_, timer.callback);
    if (signal_callback_) napi_delete_reference(env_, signal_callback_);
    if (native_renderer_) napi_delete_reference(env_, native_renderer_);
  }

  bool install(napi_value native_renderer) {
    napi_value global;
    if (!check(env_, napi_get_global(env_, &global), "get global object")) return false;
    if (!check(env_, napi_set_named_property(env_, global, "__blendxNative", native_renderer),
               "install native renderer")) return false;
    if (!check(env_, napi_create_reference(env_, native_renderer, 1, &native_renderer_),
               "retain native renderer")) return false;
    napi_value native_event_loop;
    napi_get_boolean(env_, true, &native_event_loop);
    napi_set_named_property(env_, global, "__blendxNativeEventLoop", native_event_loop);

    if (!install_function(global, "setTimeout", set_timeout, this) ||
        !install_function(global, "setInterval", set_interval, this) ||
        !install_function(global, "clearTimeout", clear_timer, this) ||
        !install_function(global, "clearInterval", clear_timer, this) ||
        !install_function(global, "setImmediate", set_immediate, this)) return false;

    napi_value performance;
    napi_create_object(env_, &performance);
    if (!install_function(performance, "now", performance_now, this)) return false;
    napi_set_named_property(env_, global, "performance", performance);

    napi_value console;
    napi_create_object(env_, &console);
    if (!install_function(console, "log", console_stdout, this) ||
        !install_function(console, "info", console_stdout, this) ||
        !install_function(console, "warn", console_stderr, this) ||
        !install_function(console, "error", console_stderr, this)) return false;
    napi_set_named_property(env_, global, "console", console);

    napi_value process;
    napi_create_object(env_, &process);
    napi_value arguments;
    const size_t app_count = app_arg_start_ < argc_ ? static_cast<size_t>(argc_ - app_arg_start_) : 0;
    napi_create_array_with_length(env_, app_count + 1, &arguments);
    set_array_string(arguments, 0, argv_[0]);
    for (size_t index = 0; index < app_count; ++index) {
      set_array_string(arguments, static_cast<uint32_t>(index + 1), argv_[app_arg_start_ + index]);
    }
    napi_set_named_property(env_, process, "argv", arguments);
    if (!install_function(process, "on", process_on, this)) return false;
    napi_set_named_property(env_, global, "process", process);
    return true;
  }

  int run() {
    bool renderer_running = true;
    while (renderer_running) {
      if (interrupted.exchange(false, std::memory_order_relaxed)) invoke_signal();
      if (!run_due_timers()) return 1;
      if (runtime_.drainJobs() == hermes::vm::ExecutionStatus::EXCEPTION) {
        print_exception(env_);
        return 1;
      }
      if (print_exception(env_)) return 1;
      renderer_running = poll_renderer();
      if (print_exception(env_)) return 1;
      // Closing the native window or calling renderer.shutdown() is terminal.
      // Animation intervals may still be registered if the close originated in
      // SDL rather than React, so they must not keep the process alive.
      if (!renderer_running) break;
      auto next = Clock::time_point::max();
      for (const auto& [_, timer] : timers_) next = std::min(next, timer.due);
      const auto now = Clock::now();
      if (next > now || next == Clock::time_point::max()) {
        const auto remaining = next == Clock::time_point::max()
                                   ? std::chrono::milliseconds(1000)
                                   : std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
        const int timeout = static_cast<int>(std::clamp<int64_t>(remaining.count(), 1, 1000));
        if (renderer_running && SDL_WasInit(SDL_INIT_EVENTS) != 0) {
          SDL_Event event;
          if (SDL_WaitEventTimeout(&event, timeout) == 1) SDL_PushEvent(&event);
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        }
      }
    }
    return 0;
  }

 private:
  struct Timer {
    napi_ref callback = nullptr;
    Clock::time_point due;
    std::chrono::milliseconds interval{0};
  };

  static Host* callback_host(napi_env env, napi_callback_info info,
                             std::vector<napi_value>& values) {
    size_t count = values.size();
    void* data = nullptr;
    napi_get_cb_info(env, info, &count, values.data(), nullptr, &data);
    values.resize(count);
    return static_cast<Host*>(data);
  }

  bool install_function(napi_value object, const char* name, napi_callback callback, void* data) {
    napi_value function;
    if (!check(env_, napi_create_function(env_, name, NAPI_AUTO_LENGTH, callback, data, &function), name)) {
      return false;
    }
    return check(env_, napi_set_named_property(env_, object, name, function), name);
  }

  void set_array_string(napi_value array, uint32_t index, const char* text) {
    napi_value value;
    napi_create_string_utf8(env_, text, NAPI_AUTO_LENGTH, &value);
    napi_set_element(env_, array, index, value);
  }

  uint64_t add_timer(napi_value callback, double delay, bool repeat) {
    const uint64_t id = next_timer_id_++;
    Timer timer;
    napi_create_reference(env_, callback, 1, &timer.callback);
    const auto milliseconds = std::chrono::milliseconds(
        static_cast<int64_t>(std::max(0.0, std::min(delay, 2147483647.0))));
    timer.due = Clock::now() + milliseconds;
    timer.interval = repeat ? std::max(milliseconds, std::chrono::milliseconds(1))
                            : std::chrono::milliseconds(0);
    timers_.emplace(id, timer);
    return id;
  }

  static napi_value timer_result(napi_env env, uint64_t id) {
    napi_value result;
    napi_create_double(env, static_cast<double>(id), &result);
    return result;
  }

  static napi_value set_timeout(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values(2);
    Host* host = callback_host(env, info, values);
    if (!host || values.empty()) return undefined(env);
    double delay = 0;
    if (values.size() > 1) napi_get_value_double(env, values[1], &delay);
    return timer_result(env, host->add_timer(values[0], delay, false));
  }

  static napi_value set_interval(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values(2);
    Host* host = callback_host(env, info, values);
    if (!host || values.empty()) return undefined(env);
    double delay = 0;
    if (values.size() > 1) napi_get_value_double(env, values[1], &delay);
    return timer_result(env, host->add_timer(values[0], delay, true));
  }

  static napi_value set_immediate(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values(1);
    Host* host = callback_host(env, info, values);
    if (!host || values.empty()) return undefined(env);
    return timer_result(env, host->add_timer(values[0], 0, false));
  }

  static napi_value clear_timer(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values(1);
    Host* host = callback_host(env, info, values);
    if (!host || values.empty()) return undefined(env);
    double raw_id = 0;
    napi_get_value_double(env, values[0], &raw_id);
    const uint64_t id = static_cast<uint64_t>(std::max(0.0, raw_id));
    auto found = host->timers_.find(id);
    if (found != host->timers_.end()) {
      napi_delete_reference(env, found->second.callback);
      host->timers_.erase(found);
    }
    return undefined(env);
  }

  static napi_value performance_now(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values;
    Host* host = callback_host(env, info, values);
    const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - host->started_).count();
    napi_value result;
    napi_create_double(env, elapsed, &result);
    return result;
  }

  static napi_value write_console(napi_env env, napi_callback_info info, std::ostream& stream) {
    std::vector<napi_value> values(32);
    callback_host(env, info, values);
    for (size_t index = 0; index < values.size(); ++index) {
      if (index) stream << ' ';
      stream << value_string(env, values[index]);
    }
    stream << '\n';
    return undefined(env);
  }

  static napi_value console_stdout(napi_env env, napi_callback_info info) {
    return write_console(env, info, std::cout);
  }

  static napi_value console_stderr(napi_env env, napi_callback_info info) {
    return write_console(env, info, std::cerr);
  }

  static napi_value process_on(napi_env env, napi_callback_info info) {
    std::vector<napi_value> values(2);
    Host* host = callback_host(env, info, values);
    if (!host || values.size() < 2 || value_string(env, values[0]) != "SIGINT") return undefined(env);
    if (host->signal_callback_) napi_delete_reference(env, host->signal_callback_);
    napi_create_reference(env, values[1], 1, &host->signal_callback_);
    return undefined(env);
  }

  static napi_value undefined(napi_env env) {
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
  }

  void invoke_signal() {
    if (!signal_callback_) return;
    napi_handle_scope scope;
    napi_open_handle_scope(env_, &scope);
    napi_value callback;
    napi_value global;
    napi_get_reference_value(env_, signal_callback_, &callback);
    napi_get_global(env_, &global);
    napi_call_function(env_, global, callback, 0, nullptr, nullptr);
    napi_delete_reference(env_, signal_callback_);
    signal_callback_ = nullptr;
    napi_close_handle_scope(env_, scope);
  }

  bool run_due_timers() {
    const auto now = Clock::now();
    std::vector<uint64_t> due;
    due.reserve(timers_.size());
    for (const auto& [id, timer] : timers_) {
      if (timer.due <= now) due.push_back(id);
    }
    std::sort(due.begin(), due.end());
    for (uint64_t id : due) {
      auto found = timers_.find(id);
      if (found == timers_.end()) continue;
      napi_handle_scope scope;
      napi_open_handle_scope(env_, &scope);
      napi_value callback;
      napi_value global;
      napi_get_reference_value(env_, found->second.callback, &callback);
      napi_get_global(env_, &global);
      if (found->second.interval.count() == 0) {
        napi_delete_reference(env_, found->second.callback);
        timers_.erase(found);
      } else {
        found->second.due = now + found->second.interval;
      }
      napi_call_function(env_, global, callback, 0, nullptr, nullptr);
      napi_close_handle_scope(env_, scope);
      if (print_exception(env_)) return false;
    }
    return true;
  }

  bool poll_renderer() {
    if (!native_renderer_) return false;
    napi_handle_scope scope;
    napi_open_handle_scope(env_, &scope);
    napi_value renderer;
    napi_value poll;
    napi_value result;
    bool running = false;
    if (napi_get_reference_value(env_, native_renderer_, &renderer) == napi_ok &&
        napi_get_named_property(env_, renderer, "poll", &poll) == napi_ok &&
        napi_call_function(env_, renderer, poll, 0, nullptr, &result) == napi_ok) {
      napi_get_value_bool(env_, result, &running);
    }
    napi_close_handle_scope(env_, scope);
    return running;
  }

  napi_env env_;
  hermes::vm::Runtime& runtime_;
  Clock::time_point started_;
  int argc_;
  char** argv_;
  int app_arg_start_;
  uint64_t next_timer_id_ = 1;
  std::unordered_map<uint64_t, Timer> timers_;
  napi_ref signal_callback_ = nullptr;
  napi_ref native_renderer_ = nullptr;
};

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);

  std::vector<uint8_t> bytecode = embedded_payload(argv[0]);
  int app_arg_start = 1;
  std::string source_name = "<embedded>";
  if (bytecode.empty() && argc > 1) {
    bytecode = read_file(argv[1]);
    source_name = argv[1];
    app_arg_start = 2;
  }
  if (bytecode.empty()) {
    std::cerr << "Usage: " << argv[0] << " <application.hbc> [application arguments...]\n";
    return 2;
  }

  auto config = hermes::vm::RuntimeConfig::Builder()
                    .withMicrotaskQueue(true)
                    .withIntl(false)
                    .build();
  auto runtime = hermes::vm::Runtime::create(config);
  napi_env env = hermes_napi_create_env(&*runtime);
  napi_handle_scope scope;
  if (!check(env, napi_open_handle_scope(env, &scope), "open root handle scope")) return 1;

  napi_value exports;
  napi_create_object(env, &exports);
  exports = blendx_module_init(env, exports);
  if (!exports || print_exception(env)) return 1;

  Host host(env, *runtime, argc, argv, app_arg_start);
  if (!host.install(exports)) return 1;

  hermes_bytecode_flags flags{};
  flags.struct_size = sizeof(flags);
  flags.persistent = true;
  napi_value result;
  const napi_status status = hermes_run_bytecode(
      env, bytecode.data(), bytecode.size(), nullptr, nullptr, source_name.c_str(), &flags, &result);
  if (status != napi_ok) {
    if (!print_exception(env)) {
      std::cerr << "BlendX Hermes host: bytecode execution failed with N-API status " << status << '\n';
    }
    return 1;
  }
  if (print_exception(env)) return 1;
  if (runtime->drainJobs() == hermes::vm::ExecutionStatus::EXCEPTION) {
    print_exception(env);
    return 1;
  }
  if (print_exception(env)) return 1;

  const int exit_code = host.run();
  napi_close_handle_scope(env, scope);
  return exit_code;
}
