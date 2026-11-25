#include "ReaderBookLanguage.h"

#include "AppDatabase.h"

#include "core/FTLanguageDetection.h"
#include "core/FTReaderRepository.h"

#include <optional>
#include <string>

QString ReaderBookLanguage::GetLanguage() const
{
    return Language;
}

void ReaderBookLanguage::Update(const QString& DocumentId)
{
    const std::optional<FTReaderDocumentDetails> Stored{
        DocumentId.isEmpty() || !AppDatabase::Get().IsReady()
            ? std::nullopt
            : AppDatabase::Get().GetReaderLibrary().FindDetails(DocumentId.toStdString())};

    const QString Updated{Stored.has_value() && Stored->Language != UndeterminedLanguage
                              ? QString::fromStdString(Stored->Language)
                              : QString{}};

    if (Language == Updated)
    {
        return;
    }

    Language = Updated;

    emit changed();
}
