## 2026-03-31 - CostEngine numeric validation QRegularExpression overhead
**Learning:** `CostEngine::estimate` calls `parseScaled` dozens of times per estimate, where creating temporary `QRegularExpression("^\\d+$")` instances causes heavy PCRE compilation and heap allocation overhead in calculation paths.
**Action:** Use fast character inspection loops (e.g. `isDigit()`) or static regex instances for numeric validation in hot evaluation loops.
