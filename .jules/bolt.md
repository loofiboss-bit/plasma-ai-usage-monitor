# Bolt's Journal - Critical Learnings

## 2026-03-31 - Avoid Dynamic QRegularExpression Instantiation in Hot Paths
**Learning:** Constructing `QRegularExpression` dynamically inside frequently called functions (such as numeric parsers called 15-30+ times per estimation) incurs heavy heap allocations and regex compilation overhead. Replacing dynamic regexes with direct string/character inspection (`QChar::isDigit`) eliminates heap allocation and PCRE parsing overhead completely.
**Action:** Always prefer direct string/char character loops or static regex patterns over dynamic `QRegularExpression` instantiation on hot code paths.
