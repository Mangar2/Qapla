# Training the net

Five steps, each one a file the next one reads:

| step | command | result |
|---|---|---|
| positions | `nnuebook leaves <n> out <book>` | a library of start positions |
| games | `nnuegames book <book> out <games> threads <n>` | the played games, three bytes a move |
| cache | `python3 prepare.py <games> <cache prefix>` | the feature indices, ready to train |
| train | `python3 train.py <cache prefix> --out <dir>` | a checkpoint and a net per epoch |
| play | `nnueeval net <net>` | the engine's value of a position |

The first two steps are commands of the statistics interface of the engine, run as
the first word on its standard input. The others need Python.

## What must not drift

Three things have a copy on both sides, and every one of them is a silent error if
the copies disagree - the net still produces numbers, they are just not the numbers
it was trained to produce.

* **The feature index.** `src/nnue/nnue-features.h` and `features()` in
  `format.py`. The order is ours: piece plane `(piece - WHITE_PAWN) ^ perspective`
  with the own king left out, eleven planes, index
  `(king * 11 + plane) * 64 + square`. No other engine uses this order and none
  has to - a feature index is a column of the weight matrix, and only these two
  places decide which column a piece gets.
* **The quantization.** `nnue-arch.h` and `netfile.py`: QA 127, QB 64, shift 6.
  QA is 127 and not 255 so that an activation fits in a signed byte, which is what
  the dot product instruction of the processor takes.
* **NET_VALUE_SCALE**, 400. The output of the net times 400 is the value in the
  unit of the engine, where a pawn is 80 to 95. It is also the scale of the sigmoid
  in the loss, which is why the loss can be written as `sigmoid(prediction)`.

The shape number in the net file header covers the sizes and the two quantization
scales, so a net of another shape is refused rather than read as noise. It does not
cover the feature order - nothing can, which is why the check below matters.

## Checking that both sides agree

```
python3 verify.py <net file>
printf 'stat\nnew\nnnueeval net <net file>\nquit\n' | ./build/Release/Qapla
```

Both print the value of the start position and both compute in integers, so the two
numbers have to be **equal**. A difference of one is a difference. With the net of
random weights written by `nnuenet` this is a full test of the chain: the feature
indices, the file layout and the arithmetic of every layer.

`nnueeval` evaluates the position the board stands at. The statistics interface
does not set one when it starts, so `new` or `setboard <fen>` has to come first -
without it the value belongs to a position nobody chose.

## The cache

`prepare.py` replays every game once and writes the feature indices, two
perspectives of 32 slots each, padded with the index 45056 that the model treats as
nothing. 128 bytes a position, so the 11.9 million positions of a hundred thousand
games are 1.5 GB - and the training then has no work to do but read memory. Without
the cache every epoch would have to replay the games, because a packed move has no
departure square.

The cache belongs to the feature set it was written with. There is no version in it:
the only honest answer to a stale cache is to write it again, and at 215000
positions a second that is a minute.

## The loss

```
target     = blend * sigmoid(value / 400) + (1 - blend) * result
prediction = sigmoid(net output)
loss       = |prediction - target| ** 2.5
```

Both labels of a position are brought into one win probability. `blend` is the
lambda of the usual formulation, and for a first net trained on the games of a hand
written evaluation it is **1.0**: the value of the search is the whole signal, the
results of those games are too noisy to learn from. From the second generation on
it goes down towards 0.7.

## Environment

`numpy` and `torch` are not installed here, and the Homebrew Python is 3.14, which
is newer than the interpreters PyTorch publishes wheels for. Everything except
`model.py`, `dataset.py`, `train.py` and `export.py` runs on the standard library
alone, which is why the check above works today. For the training:

```
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

If 3.14 has no wheels, a virtual environment on an older interpreter does.
