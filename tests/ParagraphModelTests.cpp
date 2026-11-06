#include <catch2/catch_test_macros.hpp>

#include "FTParagraphModel.h"

#include <QObject>

#include <string>
#include <vector>

namespace
{
    /** Counts model resets without pulling in Qt6::Test for one assertion. */
    class FTResetCounter
    {
    public:
        explicit FTResetCounter(FTParagraphModel& Model)
        {
            QObject::connect(&Model, &QAbstractItemModel::modelReset, &Context,
                             [this]() { ++Count; });
        }

        int GetCount() const
        {
            return Count;
        }

    private:
        QObject Context;
        int Count = 0;
    };
}

/*
 * The reading view's list. The point of it is that a large document costs what is on screen rather
 * than the whole file, so what matters is that rows convert on demand and that replacing the
 * paragraphs tells the view to redraw rather than leaving it showing the previous book.
 */

TEST_CASE("An empty model has no rows", "[paragraphmodel]")
{
    FTParagraphModel Model;

    CHECK(Model.rowCount() == 0);
}

TEST_CASE("Paragraphs come back as rows in order", "[paragraphmodel]")
{
    FTParagraphModel Model;
    Model.SetParagraphs({"premier", "deuxieme", "troisieme"});

    REQUIRE(Model.rowCount() == 3);
    CHECK(Model.data(Model.index(0, 0)).toString() == QStringLiteral("premier"));
    CHECK(Model.data(Model.index(1, 0)).toString() == QStringLiteral("deuxieme"));
    CHECK(Model.data(Model.index(2, 0)).toString() == QStringLiteral("troisieme"));
}

TEST_CASE("A row outside the model is empty rather than a crash", "[paragraphmodel]")
{
    FTParagraphModel Model;
    Model.SetParagraphs({"seul"});

    CHECK_FALSE(Model.data(Model.index(5, 0)).isValid());
    CHECK_FALSE(Model.data(QModelIndex()).isValid());
}

TEST_CASE("Text that is not ASCII survives the conversion", "[paragraphmodel]")
{
    // The paragraphs are held as UTF-8 std::string and converted per row, which is exactly where an
    // accented word would be mangled if the conversion assumed Latin-1.
    FTParagraphModel Model;
    Model.SetParagraphs({std::string("l'\xc3\xa9l\xc3\xa8ve a d\xc3\xa9j\xc3\xa0 mang\xc3\xa9")});

    REQUIRE(Model.rowCount() == 1);
    CHECK(Model.data(Model.index(0, 0)).toString()
          == QString::fromUtf8("l'\xc3\xa9l\xc3\xa8ve a d\xc3\xa9j\xc3\xa0 mang\xc3\xa9"));
}

TEST_CASE("Replacing the paragraphs resets the view", "[paragraphmodel]")
{
    FTParagraphModel Model;
    Model.SetParagraphs({"ancien"});

    const FTResetCounter Reset(Model);

    Model.SetParagraphs({"nouveau", "texte"});

    // Without the reset the view would keep drawing the previous document's rows.
    CHECK(Reset.GetCount() == 1);
    REQUIRE(Model.rowCount() == 2);
    CHECK(Model.data(Model.index(0, 0)).toString() == QStringLiteral("nouveau"));
}

TEST_CASE("Clearing empties the model and says so", "[paragraphmodel]")
{
    FTParagraphModel Model;
    Model.SetParagraphs({"un", "deux"});

    const FTResetCounter Reset(Model);

    Model.Clear();

    CHECK(Reset.GetCount() == 1);
    CHECK(Model.rowCount() == 0);
}
