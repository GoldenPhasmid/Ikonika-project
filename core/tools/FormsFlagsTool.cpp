#include "core/FTDatabase.h"
#include "core/FTFormsFlags.h"

#include <QCoreApplication>

#include <cstdio>
#include <string>

int main(int argc, char* argv[])
{
    QCoreApplication Application(argc, argv);

    if (argc != 4)
    {
        std::fprintf(stderr, "usage: FormsFlagsTool <dictionary.db> <target language> <native language>\n");

        return 2;
    }

    std::optional<FTDatabase> Dictionary = FTDatabase::Open(argv[1]);

    if (!Dictionary.has_value())
    {
        std::fprintf(stderr, "could not open %s\n", argv[1]);

        return 1;
    }

    FTFormsFlagStats Stats;

    if (!FillFormsFlags(*Dictionary, argv[2], argv[3], Stats))
    {
        std::fprintf(stderr, "writing the forms flags failed\n");

        return 1;
    }

    std::printf("forms flags: %d eligible, %d not eligible, %d unresolved\n", Stats.Eligible, Stats.NotEligible,
                Stats.Unresolved);

    return 0;
}
