#include <catch2/catch_test_macros.hpp>

#include "core/FTLanguageDetection.h"

#include <string>
#include <vector>

TEST_CASE("Language detection names the language of a book's text", "[reader]")
{
    const std::vector<std::string> French{
        "Chapitre premier",
        "Le soleil se levait à peine lorsque Marie quitta la maison de son père.",
        "Elle marchait lentement le long de la rivière, en pensant à la lettre qu'elle avait reçue la veille.",
        "« Je ne sais pas si je dois revenir », dit-elle à voix basse."};

    const std::vector<std::string> Spanish{
        "Capítulo primero",
        "El sol apenas salía cuando María dejó la casa de su padre.",
        "Caminaba despacio a lo largo del río, pensando en la carta que había recibido la noche anterior.",
        "«No sé si debo volver», dijo en voz baja."};

    const std::vector<std::string> English{
        "Chapter one",
        "The sun was barely rising when Mary left her father's house.",
        "She walked slowly along the river, thinking about the letter she had received the night before.",
        "\"I do not know whether I should come back,\" she said quietly."};

    CHECK(DetectTextLanguage(French) == "fr");
    CHECK(DetectTextLanguage(Spanish) == "es");
    CHECK(DetectTextLanguage(English) == "en");
}

TEST_CASE("Language detection reports an undetermined language for text without words", "[reader]")
{
    CHECK(DetectTextLanguage({}) == UndeterminedLanguage);
    CHECK(DetectTextLanguage({"12345 67890", "* * *"}) == UndeterminedLanguage);
}
