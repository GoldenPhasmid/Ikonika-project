# UI layouts

Standalone HTML/CSS mockups for Ikonika screens, built ahead of the real QML. Not wired into
the CMake build or the app in any way — open the `.html` files directly in a browser.

Each `fragment-*.html` is one screen or component: decks, dictionary, schedule/today, exercises,
button states, word detail. `fragment-template.html` is the empty starting point for a new one.
`work.html` is a scratch page. `Lexicon.dc.html` is a design-comp export, kept for reference.

## style.css is the shared design language

`style.css` holds every token and component style shared across fragments: colours, spacing,
typography, card/panel shells, button and answer-option states, and so on. It is meant to become
the QML design system later, so it has to stay the single source of truth.

**Rule:** any newly designed element with real meaning — a new component, a new interaction state,
a new colour or spacing token — goes into `style.css`, not inline in one fragment. Inline
`<style>` blocks inside a fragment file are only for page mechanics that have no design meaning,
such as centering the mockup on the page.

## How we iterate

1. Edit the HTML/CSS directly.
2. Open (or reload) the file in a browser — usually by dragging it in, or keeping a `file://` tab
   open.
3. Share a screenshot of what it looks like. Visual feedback from a screenshot is cheap and far
   more reliable than reasoning about layout from raw CSS, so default to asking for one.
4. Repeat until the fragment is right, then fold anything reusable back into `style.css`.
