## 2026-03-30 - Avoid Inline QRegularExpression Construction in Parsing Hot Paths
**Learning:** Constructing un-cached `QRegularExpression` instances inline inside hot helper functions (`parseScaled`, `safeModel`, `normalizedPolicy`) compiles the regex pattern and allocates memory on every single call, creating significant overhead during log parsing and cost estimations.
**Action:** Replace simple string pattern checks with direct character iteration (`QChar::isDigit()`, character ranges) or static regexes to eliminate regex compilation overhead entirely.
