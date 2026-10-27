## 2026-10-27 - Zero-Allocation Digit Checking in Hot Parsing Paths
**Learning:** Constructing dynamic `QRegularExpression` instances inside hot path functions (like `CostEngine::parseScaled`) causes significant overhead from regex AST compilation and heap allocations per field parsed.
**Action:** Replace `^\d+$` regex matching with non-allocating `isDigitsOnly(QStringView)` using direct `QChar` digit range comparisons.
