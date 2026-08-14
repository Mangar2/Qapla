#!/usr/bin/env python3
"""
Sequential A/B runtime test for two Qapla binaries.

Alternates between A and B until it is established, at the required confidence,
whether A is faster, B is faster, or neither can be shown. No fixed measurement
budget, no stopping by gut feeling.

Principles that shape the design:

* The unit of observation is the PAIR, never the single run. An A run is always
  compared against an immediately adjacent B run.
* Outlier detection works on pairs, and on the pair difference d_i rather than on
  the absolute times. The reason is substantive: the difference cancels the
  common level. If the machine shifts by half a second in the middle of a series,
  every later time deviates from the global median even though the pairs are
  sound - a filter on absolute times discards everything from that point on and
  the test never finishes. A hiccup in ONLY one half, by contrast, shows up in
  full in d_i and therefore removes the whole pair, which is what it should do.
  Half a pair would no longer be a comparison.
* The warm-up phase is discarded not because it comes first, but because its
  times deviate too far. It falls under the same rule as any other outlier.
* The order within a pair is swapped from pair to pair. That is a precaution,
  not a requirement: --order fixed shows that the result does not hinge on it.
* The node count of every run is checked. If two binaries differ there, they are
  not computing the same thing and their runtimes are not comparable - the test
  aborts instead of producing a number.

The decision is an SPRT over the relative pair differences d_i = (t_B - t_A)/t_A,
with two symmetric hypotheses, like the Elo bounds of a game SPRT:

    H0: mu = -bound  (B is faster)      H1: mu = +bound  (B is slower)

If either is accepted, the direction is settled. If the truth lies in between,
the log-likelihood ratio wanders between the boundaries and the test runs to
--max-pairs. That outcome means "undecided", NOT "equally fast": an SPRT has no
"stayed the same", only "no difference could be shown". A change without proof
does not stay.
"""

import argparse
import math
import re
import statistics
import subprocess
import sys
import time

TIME_RE = re.compile(r"Total runtime:\s*elapsed\s*=\s*(\d+):([\d.]+)")
NODE_RE = re.compile(r"total nodes:\s*(\d+)")


def run_once(qet, binary, epd, depth):
    """One measurement run. Returns (seconds, node count)."""
    cmd = [qet, "--concurrency=1", "--each", "proto=uci",
           "--epd", f"file={epd}", f"depth={depth}",
           "--engine", "name=x", f"cmd={binary}"]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    t = TIME_RE.search(out)
    n = NODE_RE.search(out)
    if not t or not n:
        raise RuntimeError(f"Could not read runtime/node count for {binary}")
    return int(t.group(1)) * 60 + float(t.group(2)), int(n.group(1))


def mad(values, centre):
    """Median absolute deviation, scaled to the sigma of a normal distribution."""
    if len(values) < 2:
        return 0.0
    return 1.4826 * statistics.median([abs(v - centre) for v in values])


def classify_pairs(pairs, k, min_for_outliers):
    """
    Split the pairs into valid and discarded ones, based on the pair difference d.

    The median of the differences is the robust level; a pair drops out when its
    difference lies farther than k robust sigma away from it. That makes the
    filter insensitive to drift and level shifts of the machine (those cancel in
    d) and it hits exactly what it should hit: pairs in which one half stumbled.
    """
    if len(pairs) < min_for_outliers:
        return list(pairs), []

    for p in pairs:
        p.pop("reason", None)   # no pair keeps a verdict from an earlier round
    diffs = [p["d"] for p in pairs]
    med = statistics.median(diffs)
    spread = mad(diffs, med)
    if spread <= 0:
        return list(pairs), []

    good, bad = [], []
    for p in pairs:
        if abs(p["d"] - med) > k * spread:
            p["reason"] = f"d={p['d']:+.2f}% vs median {med:+.2f}%"
            bad.append(p)
        else:
            good.append(p)
    return (good, bad) if good else (list(pairs), [])


def sprt_llr(diffs, bound, sigma):
    """
    Log-likelihood ratio of H1(mu=+bound) against H0(mu=-bound), normal model.
    Positive means the data favour "B slower", negative "B faster".
    """
    if sigma <= 0:
        return 0.0
    return sum(((d + bound) ** 2 - (d - bound) ** 2) for d in diffs) / (2.0 * sigma * sigma)


def main():
    # line buffered output: a test can run for hours, progress has to be visible
    sys.stdout.reconfigure(line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--a", required=True, help="binary A (reference)")
    ap.add_argument("--b", required=True, help="binary B (new)")
    ap.add_argument("--bound", type=float, default=0.3,
                    help="hypotheses in percent: H0 = -bound (B faster), H1 = +bound (B slower)")
    ap.add_argument("--alpha", type=float, default=0.05, help="type I error (default 0.05)")
    ap.add_argument("--beta", type=float, default=0.05, help="type II error (default 0.05)")
    ap.add_argument("--min-pairs", type=int, default=12, help="no decision before this count")
    ap.add_argument("--max-pairs", type=int, default=120, help="upper limit; undecided afterwards")
    ap.add_argument("--outlier-k", type=float, default=3.0, help="outlier threshold in robust sigma")
    ap.add_argument("--order", choices=["alternate", "fixed"], default="alternate",
                    help="order within a pair: alternating (default) or always A first")
    ap.add_argument("--qet", default=None, help="path to qet (default ~/bin/qet)")
    ap.add_argument("--epd", default="test/epd/wmtest.epd")
    ap.add_argument("--depth", type=int, default=16)
    args = ap.parse_args()

    qet = args.qet or f"{subprocess.os.path.expanduser('~')}/bin/qet"
    upper = math.log((1 - args.beta) / args.alpha)
    lower = math.log(args.beta / (1 - args.alpha))

    print(f"A = {args.a}")
    print(f"B = {args.b}")
    print(f"H0 = {-args.bound:+.2f} % (B faster), H1 = {args.bound:+.2f} % (B slower), "
          f"alpha {args.alpha}, beta {args.beta}")
    print(f"SPRT bounds [{lower:.2f}, {upper:.2f}], at most {args.max_pairs} pairs, "
          f"order {args.order}")
    print()

    pairs = []
    expected_nodes = None
    verdict = None
    start = time.time()

    while len(pairs) < args.max_pairs and verdict is None:
        idx = len(pairs)
        a_first = True if args.order == "fixed" else (idx % 2 == 0)
        if a_first:
            ta, na = run_once(qet, args.a, args.epd, args.depth)
            tb, nb = run_once(qet, args.b, args.epd, args.depth)
        else:
            tb, nb = run_once(qet, args.b, args.epd, args.depth)
            ta, na = run_once(qet, args.a, args.epd, args.depth)

        if expected_nodes is None:
            expected_nodes = na
        if na != nb or na != expected_nodes:
            print(f"ABORT: node counts differ (A={na}, B={nb}, expected {expected_nodes}).")
            print("The two binaries are not computing the same thing, their runtimes are not comparable.")
            return 2

        pairs.append({"i": idx + 1, "a": ta, "b": tb,
                      "d": (tb - ta) / ta * 100.0, "first": "A" if a_first else "B"})

        # The classification is not static: with every measurement the whole list is
        # judged again, and pairs can change their status in either direction.
        good, bad = classify_pairs(pairs, args.outlier_k, min_for_outliers=5)
        diffs = [p["d"] for p in good]
        n = len(diffs)
        mean = statistics.fmean(diffs) if n else 0.0
        sigma = statistics.stdev(diffs) if n > 1 else 0.0

        llr = sprt_llr(diffs, args.bound, sigma)

        p = pairs[-1]
        print(f"Pair {p['i']:3d} ({p['first']} first): A={p['a']:.3f}s B={p['b']:.3f}s "
              f"d={p['d']:+.2f}%  | valid {n}, discarded {len(bad)}, "
              f"mean {mean:+.2f}%, LLR {llr:+.2f}")

        if n >= args.min_pairs and sigma > 0:
            if llr >= upper:
                verdict = ("B is slower", f"H1 accepted ({args.bound:+.2f} %)")
            elif llr <= lower:
                verdict = ("B is faster", f"H0 accepted ({-args.bound:+.2f} %)")

    good, bad = classify_pairs(pairs, args.outlier_k, min_for_outliers=5)
    diffs = [p["d"] for p in good]
    mean = statistics.fmean(diffs) if diffs else float("nan")
    sigma = statistics.stdev(diffs) if len(diffs) > 1 else float("nan")
    err = sigma / math.sqrt(len(diffs)) if len(diffs) > 1 else float("nan")

    print()
    if bad:
        print("Discarded pairs (one half too far from the median of its group):")
        for p in bad:
            print(f"  Pair {p['i']:3d}: A={p['a']:.3f}s B={p['b']:.3f}s  ({p['reason']})")
    print(f"Valid pairs: {len(diffs)}   Test runtime: {(time.time()-start)/60:.1f} min")
    print(f"Mean difference B against A: {mean:+.2f} %  (+- {1.96*err:.2f} % at 95 %)")
    if verdict:
        print(f"RESULT: {verdict[0]} - {verdict[1]}")
    else:
        print(f"RESULT: undecided after {args.max_pairs} pairs - neither H0 nor H1 accepted.")
        print("That is not proof of equality but lack of knowledge: the change does not stay.")
        print("To show equality, run tighter (smaller --bound). If it stays undecided even "
              "then, it is a case-by-case decision whether the uncertainty is acceptable.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
