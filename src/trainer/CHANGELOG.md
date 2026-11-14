# Trainer changelog

Every change to the trainer (`src/trainer`: train.py, gamedata.py, model.py, netfile.py, export.py,
ranger_lite.py, native/) gets an entry here and a new version in `version.py`, in the same commit.

## Versions: major.minor.patch

- **patch** - the trainer writes **bit-identical nets** under every setting. Proven by
  `test_reproducible.py` against `reproducible.json`: it must report `identical`. Refactorings,
  speed, logging, new checks that only stop a run, renamed files, fewer nets written.
- **minor** - the nets change under at least one setting: a new option (even off by default), a fix,
  another default, another order of the data. After the change `test_reproducible.py --record`
  writes the new record, and the entry says which runs of the test changed.
- **major** - the procedure changes in principle, or the net file format changes.

Every file a training writes carries the version in its name: `net-v2.2.1-epoch09.nnue`,
`state-v2.2.1-epoch100.pt`; `last.pt` holds it inside. `--resume` continues only a run of the same
major.minor.

The test runs on the CPU, where two trainings with the same seed give the same bits. On the GPU (mps)
they do not, so a patch is proven on the CPU and a GPU training has a spread of its own.

## The history, versioned afterwards (2026-10-07)

The versions up to 2.2.0 were assigned on 2026-10-07 from `compare_trainers.py`: every trainer commit
trained the fixture on the CPU against its predecessor, with the commands both understand; the results
are in `trainer-comparisons.md`. "identical"/"different" below quote that file.

**Before 0.9.0 nothing is reproducible.** Those trainers did not draw the initial weights from the
seed: a trainer compared with itself gives different nets (171fc0a, b396152 measured), so no two of
them can be compared either. Their versions follow the rules from the commit, not from a measurement.

| version | commit | date | change | evidence |
|---|---|---|---|---|
| 0.1.0 | 9a92c6b | 09-26 | the loader and the trainer | first |
| 0.1.1 | 2891065 | 09-26 | the loss of a batch read without the graph | meant to change nothing; not measurable |
| 0.2.0 | 9b9e406 | 09-26 | the game result always has a weight | changes the loss |
| 0.3.0 | 9db9560 | 09-26 | the value is a win probability | changes the target |
| 0.4.0 | 6172221 | 09-26 | the cache takes several game files | new option |
| 0.5.0 | 7f17f39 | 09-27 | a validation set and the stopping rule | new behaviour |
| 0.6.0 | 9c967d8 | 09-28 | reads the packed game file directly | new loader |
| 0.6.1 | 171fc0a | 09-28 | writes games back out as pgn (export-pgn.py only) | training code untouched |
| 0.7.0 | b6588b6 | 09-29 | several game files as one corpus | new behaviour |
| 0.8.0 | b396152 | 09-30 | --resume | new option |
| 0.9.0 | 52dc531 | 10-01 | compiled loader; initial weights from the seed | different from 0.8.0; **first reproducible** (identical to itself) |
| 1.0.0 | ca34193 | 10-01 | --stacks, net format QAPLANN2 | major: format; one head identical to 0.9.0 |
| 1.1.0 | a3f6128 | 10-02 | --neighbours | new option; identical to 1.0.0 with one and with eight heads |
| 1.1.1 | bded1e1 | 10-02 | pgn exporter: promotions | training code untouched |
| 1.2.0 | 794a3d0 | 10-04 | --skip-tactical | new option; identical to 1.1.0 without it |
| 1.3.0 | 2eb17be | 10-04 | --epoch-size: Stockfish's procedure as an option | new option; identical to 1.2.0 without it (also with 8 heads and skip) |
| 1.4.0 | 70ba1e9 | 10-05 | skipping captures and checks by default | different by default; identical with --skip-tactical |
| 1.4.1 | 5762614 | 10-05 | a net every 100 epochs by default | fewer nets written; the nets identical |
| 2.0.0 | 61ec5c1 | 10-05 | Stockfish's procedure the default | major: procedure; identical with --epoch-size 0, different with --epoch-size (RangerLite default) |
| 2.1.0 | 13fa48f | 10-05 | --skip-early | new option; identical without it |
| 2.2.0 | ad3792b | 10-05 | --accumulator, --l1 | new option; identical at the default sizes, same file format there |
| 2.2.1 | 6064f86 | 10-07 | versioned file names, version printed, --resume checks it; test_reproducible.py | identical to 2.2.0 (test, and compare_trainers) |

`1039c45` (09-28, convert.py) changes how a pgn becomes a game file - the data, not the trainer.

## Which nets came from which version

A net is ascribed to the last trainer commit before its training started; whether the working copy was
clean is not recorded for any of them.

| net | trained | version | learnings resting on it |
|---|---|---|---|
| nets-set1-nowdl e08 | 09-28 | 0.6.1 | 2, 4, 5, 6 (baseline) |
| set1234 best (e11) | 09-30 | 0.7.0 | 3 (baseline), reference field |
| set123456-s8 e14 | 10-01 | 1.0.0 | 3, 7 (baseline) |
| set1-sf | 10-04 | 1.1.1 | 4 |
| set1-lowsel e09 | 10-04 | 1.1.1 | 6 |
| set123456-s8-skip e09 | 10-04 | 1.2.0 | 7, 8 (baseline) |
| s8-sflike e300-e800 | 10-05 | 1.3.0 | 8 |
| A1, set1-nolmr, set1-hce-retrain | 10-06/07 | 2.2.0 | - |

What the comparisons say about the learnings: 7 (s8 1.0.0 against s8-skip 1.2.0, s8 command) and 8
(s8-skip 1.2.0 against sflike 1.3.0, s8-skip command) - the trainers are identical under the baseline's
command, so each compared exactly one factor. 3, 4, 5, 6 compared nets from a 0.x trainer with nets
from a 1.x trainer - two factors, and the 0.x side not even reproducible.

## 6.1.1 - 2026-11-14

Every checkpoint (`last.pt`, the `state-*` kept every 100 epochs, the `.pt` of the old procedure) is written to a
temporary file and renamed into place. Trainings on the Studio now hold the GPU lock as interruptible runs
(`~/dev/sglang/gpu-lock`, profile `schach-training`), which ends them with SIGTERM at any moment when an image or video
job waits; a stop in the middle of a save left half a `last.pt` before. Patch: `test_reproducible.py` identical to 6.1.0
in every run but `fixed-ranger-moe-layers` (not reproducible, see 6.1.0).

## 6.1.0 - 2026-11-14

`--future-weight W` (Volker: a bucket learns from the current pieces only and never sees how its positions go on once a
few more pieces are gone). With `--stacks` and `--epoch-size`: every position trains its own head and, with weight W, the
head one stack up (more pieces), for which it is the future of its game - head k learns from bucket k and, at W, from
bucket k - 1. The loss is `neighbour_loss_of` with the weights 0 below and W above (`--neighbours` now passes its weight
for both sides, unchanged); `all_heads` takes the batch's csr, so the fast feature transformer runs as before. The held
back loss is still measured with the own head alone; the loss printed for training includes the second head. The engine
is untouched. Not with `--psqt`, `--moe`, `--neighbours`.
- Without `--future-weight` identical to 6.0.0: `test_reproducible.py`, every run but `fixed-ranger-moe-layers`.
- `fixed-ranger-moe-layers` is not reproducible, already at clean 6.0.0: three runs, three different nets at epoch 2
  (the 6.0.0 record, 3b333bed, ffa6f83e). Not followed up - MoE is not trained again without a solution for the jumps
  in the evaluation (Volker 2026-11-14). Its recorded value is therefore meaningless.
- One run added (`fixed-ranger-8heads-future`, W 0.75), record rewritten.

## 6.0.0 - 2026-11-12

`--moe-mix sum|layers` (Volker: MoE models choose in every layer; 2 x 16 -> 16 with weighted experts). `layers`: experts in
all three layers, 16 each - 512 -> 16, 16 -> 32, 32 -> 1 - and in each layer the two of the highest logits computed and
mixed by their weights, g1 = sigmoid(l1 - l2) for the higher logit, g2 = 1 - g1. The 3 x 16 logits are accumulator values
(own plus opponent value k of the layer's 16): the accumulator is 256 + 48 wide and learns them directly, no router
layer - a router on accumulator values was only another linear map. The forward value is the mix, with the exact
gradient; with `--moe-share full` the chosen experts' share of the softmax over all 16 is added straight through, so a
chosen pair that is wrong together loses share against the others. The balancing term covers all three choices, the
`experts` and `switched` reports follow the choices (switched: any of the three pairs). `sum` (5.x) stays the default.
In the engine (build with `-DQAPLA_NNUE_MOE_LAYERS`, accumulator 304) g1 comes from a table over the logit difference (32
steps a logit unit, 256 entries, scale 128) and the mix is (g1 x + (128 - g1) y) >> 7 in every layer - the same table
and integers in `netfile.evaluate_moe_layers`. Major: a new net file format, `QAPLANN6`.
- Without `--moe-mix layers` identical to 5.1.0: `test_reproducible.py`, all fourteen runs.
- One run added (`fixed-ranger-moe-layers`), record rewritten.
- Trainer and engine agree to the integer on 300 positions of a small net of the format, vector and plain path equal
  (found on the way: the mix computed unsigned, the constants living in an enum of uint32_t - every negative output
  value came out wrong). The QAPLANN5 build reads q1234-02abcd-03a-moe-no16-b75 as before; the normal build 207,841,327
  nodes on wmtest depth 18. Quantized against float on that small net: 11.7 apart on average, 7.7 the median.

## 5.1.0 - 2026-11-12

`--moe-share pair|full` (Volker: "baue ein bestrafungsverfahren das funktioniert"). 5.0.0 renormalized the chosen
experts' softmax share over the two of them (`pair`, still the default): the gradients of their two logits are then
equal and opposite, only their difference is trained - two chosen experts that are both wrong the same way get no
gradient at all, and no third one ever moves up. `full` takes their share of the softmax over all 16 (Switch
Transformer): a chosen expert that hurts loses share against every other one, its logit falls and the others rise
until one overtakes it. Checked on four logits, both chosen harmful: `pair` moves no logit, `full` lowers the two
chosen and raises the others.
With `--moe` and held back data every epoch line now ends with `switched x %`: the share of 16,384 fixed held back
positions whose pair of experts differs from the epoch before - the probe reads the validation set and changes no
net.
- With the default `pair` identical to 5.0.0: `test_reproducible.py`, all thirteen runs (the probe included).
- One run added (`fixed-ranger-moe-full`), record rewritten.
- Minor: a new option; the net format is unchanged.

## 5.0.0 - 2026-11-12

`--moe` (Volker): a mixture of experts in place of the layer stacks. The accumulator gets 16 more values per
perspective; a router (32 -> 16, the 16 values of both perspectives, own first, unclipped) picks the top 2 of 16
experts of 16 first-layer neurons each (512 -> 16), so 32 neurons are computed as with the stacks. Every expert has
its own 16 columns of the second layer (shared 256 -> 32): the two chosen experts' contributions are summed into it,
then 32 -> 1. Forward exactly as the engine: the chosen experts count one each; the router learns through a
straight-through factor, the chosen experts' renormalized softmax share (value one, its gradient). `--moe-balance`
(default 1e-3) weighs the Switch Transformer balancing term (16 x sum of choice fraction x mean share); 1e-4 let
three experts take 75 % on a test, 1e-3 spread them over 4-15 %, 1e-2 forced them even. The loss printed is that of
the values alone; every epoch line gives each expert's share in per cent. Router weights are int16 of scale 1024
(int8 of scale 64 flipped one choice in five), clamped to +-8. Major: a new net file format, `QAPLANN5`
(`netfile.write_moe`). Only with `--epoch-size`, `--stacks 1`, no `--psqt`, `--sqr`, `--neighbours`, default sizes.
- Without `--moe` identical to 4.0.0: `test_reproducible.py` against the 4.0.0 record, all twelve runs.
- One run added (`fixed-ranger-moe`, balance 0.002), record rewritten.
- The engine reads the format in a build with `EXTRA_DEFINES="-DQAPLA_USE_NNUE -DQAPLA_NNUE_MOE"` (accumulator 272
  wide; a normal build refuses the net, the MoE build every other format): trainer and engine agree to the integer
  on 300 positions of a small MoE net, vector path and plain path equal. The normal build is unchanged: wmtest
  depth 18 with the embedded net, 207,841,327 nodes.
- On a net of three short epochs the quantized router picked other experts than the float one in 39 of 300
  positions (near ties of a young router); with the same choice the value differs by 5 on average, 20 at most.

## 4.0.0 - 2026-11-11

`--sqr` (Volker): every output x of the first dense layer activated twice, as Stockfish does -
`min(x^2, 1) * 127/128` and `clamp(x, 0, 1)`, the squares first - so the second layer reads 2 x 32 inputs.
The square drops the sign: an output below zero, which the clipped relu turns into nothing, still reaches the
next layer. 127/128 is what the engine's integer square comes to, `(v*v) >> 7` at the scale QA (Stockfish's
SqrClippedReLU). Major: a new net file format, `QAPLANN4` - the stacked format with a second layer of 64
inputs a row; no piece-square part (`--psqt` and `--sqr` refuse each other).
- Without `--sqr` identical to 3.3.0: `test_reproducible.py` against the 3.3.0 record, all nine runs.
- Three runs added (`fixed-ranger-1head-sqr`, `fixed-ranger-8heads-sqr`, `pass-adam-8heads-neighbours-sqr`:
  one head, the stacked forward and all_heads), record rewritten.
- The engine reads the new format (same commit): trainer and engine agree to the integer on 400 positions of
  a small sqr net (8 stacks), the first layer's values from -166 to 163, so both cuts are reached; vector path
  and plain path equal. For the old formats the engine is unchanged: wmtest depth 18 with the embedded net
  (q1234-02abcd-03a-no16-b75), 207,841,327 nodes before and after.

## 3.3.0 - 2026-10-18

`--blend-buckets b0,...,b7` (Volker): one blend per bucket for the whole run - the bucket of the layer stack,
(pieces - 1) // 4, bucket 0 the fewest pieces, whatever `--stacks` is - in training and for the held back loss;
it replaces `--blend-start`/`--blend-end`, only with `--epoch-size`. The idea: the game result says more the
nearer a position is to the end of the game, e.g. `0.69,0.72,0.75,0.78,0.81,0.84,0.87,0.90` (31 % result in the
endgame, 10 % in the opening, 0.795 on average).
- Without the option the nets are unchanged: `test_reproducible.py` identical to 3.1.0 in all runs before the
  new ones.
- Eight equal blends give the net of the one blend, bit for bit (0.8 x 8 against `--blend-start 0.8 --blend-end
  0.8`, fixture with results, 8 stacks, CPU); 0.69-0.90 gives another net.
- **The test did not reach the game result before:** its fixture is set 1 without results, so no blend ever
  changed a net in it. `RUNS_WDL` now train the same 2000 games with their results (`fixture-wdl.gam`): the blend
  as one number and by bucket. Recorded with this version.

## 3.2.0 - 2026-10-15

Fast training on the gpu of a Mac (Volker: the trainer has to use the Mac Studio). Three changes:
- `fastft.py`: the feature transformer on two own Metal kernels instead of EmbeddingBag, both perspectives in one
  call. The backward pass sums, per weight, over the positions holding its feature - the loader groups each batch
  by feature (`batcher --csr`, a counting sort), so no atomics. Forward bit-identical to EmbeddingBag; the
  gradient adds the same terms in another order.
- the loss is summed on the device and read only when printed - no host wait after every batch.
- the csr batches are copied to the gpu without blocking, so the next batch is prepared while one runs.
`--fast-ft auto|on|off`, auto = on for mps in the fixed-epoch mode. Mac Studio, sets 1-6, 20 M an epoch: 9 s an
epoch against 17.5 s (1.9x), loss at epoch 5 0.001872 against 0.001870. M4: 11 s against 15.5 s.
Minor: nets on mps change (another summation order; mps was never bit-reproducible anyway). The cpu keeps
EmbeddingBag: `test_reproducible.py` identical to 3.1.0 in all runs.

## 3.1.0 - 2026-10-10

Captures and checks are always skipped; `--skip-tactical` / `--no-skip-tactical` are gone, and with them
`--loader python` (the python loader cannot skip). Volker: a test with those positions makes no sense any more.
Minor: options removed. Runs at the defaults are bit-identical to 3.0.0 (`pass-adam-8heads-neighbours`,
`fixed-ranger-1head`, `fixed-ranger-1head-psqt`, `fixed-ranger-resumed`); the runs that used the removed options
were changed to skip (`pass-adam-1head`, `fixed-adam-8heads-512x16`, `pass-adam-8heads-psqt`) or dropped
(`pass-python-loader`), and the record rewritten.

## 3.0.0 - 2026-10-09

`--psqt`: the piece-square part as Stockfish has it (plan/run-goals.md, "Architecture PSQT"). Every
feature carries, besides its accumulator column, one value per bucket; the bucket is chosen like the
layer stack, by the number of pieces; half the difference of the two perspectives' sums goes straight to
the output. The values start from the material (pawn 90, knight 300, bishop 310, rook 480, queen 930
engine units, positive for the own pieces) and are not clamped. Major: a new net file format,
`QAPLANN3` - the stacked format with PSQT_BUCKETS int32 per feature between the feature weights and the
heads, of the output scale QA*QB.

- Without `--psqt` identical to 2.2.1: `test_reproducible.py` against the 2.2.1 record, all six runs.
- Two runs added to the test (`fixed-ranger-1head-psqt`, `pass-adam-8heads-psqt`), record rewritten.
- The engine reads the new format (same commit); trainer and engine agree to the integer on 202
  positions of a small psqt net, vector path and plain path equal, and a search with
  QAPLA_VERIFY_NNUE_INCREMENTAL over wmtest at depth 10 found no difference of the incremental sums.
  For the old formats the engine is unchanged: wmtest depth 18 with s1-sflike e800, 194,484,867 nodes
  before and after, runtime +2.4 % (mean of four runs each).

## 2.2.1 - 2026-10-07

First version kept by these rules (named 1.0.0 for a few hours on 2026-10-07 before the history was
numbered; the files written then carry `v1.0.0` and are the same trainer).

- The version is printed at the start and goes into every file name; `--resume` refuses a checkpoint
  of another major.minor, or one from before versioning. `test/nnue/nets-a1/last.pt` (A1, started
  2026-10-06 with 2.2.0) was stamped by hand; the unstamped original is `last.pt.before-1.0.0`.
- `test_reproducible.py` and its record `reproducible.json`; `compare_trainers.py` and its record
  `trainer-comparisons.md`.
