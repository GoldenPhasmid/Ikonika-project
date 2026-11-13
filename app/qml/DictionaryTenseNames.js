.pragma library

const labels = {
    en: [ ["Present", "Present", "Imperfect", "Simple past", "Future"], ["Subjunctive", "Present subjunctive", "Imperfect subjunctive"], "Conditional", "Imperative" ],
    fr: [ ["présent", "présent", "imparfait", "passé simple", "futur"], ["subjonctif", "subjonctif présent", "subjonctif imparfait", "", "subjonctif futur"], "conditionnel présent", "impératif présent" ],
    es: [ ["presente", "presente", "pretérito imperfecto", "pretérito perfecto simple", "futuro simple"], ["subjuntivo", "presente de subjuntivo", "imperfecto de subjuntivo", "", "futuro de subjuntivo"], "condicional simple", "imperativo" ],
    pt: [ ["presente", "presente", "pretérito imperfeito", "pretérito perfeito", "futuro do presente"], ["conjuntivo", "presente do conjuntivo", "imperfeito do conjuntivo", "", "futuro do conjuntivo"], "condicional", "imperativo" ],
    it: [ ["presente", "presente", "imperfetto", "passato remoto", "futuro semplice"], ["congiuntivo", "congiuntivo presente", "congiuntivo imperfetto"], "condizionale presente", "imperativo presente" ],
    de: [ ["Präsens", "Präsens", "Präteritum", "Präteritum", "Futur I", "", "", "", "", "Perfekt"], ["Konjunktiv I", "Konjunktiv I", "Konjunktiv II"], "Konditional", "Imperativ" ]
}

function name(language, mood, tense)
{
    const names = labels[language] || labels.en
    if (mood === 2)
    {
        return names[1][tense] || names[1][0]
    }
    if (mood === 3 || mood === 4)
    {
        return names[mood - 1]
    }
    return names[0][tense] || ""
}

function longest(language)
{
    const names = labels[language] || labels.en
    return names[0].concat(names[1], names[2], names[3])
        .reduce((longest, label) => label.length > longest.length ? label : longest, "")
}
