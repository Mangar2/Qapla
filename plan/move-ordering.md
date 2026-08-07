# Move ordering

Findings from reading the move ordering, in the proposed working order. Bug fixes first,
they change what the later measurements are based on. Every item is its own tag and its own
SPRT run.

- [ ] **SEE removes all attackers of a piece type at once**, not the single piece that
      captures. Pieces behind the removed ones become visible too early, so the exchange
      sees attackers that are not there. Affects the capture ordering of the main search.

- [ ] **History sorting of the silent moves does not sort by history.** The comparison runs
      against a constant instead of the best value found so far, so the result is the last
      move with a positive history, not the best one. The history values do not influence
      the order at all.

- [ ] **Killer moves are tried after all captures.** Usual is before the losing ones.

- [ ] **Losing captures are tried before the killers and before the silent moves.** They are
      only devalued within the capture stage instead of being moved behind the quiet moves.

- [ ] **Captures are ordered by the captured piece alone.** The value of the capturing piece
      does not take part, so a queen taking a pawn and a pawn taking a pawn rank equally.

- [ ] **Promotions without capture rank behind every capture**, as their weight is the value
      of a captured piece and there is none.

- [X] **The quiescence search does not use the static exchange evaluation.** The switch that
      exists there uses a light heuristic instead, and it is switched off. A test with the
      real exchange evaluation has never run.
