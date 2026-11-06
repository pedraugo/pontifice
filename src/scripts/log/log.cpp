#include "scripts/log/log.h"

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace logging {

namespace {

const char* BaseName(const char* path) {
  const char* slash = std::strrchr(path, '/');
  const char* backslash = std::strrchr(path, '\\');
  const char* last = slash > backslash ? slash : backslash;
  return last != nullptr ? last + 1 : path;
}

}  // namespace

void Write(const char* file, const char* format, ...) {
  SDL_Time now = 0;
  SDL_DateTime dt{};
  if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) dt = SDL_DateTime{};
  char message[1024];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  std::fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] - %s\n", dt.year, dt.month, dt.day,
               dt.hour, dt.minute, dt.second, BaseName(file), message);
}

}  // namespace logging
