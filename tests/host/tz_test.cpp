// Host test of WebUiValidTimeZone(), compiled from BF-018ARev5/BF_WebUi.cpp (run_host_tests.py extracts it into tz.inc).
// Reads "expected tz" lines from stdin and prints "result tz" for each, for comparison with the page's validTz().
#include <cctype>
#include <cstdio>
#include <cstring>
#include "tz.inc"

int main()
{
  char line[128];
  int failures = 0;
  while (std::fgets(line, sizeof(line), stdin)) {
    line[std::strcspn(line, "\n")] = '\0';
    if (line[0] == '#' || line[0] == '\0') continue;
    const char *tz = line + 2;
    int got = WebUiValidTimeZone(tz);
    std::printf("%d %s\n", got, tz);
    if (got != line[0] - '0') {
      std::fprintf(stderr, "FAIL: WebUiValidTimeZone(\"%s\") = %d\n", tz, got);
      ++failures;
    }
  }
  return failures != 0;
}
