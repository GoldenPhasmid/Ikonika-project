# V3 rules

Constraints every fragment and every rule in `wireframe.css` has to hold. A drawing that breaks
one of these is wrong even when it looks right, because the QML built from it inherits the fault.

## Type

**Minimum font size is 12px.** Nothing renders text below it, including monospace captions,
numbered badges and uppercase kickers. Tracked uppercase mono at 10px is the worst case in the
sheet: the letterforms are small, the tracking pulls them apart, and the colour is already the
quietest step in the palette. Under 12px the label stops being read and starts being decoration.

The rule lives in the sheet rather than in each fragment. Every caption on Today, in the rail, in
the session shell and in the setup screens comes from `.kicker`, `.meta` or a component rule, so
one value per rule holds the whole deck.

`.scaffold-label` in `master.html` is exempt. It labels the frames of the contact sheet and is not
part of any screen.
