# Word forms training

Two trainings teach the forms of words the learner already studies. Conjugations trains verbs, one
tense at a time. Forms trains irregular nouns and adjectives together. Both start from the Today
screen. Only languages that define forms rules, French, Spanish, Portuguese and Italian, get these
trainings; for any other language the buttons are hidden. Italian has Conjugations only: its plurals
change the last vowel ("libro", "libri"), which the regular-form rules do not model, so Forms is hidden.

Italian conjugates in five tenses: presente, passato prossimo, imperfetto, futuro and passato remoto.
The passato prossimo takes "essere" for the verbs of movement and change of state, and their
participle agrees in the plural ("noi siamo andati"). Wiktionary gives a common Italian verb two or
three tables, the later ones with archaic and regional spellings ("so" for "sono"), and lists a
second spelling in the same cell ("debbo" next to "devo"). The import keeps the first table and the
first spelling of each cell, so the trainer shows the standard form.

## Design

Forms used to be cards in fixed decks, with their own ladder and reviews. A verb was reachable only
through its fixed deck. A forms card could outlive its deck and block the word from being offered
again. A session could start with any number of words, and sessions were long.

Progress now lives on the headword, not in a deck.

Pros: a word from any deck can be trained. There is no forms deck to manage and no forms card to
lose. Sessions are short.

Cons: forms are not reviewed. Progress is one mark per word, or per tense for a verb, never per
form.

## Progress

A word has one progress record per kind, and one per tense for a verb.

| State | Meaning |
| --- | --- |
| Not there | Never picked for a training. |
| Waiting | Picked for a training that has not been finished. |
| Learned | Trained. |

A word becomes waiting when a training collects it, before the session runs.

Progress is scoped to the language pair and synced with the rest of the learner's data. It is not a
word card: it does not show in decks, word lists, search, reviews or the learning pool.

The forms themselves are never stored. Every session load rebuilds them from the dictionary, so a
corrected dictionary reaches the learner at the next load.

## Which words are trained

A training takes only studied words: a word card on the ladder or completed, from any deck, whose
part of speech is verb, noun or adjective. Words waiting in the pool are not taken, and neither are
words already waiting or learned here. There is no triage: the learner did not know the word, so
there is no reason to assume they know its forms.

A studied word is matched to its dictionary headword, so a card for an inflected or unaccented
spelling trains the headword's forms: "belle" trains "beau".

| Kind | Taken | Not taken |
| --- | --- | --- |
| Verb | Every person of the tense has a single-word spelling. Regular verbs are included. | A verb missing a person, such as "falloir". A headword of more than one word, such as "s'appeler". |
| Adjective | The feminine or a plural departs from the language's regular rule: beau, heureux, normal. | grand, and in French an adjective with no separate feminine, such as rouge. |
| Noun | The plural or the feminine partner departs from the rule: cheval, œil, vendeur, roi. | chat, temps, ami. A noun with no gender. |

A regular noun or adjective teaches one rule that covers thousands of words, so it gets no card.
Forms are compared to the rule without regard to case: the dictionary lists "Chinoise" under the
noun "chinois", and that noun is still regular.

A noun or adjective is dropped when one of its cells has two competing spellings, or when it has no
plural and its ending does not explain why. Inclusive spellings, "ami·e·s", and the second
masculine form of an adjective, "bel", are ignored.

What counts as regular belongs to the language.

| Rule | French | Spanish |
| --- | --- | --- |
| Regular feminine | Plus "e". A headword ending in "e" stays as it is. | A final "o" becomes "a", a final "a" stays, any other ending takes "a". |
| Regular plural | Plus "s". A word ending in s, x or z stays as it is. | Plus "s" after a vowel, plus "es" otherwise. |
| Adjective with one gender | Gets no card. | Gets a card when its plural is not regular: feliz, felices. |

Portuguese uses the Spanish rules. A nasal or circumflex vowel counts as a vowel, so "irmã, irmãs"
and "mão, mãos" are regular. The plurals a learner has to memorise are the ones the rule does not
reach, and those get a card: "pão, pães", "coração, corações", "viagem, viagens", "animal, animais",
"fácil, fáceis".

A noun's gender is the one most of its meanings have. A plural listed under both genders counts
once: "jefes" under the feminine plural of "jefe" is ignored, so "jefas" is the feminine plural.

Two dictionary gaps are filled by rule. A missing masculine plural is spelled like the singular
when the language keeps that ending unchanged: vieux, vieux. A missing feminine plural is the
feminine with the regular plural: actrice, actrices.

## Starting a training

A training starts only with a full session. Its Today button is dimmed until enough words are in
reach, and a tap names the kind of word to study. The number on the button is the words in reach:
those waiting plus the studied words not yet picked.

Starting takes the waiting words first, oldest first, then fills the session with studied words
picked at random. The session starts at once.

Conjugation sessions fill with irregular verbs before regular ones. The studied verbs are shuffled,
then ordered in three groups, and the session takes from the front.

| Group | Verbs | Order inside the group |
| --- | --- | --- |
| 1 | Irregular verbs | Most frequent first: être, avoir, faire before comprendre. |
| 2 | Irregular verbs whose base verb is learned, waiting, or in the same pick | Most frequent first. |
| 3 | Regular verbs | Shuffled in the present. In a later tense, the order their present was learned. |

A verb is derived from a base verb when its headword is a prefix plus the base, and every form of
the tense is that prefix plus the base's form: comprendre from prendre, devenir from venir, obtener
from tener. A verb that only ends like another, savoir and avoir, is not derived. A compound tense
checks the participle: "compris" is "com" plus "pris". The base counts only in the same tense.

A learner with fewer irregular verbs than the session needs gets regular verbs for the rest. The
same order applies to a later tense, where it only reorders the verbs that are ready, since the
waiting ones come first.

A verb is irregular by the textbook definition of its language, read from the dictionary's forms.

| Language | Irregular verb |
| --- | --- |
| French | A verb of the third group: not an "-er" verb other than "aller", and not an "-ir" verb whose "nous" form ends in "-issons". Spelling changes inside the first group, "payer", "appeler", "manger", do not make a verb irregular. |
| Spanish, Portuguese | A verb whose stem or endings leave the model of its "-ar", "-er" or "-ir" conjugation in any simple tense: a changing stem, "pensar, pienso", a new consonant, "tener, tengo", a different root, "ser", "ir". |

Spanish and Portuguese verbs that only change their spelling to keep a sound stay regular:
"buscar, busqué", "llegar, llegué", "coger, cojo", "leer, leyó", "ficar, fiquei". A verb with only an
irregular participle, "escribir, escrito", stays regular, because the participle is not one of the
simple tenses.

Each training keeps its own cached session. While one exists, the button reads Continue and resumes
it in place of starting a new one.

The table stage shows the translation from the learner's own word card as it is when the session
loads. A running session keeps the translation it loaded. Card editing is off in forms sessions.

## Tenses

A verb is learned one tense at a time. Each tense of a verb has its own progress, a session never
mixes tenses, and every screen of a verb session names its tense.

Opening Conjugations shows a sheet listing the tenses, unless a cached session resumes. The present
takes new studied verbs. A later tense opens for a verb once its present is learned, and its row
shows how many verbs are ready. A later-tense session takes the verbs already waiting in that tense
first, then ready verbs.

| Tense (French) | Opens from | Source of the forms |
| --- | --- | --- |
| présent | studied verbs | Stored in the dictionary. |
| imparfait | présent | Stored in the dictionary. |
| futur | présent | Stored in the dictionary. |
| passé composé | présent | Built: the present of avoir or être, then the past participle. |

| Tense (Spanish) | Opens from | Source of the forms |
| --- | --- | --- |
| presente | studied verbs | Stored in the dictionary. |
| imperfecto | presente | Stored in the dictionary. |
| indefinido | presente | Stored in the dictionary. |
| futuro | presente | Stored in the dictionary. |
| pretérito perfecto | presente | Built: the present of haber, then the past participle. |

| Tense (Portuguese) | Opens from | Source of the forms |
| --- | --- | --- |
| presente | studied verbs | Stored in the dictionary. |
| pretérito perfeito | presente | Stored in the dictionary. |
| imperfeito | presente | Stored in the dictionary. |
| futuro | presente | Stored in the dictionary. |
| perfeito composto | presente | Built: the present of ter, then the past participle. |

The pretérito perfeito comes second in Portuguese because it is the everyday past in Portugal,
where Spanish and French speech use the compound. The six persons are eu, tu, ele, nós, vós, eles;
vós is kept because textbooks and the dictionary's tables keep it. The forms are the European
ones: "nós falámos" in the past, where Brazil writes "falamos".

The dictionary has simple tenses only, so a compound tense is built by rule. In French a fixed list
names the verbs that take être, such as aller, venir and partir, and their participle takes the
plural ending in the plural: "nous sommes venus". Every other verb takes avoir. Every Spanish verb
takes haber and every Portuguese verb takes ter, and the participle never agrees.

A verb with two accepted spellings for a person, "je paie" and "je paye", shows one and accepts
either when typed.

Spanish labels its third persons "él/usted" and "ellos/ustedes", because usted and ustedes take
those forms. Vosotros is spoken only in Spain, so a setting leaves it out of conjugation trainings.
The setting is on by default and shows only for Spanish. When it is off, a session never asks the
vosotros form and the table leaves its cell blank. Progress is kept per verb and tense, so turning
the setting on or off changes no stored data.

| Person | Spelling shown |
| --- | --- |
| First person singular | The one furthest from the infinitive: "paie" for "payer". |
| Every other person | The one closest to the first person's. |
| A tie | The alphabetically first. |

## Session

| Stage | Exercise | What the learner does |
| --- | --- | --- |
| 1 | Table | Sees each word with its translation and the table of its forms, and can step back and forth. |
| 2 | Speed match | Matches the forms of one word per screen. Each word comes up once, in random order. |
| 3 | Missing word | Fills each form into two dictionary sentences, one word at a time. |

The match comes before typing, so the learner recognises a form before having to produce it.

The table has a singular and a plural column. Every cell carries its cue before the form: the
person for a verb, the pronoun of its gender for an adjective, "il", "elle", "ils", "elles", and
the article for a noun, "un", "une", "des".

### Match

A match screen holds the forms of one word and never mixes two words.

| Kind | Left side | Right side |
| --- | --- | --- |
| Verb | The person: je to ils/elles | The form |
| Adjective | The pronoun: il, elle, ils, elles | The form |
| Noun | The article: un, une, des, des | The form |

The cues always stand on the left in the same order, and only the forms are shuffled, so the
learner has to read them. Two forms spelled alike, such as "heureux" for the masculine singular and
plural, take either cue, and the two "des" of a noun take either plural.

A wrong pair only flashes. A matched screen moves on to the next word by itself.

### Missing word

Each form is asked in two rounds, and each round must be answered right once. A wrong answer does
not count and does not take anything off; the round comes back later. A form spelled like the
headword is not asked, because the word already stands on top; it stays in the match.

A round shows a sentence from the dictionary with the form blanked out: "Je ne ___ pas ta
psychologie." The word stands on top. The two rounds take two different sentences. A form with one
sentence shows it in both rounds. A form with none falls back to the bare question: "vous ___" or
"il/elle ___" for a verb, the cue for a noun or adjective, "elles ___".

The first round takes the words one at a time, in random order. It asks every form of a word in a
shuffled order, then asks the missed forms again until each is answered right, and only then moves
to the next word. The second round takes the words again in a new order, with the other sentence of
each form. The word that closed the first round does not open the second, and a batch never opens
with the form that closed the last one, so the two rounds of a form never come one after the other.

A sentence can carry any words, known or not. A verb's sentence must speak the form in its
person.

| Sentence | Taken for |
| --- | --- |
| A subject pronoun before the verb, with only "ne" or object pronouns between: "Je ne l'aime pas". | That pronoun's person. |
| An inverted pronoun after a hyphen: "Voulez-vous", "veut-il". | That pronoun's person. |
| No pronoun, inside a clause: "Marie aime le chocolat". | The third person. |
| No pronoun, opening a sentence or a clause: "Voulons la paix." | Nothing; it is an imperative. |

Spanish, Portuguese and Italian drop the subject, so a form there needs no pronoun, and a pronoun that is
present must match. A noun or adjective form is taken wherever it stands.

Every form has the same two sentences for every learner. The dictionary build picks them once and
stores them, for verbs, nouns and adjectives alike. Each sentence of the word is filed under every
form it fits. For a verb, a sentence that speaks the pronoun ranks first. Then an easier grammar
level ranks first, then a shorter sentence. The forms with the fewest sentences pick first, and a
sentence goes to one form only. A sentence is not taken when the blanked word is another word
spelled like the form, when the answer shows again in the sentence, when a present form is really a
participle after an auxiliary, or when a third person form is really a command to tú. A dictionary built before this table falls
back to the same picking at session start, which reads every sentence of the word.

After an answer the sentence shows whole with the form marked, and its translation appears under
it. A bare question is revealed as before: the spoken text with its ending marked, "un vendeur",
"elle belle", and the word's translation under it. On the phone the full table of the word shows
under the answer field either way. On the desktop the info button in the top bar opens the table
once the answer is revealed, and in the match at any time.

A wrong answer or "Don't know" must be typed right before the learner can continue. The wrong text
stays in the field until the first keystroke replaces it. Only the first answer is scored; the
retype is not, however many tries it takes. Typing waits for Continue.

### Marking

Wherever a full form is shown, the part that changes is drawn in the accent colour and underlined.
A simple tense marks its standard ending: veu**x**, voul**ons**. A form with no standard ending,
such as "font", is marked whole. A compound tense, a noun and an adjective mark where the form
departs from the headword: j'ai pr**is**, chev**aux**, be**lle**.

### Finishing

Finishing the session marks each of its words learned, whatever the answers were. Completing the
typing stage from the session menu finishes the session the same way. A forms session does not
count toward the daily word goal; it is recorded as activity for the day.

## Leaving and resuming

The session is cached on every change: the stage, the current exercise, the rounds answered, the
batch, the seed that picked the sentences and the order of the words, and a retype still owed. Leaving the screen keeps it. On app start every cached session is
restored, the most recent becomes active, and one whose words are gone is dropped. A cached
exercise that no longer belongs to its stage, for example after the stage order changed, restarts
that stage.

The session menu has three actions.

| Action | Effect |
| --- | --- |
| Restart stage | Runs the current stage again from the start. |
| Complete stage | Ends the current stage and moves to the next one. |
| End this session | Press and hold. Discards the cached session. Its words stay waiting and are taken first next time. |

## Dictionary dependence

Forms come from the dictionary at every load. Without a dictionary there are no new words in reach,
only waiting words count, and no verb is ready for a later tense. A session whose words cannot be
expanded does not start. A word whose forms cannot be built is left out of the match and typing,
still shows in the table stage, and is still marked learned. Reopening the dictionary drops every
cached lookup.

## Sentences

Sentence exercises use conjugation progress. A sentence with a conjugated verb is offered once
that verb has been learned in that tense.

- A verb with a regular present needs only the word for the present and the imperative.
- An infinitive needs only the word.
- The imperative opens with the present.
- A compound tense also needs the auxiliary learned in the present.
- A form with no tense in the table, such as the conditionnel or the subjonctif, opens once every
  listed tense of that verb has been learned.

## Upgrading

An install from before this design had the fixed forms decks. The first start after the upgrade
retires those decks and every card that belonged to them, including a card also kept in another
deck. Forms progress made there is not carried over.

## Open

- Forms get no number of their own yet. A forms training counts toward neither learned nor reviewed words, so it moves no ring on Today.
- Reviews: none today. Irregular verbs are the case that may need them.
- The second masculine form of a few adjectives: bel, vieil, nouvel, fol, mol.
- More tenses: the conditionnel, which would open from futur and imparfait, and the subjunctive.
- Progress is keyed without the native language, so the same target language studied from two
  native languages may share or lose forms progress.
