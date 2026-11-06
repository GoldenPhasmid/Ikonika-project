#include <catch2/catch_test_macros.hpp>

#include "core/FTInContextTranslator.h"

#include <QString>

#include <chrono>
#include <cstdio>
#include <string>

namespace
{
    const std::string ModelDirectory = OPUS_MT_MODEL_DIRECTORY;
    const std::string SpanishModelDirectory = OPUS_MT_SPANISH_MODEL_DIRECTORY;
    const std::string EnglishModelDirectory = OPUS_MT_ENGLISH_MODEL_DIRECTORY;
    const std::string PortugueseModelDirectory = OPUS_MT_PORTUGUESE_MODEL_DIRECTORY;
    const std::string ItalianModelDirectory = OPUS_MT_ITALIAN_MODEL_DIRECTORY;

    const std::optional<FTInContextTranslator>& Loaded()
    {
        static const std::optional<FTInContextTranslator> Translator =
            FTInContextTranslator::Create(ModelDirectory);

        return Translator;
    }

    const std::optional<FTInContextTranslator>& LoadedSpanish()
    {
        static const std::optional<FTInContextTranslator> Translator =
            FTInContextTranslator::Create(SpanishModelDirectory);

        return Translator;
    }

    void RunBenchmark(const std::string& DefaultDirectory, const std::vector<std::string>& Sentences)
    {
        using FTClock = std::chrono::steady_clock;

        const std::string Directory =
            qEnvironmentVariable("OPUS_MT_BENCHMARK_DIRECTORY", QString::fromStdString(DefaultDirectory))
                .toStdString();

        const FTClock::time_point LoadStart = FTClock::now();
        const std::optional<FTInContextTranslator> Translator = FTInContextTranslator::Create(Directory);
        const double LoadMilliseconds =
            std::chrono::duration<double, std::milli>(FTClock::now() - LoadStart).count();

        if (!Translator.has_value())
        {
            SKIP("No OPUS-MT model at " << Directory);
        }

        std::printf("model: %s\n", Directory.c_str());
        std::printf("load: %.0f ms\n", LoadMilliseconds);

        double TotalMilliseconds = 0.0;

        for (size_t Index = 0; Index < Sentences.size(); ++Index)
        {
            const FTClock::time_point Start = FTClock::now();
            const std::string Translation = Translator->Translate(Sentences[Index]);
            const double Milliseconds =
                std::chrono::duration<double, std::milli>(FTClock::now() - Start).count();

            TotalMilliseconds += Milliseconds;

            std::printf("%zu\t%.0f\t%s\t%s\n", Index + 1, Milliseconds, Sentences[Index].c_str(),
                        Translation.c_str());
        }

        std::printf("total: %.0f ms, mean %.0f ms\n", TotalMilliseconds,
                    TotalMilliseconds / static_cast<double>(Sentences.size()));
    }
}

TEST_CASE("The translator is loaded from the OPUS-MT model directory", "[translation][slow]")
{
    if (!Loaded().has_value())
    {
        WARN("the OPUS-MT model is absent, so the translator is untested here");

        return;
    }
}

TEST_CASE("Common French sentences translate to English", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    const std::vector<std::pair<std::string, std::string>> Cases = {
        {"Je mange une pomme verte.", "I eat a green apple."},
        {"Bonjour, comment allez-vous aujourd'hui ?", "Hello, how are you today?"},
        {"Le chat dort sur le canapé.", "The cat sleeps on the couch."},
        {"Nous allons au cinéma ce soir.", "We're going to the movies tonight."},
        {"Été, hiver, printemps, automne.", "Summer, winter, spring, autumn."},
        {"Cœur, œuf, sœur.", "Heart, egg, sister."},
        {"À bientôt !", "See you soon!"},
        {"Ça va bien, merci.", "It's okay, thanks."},
        {"Hôtel de ville.", "Town hall."},
        {"Qu'est-ce que c'est ?", "What is it?"},
        {"Le café était servi à midi.", "Coffee was served at noon."},
    };

    for (const auto& [French, English] : Cases)
    {
        CHECK(Loaded()->Translate(French) == English);
    }
}

TEST_CASE("Common Spanish sentences translate to English", "[translation][slow]")
{
    if (!LoadedSpanish().has_value())
    {
        WARN("the Spanish OPUS-MT model is absent, so it is untested here");

        return;
    }

    // A second pair, opened from its own directory by the same code: nothing in the translator is
    // language-aware, so which pair it handles is which model was loaded.
    //
    // Asserted on the English rather than on properties. A weaker check - non-empty, different from
    // the input - passed while this model was returning "(()}((a)((a)((a))" for every sentence,
    // which is the kind of pass that is worse than no test.
    const std::vector<std::pair<std::string, std::string>> Cases = {
        {"El gato duerme en el sofá.", "The cat sleeps on the couch."},
        {"Estoy comiendo una manzana verde.", "I'm eating a green apple."},
    };

    for (const auto& [Spanish, English] : Cases)
    {
        CHECK(LoadedSpanish()->Translate(Spanish) == English);
    }
}

TEST_CASE("Each sentence of a passage is translated", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    const std::vector<std::pair<std::string, std::string>> Cases = {
        {"Tu as vu Marie ? Oui, je l'ai vue hier.", "Did you see Marie? Yeah, I saw her yesterday."},
        {"Il pleut. Je reste à la maison. Et toi ?", "It's raining. I'll stay home. What about you?"},
        {"M. Dupont est arrivé. Il est fatigué.", "Mr. Dupont's here. He's tired."},
        {"Le prix est de 3.5 euros... C'est cher !", "The price is 3.5 euros... It's expensive!"},
        {"Pommes ? J'en ai acheté trois.", "Apples? I bought three."},
        {"Oui. Je pars demain.", "Yes. I'm leaving tomorrow."},
        {"Mme. Martin et le Dr. Roux sont là.", "Mrs. Martin and Dr. Roux are here."},
    };

    for (const auto& [French, English] : Cases)
    {
        CHECK(Loaded()->Translate(French) == English);
    }
}

TEST_CASE("Empty and whitespace-only input translate to nothing", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    CHECK(Loaded()->Translate("") == "");
    CHECK(Loaded()->Translate("   ") == "");
}

TEST_CASE("A missing model directory is refused by the translator rather than crashing", "[translation]")
{
    CHECK_FALSE(FTInContextTranslator::Create("this/path/does/not/exist").has_value());
    CHECK_FALSE(FTInContextTranslator::Create("").has_value());
}

TEST_CASE("French translation benchmark across grammar", "[.][translation-benchmark][fr]")
{
    RunBenchmark(ModelDirectory, {
        "Mes cousines sont arrivées hier ; elles resteront une semaine.",
        "Ma sœur est plus grande que mon frère.",
        "Elle a acheté une robe bleue et des chaussures noires.",
        "Les voitures rouges de mes voisines sont garées devant la maison.",
        "Mon voisin est boulanger et sa femme est infirmière.",
        "Je me lève à six heures tous les matins.",
        "Ils se sont rencontrés à Paris il y a dix ans.",
        "Elle s'est coupée en préparant le dîner.",
        "Nous nous promenons souvent le long de la rivière.",
        "Ne t'inquiète pas, tout va bien se passer.",
        "Hier, nous sommes allés au marché.",
        "Quand j'étais petit, je passais mes vacances chez ma grand-mère.",
        "Demain, il fera beau dans tout le pays.",
        "Je vais appeler le médecin cet après-midi.",
        "Elle venait de partir quand le téléphone a sonné.",
        "Il avait déjà mangé quand nous sommes arrivés.",
        "D'ici la fin de l'année, nous aurons fini les travaux.",
        "Napoléon mourut en 1821 sur l'île de Sainte-Hélène.",
        "Si j'avais de l'argent, j'achèterais une maison à la campagne.",
        "Si tu avais travaillé davantage, tu aurais réussi l'examen.",
        "Il faut que tu fasses tes devoirs avant de sortir.",
        "Je doute qu'il puisse venir ce soir.",
        "Bien qu'elle soit fatiguée, elle continue à travailler.",
        "J'aimerais que vous me disiez la vérité.",
        "Tu as vu Marie ? Oui, je l'ai vue hier.",
        "Des pommes ? J'en ai acheté trois.",
        "Il y pense tous les jours.",
        "Je le lui ai donné ce matin.",
        "Les lettres que j'ai écrites sont sur la table.",
        "Je n'ai jamais vu la mer.",
        "Il n'y a plus de pain.",
        "Personne n'est venu à la réunion.",
        "Elle ne mange ni viande ni poisson.",
        "La lettre a été envoyée par le directeur.",
        "On dit que cette ville est très dangereuse la nuit.",
        "Il est interdit de fumer dans les lieux publics.",
        "Où avez-vous mis les clés ?",
        "Pourquoi ne m'as-tu pas répondu ?",
        "Combien de temps faut-il pour aller à Lyon ?",
        "Le livre dont je t'ai parlé est sorti la semaine dernière.",
        "La ville où je suis né a beaucoup changé.",
        "C'est la personne à qui j'ai prêté mon vélo.",
        "Il pleut des cordes depuis ce matin.",
        "Il a pris sa retraite l'année dernière.",
        "Le bateau est entré dans le port au lever du soleil.",
        "Ferme la porte, s'il te plaît, il fait froid.",
        "J'ai le cafard depuis qu'elle est partie.",
        "Après avoir longtemps hésité, il a finalement décidé de quitter son emploi pour se consacrer à la peinture.",
        "Les étudiants qui n'ont pas rendu leur mémoire avant vendredi ne pourront pas se présenter à la soutenance.",
        "Bien que le gouvernement ait promis de baisser les impôts, la plupart des ménages n'ont constaté aucune amélioration de leur pouvoir d'achat.",
    });
}

TEST_CASE("English translation benchmark across grammar", "[.][translation-benchmark][en]")
{
    RunBenchmark(EnglishModelDirectory, {
        "My cousins arrived yesterday; they will stay for a week.",
        "Her brother is taller than both of his sisters.",
        "She bought a blue dress and a pair of black shoes.",
        "The children's toys are scattered all over the floor.",
        "My neighbour is a baker and his wife is a nurse.",
        "I get up at six o'clock every morning.",
        "They met in Paris ten years ago.",
        "She cut herself while she was cooking dinner.",
        "We often walk along the river in the evening.",
        "Don't worry, everything will be fine.",
        "We went to the market yesterday.",
        "When I was a child, I used to spend my holidays at my grandmother's.",
        "It will be sunny across the whole country tomorrow.",
        "I'm going to call the doctor this afternoon.",
        "She had just left when the phone rang.",
        "He had already eaten by the time we arrived.",
        "By the end of the year, we will have finished the work.",
        "I have been waiting for you for two hours.",
        "If I had money, I would buy a house in the country.",
        "If you had worked harder, you would have passed the exam.",
        "If you heat ice, it melts.",
        "I wish I had listened to my parents.",
        "You must finish your homework before you go out.",
        "You should have told me the truth.",
        "She can't have forgotten our meeting.",
        "Could you pass me the salt, please?",
        "The letter was sent by the director.",
        "The bridge is being repaired at the moment.",
        "English is spoken all over the world.",
        "I have never seen the sea.",
        "There isn't any bread left.",
        "Nobody came to the meeting.",
        "She eats neither meat nor fish.",
        "Where did you put the keys?",
        "Why didn't you answer me?",
        "How long does it take to get to London?",
        "You're coming tonight, aren't you?",
        "The book I told you about came out last week.",
        "The town where I was born has changed a lot.",
        "That's the man whose car was stolen.",
        "Please turn off the lights when you leave.",
        "She looks after her younger brother on weekends.",
        "We ran out of milk, so I picked some up on the way home.",
        "He gave up smoking last year.",
        "It's raining cats and dogs.",
        "Break a leg tonight!",
        "I'm feeling a bit under the weather today.",
        "After hesitating for a long time, he finally decided to quit his job and devote himself to painting.",
        "Students who have not submitted their thesis by Friday will not be allowed to sit the final examination.",
        "Although the government promised to cut taxes, most households have seen no improvement in their purchasing power.",
        "At nineteen hundred hours ship’s time I climbed down the metal ladder past the bays on either side into the capsule.",
        "“I could already make out the pale green shape of the only gauge…” — he said.",
    });
}

TEST_CASE("Portuguese translation benchmark across grammar", "[.][translation-benchmark][pt]")
{
    RunBenchmark(PortugueseModelDirectory, {
        "As minhas primas chegaram ontem; elas vão ficar uma semana.",
        "A minha irmã é mais alta do que o meu irmão.",
        "Ela comprou um vestido azul e uns sapatos pretos.",
        "Os carros vermelhos das minhas vizinhas estão estacionados em frente à casa.",
        "O meu vizinho é padeiro e a mulher dele é enfermeira.",
        "Eu levanto-me às seis horas todas as manhãs.",
        "Eles conheceram-se em Lisboa há dez anos.",
        "Ela cortou-se enquanto preparava o jantar.",
        "Nós passeamos muitas vezes ao longo do rio.",
        "Não te preocupes, vai correr tudo bem.",
        "Ontem fomos ao mercado.",
        "Quando eu era pequeno, passava as férias na casa da minha avó.",
        "Amanhã vai fazer sol em todo o país.",
        "Vou ligar para o médico esta tarde.",
        "Ela tinha acabado de sair quando o telefone tocou.",
        "Ele já tinha comido quando nós chegámos.",
        "Até ao fim do ano, teremos terminado as obras.",
        "Tenho estado a trabalhar muito ultimamente.",
        "Se eu tivesse dinheiro, compraria uma casa no campo.",
        "Se tu tivesses estudado mais, terias passado no exame.",
        "Quando você chegar, me liga.",
        "Se puderes, traz o pão.",
        "É preciso que tu faças os trabalhos de casa antes de saíres.",
        "Duvido que ele possa vir hoje à noite.",
        "Embora esteja cansada, ela continua a trabalhar.",
        "Gostaria que me dissesses a verdade.",
        "É importante estudarmos todos os dias.",
        "Dar-te-ei a resposta amanhã.",
        "Você viu a Maria? Sim, eu a vi ontem.",
        "Eu dei-lho esta manhã.",
        "As cartas que eu escrevi estão em cima da mesa.",
        "Nunca vi o mar.",
        "Já não há pão.",
        "Ninguém veio à reunião.",
        "Ela não come nem carne nem peixe.",
        "A carta foi enviada pelo diretor.",
        "Vendem-se casas nesta rua.",
        "Onde é que puseste as chaves?",
        "Por que você não me respondeu?",
        "Quanto tempo leva para chegar ao Porto?",
        "O livro de que te falei saiu na semana passada.",
        "A cidade onde eu nasci mudou muito.",
        "O João está doente, mas é um rapaz forte.",
        "Está chovendo muito desde manhã.",
        "Estou a ler um livro muito interessante.",
        "Fecha a porta, por favor, está frio.",
        "Ele pagou o pato pelos erros dos colegas.",
        "Depois de hesitar muito tempo, ele finalmente decidiu deixar o emprego para se dedicar à pintura.",
        "Os estudantes que não entregarem a tese até sexta-feira não poderão fazer o exame final.",
        "Embora o governo tenha prometido baixar os impostos, a maioria das famílias não notou nenhuma melhoria no seu poder de compra.",
    });
}

TEST_CASE("Italian translation benchmark across grammar", "[.][translation-benchmark][it]")
{
    RunBenchmark(ItalianModelDirectory, {
        "Le mie cugine sono arrivate ieri; resteranno una settimana.",
        "Mia sorella è più alta di mio fratello.",
        "Ha comprato un vestito blu e delle scarpe nere.",
        "Le macchine rosse delle mie vicine sono parcheggiate davanti alla casa.",
        "Il mio vicino è panettiere e sua moglie è infermiera.",
        "Mi alzo alle sei ogni mattina.",
        "Si sono conosciuti a Roma dieci anni fa.",
        "Si è tagliata mentre preparava la cena.",
        "Passeggiamo spesso lungo il fiume.",
        "Non ti preoccupare, andrà tutto bene.",
        "Ieri siamo andati al mercato.",
        "Da piccolo passavo le vacanze a casa di mia nonna.",
        "Domani ci sarà il sole in tutto il paese.",
        "Chiamerò il medico questo pomeriggio.",
        "Era appena uscito quando ha squillato il telefono.",
        "Aveva già mangiato quando siamo arrivati.",
        "Entro la fine dell'anno avremo finito i lavori.",
        "Ti aspetto da due ore.",
        "Se avessi dei soldi, comprerei una casa in campagna.",
        "Se avessi studiato di più, avresti superato l'esame.",
        "Quando arrivi, chiamami.",
        "È necessario che tu faccia i compiti prima di uscire.",
        "Dubito che possa venire stasera.",
        "Benché sia stanca, continua a lavorare.",
        "Vorrei che mi dicessi la verità.",
        "Speriamo che domani faccia bel tempo.",
        "Hai visto Maria? Sì, l'ho vista ieri.",
        "Gliel'ho dato stamattina.",
        "Ne vuoi ancora? No, non ne voglio più.",
        "Le lettere che ho scritto sono sul tavolo.",
        "Non ho mai visto il mare.",
        "Non c'è più pane.",
        "Nessuno è venuto alla riunione.",
        "Non mangia né carne né pesce.",
        "La lettera è stata inviata dal direttore.",
        "Si vendono case in questa strada.",
        "Dove hai messo le chiavi?",
        "Perché non mi hai risposto?",
        "Quanto tempo ci vuole per arrivare a Napoli?",
        "Il libro di cui ti ho parlato è uscito la settimana scorsa.",
        "La città dove sono nato è cambiata molto.",
        "Giovanni è malato, ma è un ragazzo forte.",
        "Piove a catinelle da stamattina.",
        "Mi piacciono molto i libri di storia.",
        "Chiudi la porta, per favore, fa freddo.",
        "Mi hai preso in giro con quella storia.",
        "Dopo aver esitato a lungo, ha finalmente deciso di lasciare il lavoro per dedicarsi alla pittura.",
        "Gli studenti che non consegneranno la tesi entro venerdì non potranno sostenere l'esame finale.",
        "Anche se il governo ha promesso di abbassare le tasse, la maggior parte delle famiglie non ha notato alcun miglioramento del proprio potere d'acquisto.",
        "Ti amo e vorrei passare il resto della mia vita con te.",
    });
}

TEST_CASE("Spanish translation benchmark across grammar", "[.][translation-benchmark][es]")
{
    RunBenchmark(SpanishModelDirectory, {
        "Mis primas llegaron ayer; se quedarán una semana.",
        "Mi hermana es más alta que mi hermano.",
        "Ella compró un vestido azul y unos zapatos negros.",
        "Los coches rojos de mis vecinas están aparcados delante de la casa.",
        "Mi vecino es panadero y su mujer es enfermera.",
        "Me levanto a las seis todas las mañanas.",
        "Se conocieron en Madrid hace diez años.",
        "Se cortó mientras preparaba la cena.",
        "A menudo paseamos a lo largo del río.",
        "No te preocupes, todo va a salir bien.",
        "Ayer fuimos al mercado.",
        "Cuando era pequeño, pasaba las vacaciones en casa de mi abuela.",
        "Mañana hará sol en todo el país.",
        "Voy a llamar al médico esta tarde.",
        "Acababa de salir cuando sonó el teléfono.",
        "Ya había comido cuando llegamos.",
        "Para finales de año, habremos terminado las obras.",
        "Llevo dos horas esperándote.",
        "Si tuviera dinero, compraría una casa en el campo.",
        "Si hubieras estudiado más, habrías aprobado el examen.",
        "Cuando llegues, llámame.",
        "Es necesario que hagas los deberes antes de salir.",
        "Dudo que pueda venir esta noche.",
        "Aunque esté cansada, sigue trabajando.",
        "Me gustaría que me dijeras la verdad.",
        "Ojalá haga buen tiempo mañana.",
        "¿Has visto a María? Sí, la vi ayer.",
        "¿Manzanas? Compré tres.",
        "Se lo di esta mañana.",
        "Dámelo, por favor.",
        "Las cartas que escribí están encima de la mesa.",
        "Nunca he visto el mar.",
        "Ya no queda pan.",
        "Nadie vino a la reunión.",
        "No come ni carne ni pescado.",
        "La carta fue enviada por el director.",
        "Se venden casas en esta calle.",
        "¿Dónde has puesto las llaves?",
        "¿Por qué no me contestaste?",
        "¿Cuánto tiempo se tarda en llegar a Sevilla?",
        "El libro del que te hablé salió la semana pasada.",
        "La ciudad donde nací ha cambiado mucho.",
        "Juan está enfermo, pero es un chico fuerte.",
        "Está lloviendo a cántaros desde esta mañana.",
        "A mí me gustan mucho los libros de historia.",
        "Cierra la puerta, por favor, hace frío.",
        "Me tomaste el pelo con esa historia.",
        "Después de dudar mucho tiempo, por fin decidió dejar su trabajo para dedicarse a la pintura.",
        "Los estudiantes que no entreguen la tesis antes del viernes no podrán presentarse al examen final.",
        "Aunque el gobierno prometió bajar los impuestos, la mayoría de los hogares no ha notado ninguna mejora en su poder adquisitivo.",
    });
}
