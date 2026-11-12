#pragma once

#include <string>
#include <vector>

namespace ultralight { class View; class String16; }

namespace stub {
/* Every view the stub renderer created (nullptr once destroyed). */
extern std::vector<ultralight::View *> g_views;
/* What the views were asked to load: "html:<source>" or "url:<url>". */
extern std::vector<std::string> g_loads;
/* Number of script evaluations. */
extern int g_evals;
std::string narrow(const ultralight::String16 &s);
}
