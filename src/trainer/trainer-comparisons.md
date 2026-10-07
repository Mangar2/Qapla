# Trainer comparisons

Written by compare_trainers.py: two commits, one command, the fixture of test_reproducible.py on the CPU.

| A | B | command | result | time A / B |
|---|---|---|---|---|
| ad3792b | ad3792b | `--epoch-size 0 --epochs 2 --workers 2 --no-skip-tactical` | identical (2 nets) | 33 s / 32 s |
| ca34193 | 794a3d0 | `--blend-start 0.8 --blend-end 0.7 --epochs 2 --patience 2 --workers 6 --seed 1 --validation-every 100 --stacks 8` | identical (2 nets) | 33 s / 15 s |
| 794a3d0 | 2eb17be | `--blend-start 0.8 --blend-end 0.7 --epochs 2 --patience 2 --workers 6 --seed 1 --validation-every 100 --stacks 8 --skip-tactical` | identical (2 nets) | 11 s / 16 s |
| b6588b6 | ca34193 | `--blend-start 0.8 --blend-end 0.7 --epochs 2 --patience 2 --workers 6 --seed 1 --validation-every 100` | different from epoch 1 on | 92 s / 32 s |
| 9c967d8 | 171fc0a | `--epochs 2 --workers 2` | different from epoch 1 on | 86 s / 85 s |
| 171fc0a | b6588b6 | `--epochs 2 --workers 2` | different from epoch 1 on | 86 s / 87 s |
| b6588b6 | b396152 | `--epochs 2 --workers 2` | different from epoch 1 on | 88 s / 89 s |
| b396152 | 52dc531 | `--epochs 2 --workers 2` | different from epoch 1 on | 89 s / 36 s |
| 52dc531 | ca34193 | `--epochs 2 --workers 2` | identical (2 nets) | 34 s / 34 s |
| ca34193 | a3f6128 | `--epochs 2 --workers 2` | identical (2 nets) | 34 s / 14 s |
| ca34193 | a3f6128 | `--epochs 2 --workers 2 --stacks 8` | identical (2 nets) | 14 s / 19 s |
| a3f6128 | 794a3d0 | `--epochs 2 --workers 2 --stacks 8` | identical (2 nets) | 14 s / 20 s |
| 794a3d0 | 2eb17be | `--epochs 2 --workers 2` | identical (2 nets) | 36 s / 34 s |
| 794a3d0 | 2eb17be | `--epochs 2 --workers 2 --stacks 8 --skip-tactical` | identical (2 nets) | 29 s / 29 s |
| 2eb17be | 70ba1e9 | `--epochs 2 --workers 2` | different from epoch 1 on | 34 s / 28 s |
| 2eb17be | 70ba1e9 | `--epochs 2 --workers 2 --skip-tactical` | identical (2 nets) | 27 s / 28 s |
| 2eb17be | 70ba1e9 | `--epochs 2 --workers 2 --epoch-size 100000 --skip-tactical` | identical (1 nets) | 15 s / 15 s |
| 70ba1e9 | 5762614 | `--epochs 2 --workers 2 --epoch-size 100000` | identical (1 nets) | 15 s / 17 s |
| 70ba1e9 | 5762614 | `--epochs 2 --workers 2 --epoch-size 100000 --save-every 1` | identical (2 nets) | 15 s / 15 s |
| 5762614 | 61ec5c1 | `--epochs 2 --workers 2` | different from epoch 1 on | 29 s / 1326 s |
| 5762614 | 61ec5c1 | `--epochs 2 --workers 2 --epoch-size 0` | identical (2 nets) | 29 s / 29 s |
| 5762614 | 61ec5c1 | `--epochs 2 --workers 2 --epoch-size 100000 --save-every 1` | different from epoch 1 on | 16 s / 20 s |
| 61ec5c1 | 13fa48f | `--epochs 2 --workers 2 --epoch-size 0` | identical (2 nets) | 30 s / 32 s |
| 61ec5c1 | 13fa48f | `--epochs 2 --workers 2 --epoch-size 100000 --save-every 1` | identical (2 nets) | 23 s / 21 s |
| 13fa48f | ad3792b | `--epochs 2 --workers 2 --epoch-size 0` | identical (2 nets) | 29 s / 29 s |
| 13fa48f | ad3792b | `--epochs 2 --workers 2 --epoch-size 100000 --save-every 1 --stacks 8` | identical (2 nets) | 22 s / 21 s |
| ad3792b | 6064f86 | `--epochs 2 --workers 2 --epoch-size 0` | identical (2 nets) | 31 s / 28 s |
| ad3792b | 6064f86 | `--epochs 2 --workers 2 --epoch-size 100000 --save-every 1` | identical (2 nets) | 8 s / 8 s |
| 171fc0a | 171fc0a | `--epochs 2 --workers 2` | different from epoch 1 on | 65 s / 86 s |
| b396152 | b396152 | `--epochs 2 --workers 2` | different from epoch 1 on | 96 s / 97 s |
| 52dc531 | 52dc531 | `--epochs 2 --workers 2` | identical (2 nets) | 39 s / 38 s |
