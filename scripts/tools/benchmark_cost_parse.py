#!/usr/bin/env python3
"""Compile an isolated parseScaled microbenchmark from the actual CostEngine source.

No product files are changed. Parity compares both the 128-bit result and ok flag.
"""

from __future__ import annotations

import argparse
import json
import shlex
import statistics
import subprocess
import tempfile
from pathlib import Path

from benchmark_environment import isolated_environment

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path)
    parser.add_argument("--baseline-ref", default="v25.0.1")
    parser.add_argument("--iterations", type=int, default=100000)
    parser.add_argument("--rounds", type=int, default=8)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source = (
        args.source.read_text()
        if args.source
        else subprocess.check_output(
            ["git", "show", f"{args.baseline_ref}:plugin/costengine.cpp"],
            cwd=ROOT,
            text=True,
        )
    )
    start = source.index("QString trimDecimal(")
    end = source.index("WideInt roundDivide(")
    functions = source[start:end]
    replacement = functions.replace("parseScaled(", "parseScaledAscii(")
    replacement = replacement[replacement.index("WideInt parseScaledAscii(") :]
    replacement = replacement.replace(
        'integerPart.contains(QRegularExpression(QStringLiteral("^\\\\d+$")))',
        "asciiDigits(integerPart)",
    )
    replacement = replacement.replace(
        'fractionalPart.contains(QRegularExpression(QStringLiteral("^\\\\d+$")))',
        "asciiDigits(fractionalPart)",
    )
    if "QRegularExpression" in replacement:
        raise RuntimeError("source parser changed; review benchmark adaptation")
    cpp = (
        r"""#include <QtCore>
#include <cmath>
#include <iostream>
using WideInt = __int128_t;
bool asciiDigits(const QString &value) {
    const auto length = value.size() - (value.endsWith(QLatin1Char('\n')) ? 1 : 0);
    if (length == 0) return false;
    for (QChar digit : QStringView(value).left(length)) {
        if (digit < QLatin1Char('0') || digit > QLatin1Char('9')) return false;
    }
    return true;
}
"""
        + functions
        + replacement
        + r"""
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QList<QVariant> parity = {"", ".", "+.", "-.", ".5", "1.", "0", "-0", "+1.2", " 1.2 ",
        "1.1234567895", "1.123456789500000", "1.123456789500x", "1.123456x", "1..2", "1,2", "1\n.2", "1\n\n.2", "1\r\n.2",
        "9223372036854775807", "9223372036854775808", "-9223372036854775808",
        "1e-6", "NaN", "inf", QVariant(), QVariant(true), QVariant(1.25),
        QString::fromUtf8("١.٢"), QString::fromUtf8("１.２"), QString::fromUtf8("1.٢"),
        QString::fromUtf8("1.1234567895٢"), QString::fromUtf8("1.1234567895\u2028"),
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
    for (int digit=0; digit<65536; ++digit) {
        if (QChar(digit).isDigit()) parity.append(QStringLiteral("1.1234567895") + QChar(digit));
    }
    for (qint64 scale : {1000000LL, 1000000000LL}) {
        for (const auto &value : parity) {
            bool oldOk, newOk;
            auto before = parseScaled(value, scale, &oldOk);
            auto after = parseScaledAscii(value, scale, &newOk);
            if (before != after || oldOk != newOk) {
                std::cerr << "Parity failure " << value.toString().toStdString() << "\n"; return 2;
            }
        }
    }
    const QList<QVariant> typical = {"0.15", "3", "0.075", "15.500000000", "1234.56789", "0", ".125", "-1.250001"};
    int iterations = app.arguments().at(1).toInt();
    int rounds = app.arguments().at(2).toInt();
    volatile quint64 sink = 0;
    for (int round=-2; round<rounds; ++round) {
        for (int step=0; step<2; ++step) {
            bool ascii = ((round + step + 2) % 2) != 0;
            QElapsedTimer timer; timer.start();
            quint64 checksum=0;
            for (int i=0; i<iterations; ++i) {
                bool ok=false;
                auto value = ascii ? parseScaledAscii(typical.at(i%typical.size()), 1000000000LL, &ok)
                                   : parseScaled(typical.at(i%typical.size()), 1000000000LL, &ok);
                checksum += static_cast<quint64>(value) + ok;
            }
            auto ns = timer.nsecsElapsed(); sink = checksum;
            if (round>=0) std::cout << (ascii ? "ascii" : "regex") << " " << ns << " " << checksum << "\n";
        }
    }
    std::cerr << "parity_cases=" << parity.size()*2 << " sink=" << sink << "\n";
}
"""
    )
    with tempfile.TemporaryDirectory(prefix="ai-monitor-cost-parse-") as temporary:
        directory = Path(temporary)
        (directory / "bench.cpp").write_text(cpp)
        flags = shlex.split(
            subprocess.check_output(
                ["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True
            )
        )
        command = [
            "c++",
            "-O3",
            "-DNDEBUG",
            "-std=c++20",
            str(directory / "bench.cpp"),
            "-o",
            str(directory / "bench"),
            *flags,
        ]
        subprocess.run(command, check=True)
        result = subprocess.run(
            [
                "dbus-run-session",
                "--",
                str(directory / "bench"),
                str(args.iterations),
                str(args.rounds),
            ],
            env=isolated_environment(directory / "profile"),
            check=True,
            text=True,
            capture_output=True,
        )
    samples = {"regex": [], "ascii": []}
    checksums = set()
    for line in result.stdout.splitlines():
        kind, duration, checksum = line.split()
        samples[kind].append(int(duration))
        checksums.add(checksum)
    assert len(checksums) == 1
    medians = {name: statistics.median(values) for name, values in samples.items()}
    improvement = (1 - medians["ascii"] / medians["regex"]) * 100
    report = {
        "source": str(args.source.resolve())
        if args.source
        else f"{args.baseline_ref}:plugin/costengine.cpp",
        "iterations": args.iterations,
        "rounds": args.rounds,
        "compileCommand": command,
        "parity": result.stderr.strip(),
        "samplesNs": samples,
        "medianNs": medians,
        "improvementPercent": improvement,
        "meetsTenPercentGate": improvement >= 10,
        "scope": "parseScaled string-rate microbenchmark only",
    }
    rendered = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    print(rendered, end="")


if __name__ == "__main__":
    main()
