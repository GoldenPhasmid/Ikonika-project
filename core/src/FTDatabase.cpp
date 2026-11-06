#include "core/FTDatabase.h"

#include "core/FTLog.h"

#include <QSqlDriver>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QUuid>
#include <QVariant>

#include <sqlite3.h>

#include <utility>

struct FTCommitNotice
{
    std::function<void()> Listener;
    bool bInTransaction = false;
    bool bWroteInTransaction = false;

    void OnWrite()
    {
        if (bInTransaction)
        {
            bWroteInTransaction = true;

            return;
        }

        if (Listener)
        {
            Listener();
        }
    }
};

// sqlite3 is deliberately opaque (SQLite never defines the struct in its public header), which
// QMetaType/QVariant cannot hold a pointer to without this declaration. Qt's own SQLite driver
// hands back exactly this pointer type from QSqlDriver::handle(), so this only widens what the
// metatype system already has to deal with, not something new here.
Q_DECLARE_OPAQUE_POINTER(sqlite3*)

namespace
{
    // Rewrites SQLite's ?1, ?2, ... placeholders to Qt's named :p1, :p2, ... form. Needed because
    // QSqlQuery's positional binding is strictly sequential and cannot express a placeholder used
    // more than once in the same statement, which several queries in FTCardRepository do.
    std::string ToNamedPlaceholders(std::string_view Sql)
    {
        std::string Result;
        Result.reserve(Sql.size() + 16);

        for (size_t Index = 0; Index < Sql.size(); )
        {
            if (Sql[Index] == '?' && Index + 1 < Sql.size() && std::isdigit(static_cast<unsigned char>(Sql[Index + 1])))
            {
                size_t End = Index + 1;

                while (End < Sql.size() && std::isdigit(static_cast<unsigned char>(Sql[End])))
                {
                    ++End;
                }

                Result += ":p";
                Result.append(Sql.substr(Index + 1, End - Index - 1));
                Index = End;
            }
            else
            {
                Result += Sql[Index];
                ++Index;
            }
        }

        return Result;
    }

    QString PlaceholderName(int Index)
    {
        return QStringLiteral(":p%1").arg(Index);
    }
}

// ------------------------------------------------------------------- FTStatement

FTStatement::FTStatement(QSqlQuery&& InQuery, std::shared_ptr<FTCommitNotice> InNotice)
    : Query(std::move(InQuery))
    , Notice(std::move(InNotice))
    , bValid(true)
{
}

bool FTStatement::IsValid() const
{
    return bValid;
}

void FTStatement::BindText(int Index, std::string_view Value)
{
    if (!bValid)
    {
        return;
    }

    Query->bindValue(PlaceholderName(Index), QString::fromUtf8(Value.data(), static_cast<int>(Value.size())));
}

void FTStatement::BindInt(int Index, int Value)
{
    if (!bValid)
    {
        return;
    }

    Query->bindValue(PlaceholderName(Index), Value);
}

void FTStatement::BindInt64(int Index, int64_t Value)
{
    if (!bValid)
    {
        return;
    }

    Query->bindValue(PlaceholderName(Index), static_cast<qint64>(Value));
}

void FTStatement::BindDouble(int Index, double Value)
{
    if (!bValid)
    {
        return;
    }

    Query->bindValue(PlaceholderName(Index), Value);
}

void FTStatement::BindNull(int Index)
{
    if (!bValid)
    {
        return;
    }

    Query->bindValue(PlaceholderName(Index), QVariant());
}

bool FTStatement::Step()
{
    if (!bValid)
    {
        return false;
    }

    // A prepared statement has to be executed before its result set exists at all. sqlite3_step()
    // does both in one call; QSqlQuery splits them into exec() and next(), and callers of Step()
    // never call exec() themselves, so it happens here, once, the first time a caller steps.
    if (!Query->isActive())
    {
        if (!Query->exec())
        {
            return false;
        }

        if (!Query->isSelect() && Notice != nullptr)
        {
            Notice->OnWrite();
        }
    }

    return Query->next();
}

bool FTStatement::HasFailed() const
{
    return !bValid || Query->lastError().isValid();
}

bool FTStatement::Run()
{
    if (!bValid)
    {
        return false;
    }

    const bool bSucceeded = Query->exec();

    LastChangeCount = bSucceeded ? Query->numRowsAffected() : 0;

    if (bSucceeded && !Query->isSelect() && Notice != nullptr)
    {
        Notice->OnWrite();
    }

    return bSucceeded;
}

int FTStatement::GetChangeCount() const
{
    return LastChangeCount;
}

std::string FTStatement::GetText(int Column) const
{
    if (!bValid)
    {
        return {};
    }

    return Query->value(Column).toString().toStdString();
}

std::string FTStatement::GetBlob(int Column) const
{
    if (!bValid)
    {
        return {};
    }

    return Query->value(Column).toByteArray().toStdString();
}

int FTStatement::GetInt(int Column) const
{
    if (!bValid)
    {
        return 0;
    }

    return Query->value(Column).toInt();
}

int64_t FTStatement::GetInt64(int Column) const
{
    if (!bValid)
    {
        return 0;
    }

    return static_cast<int64_t>(Query->value(Column).toLongLong());
}

double FTStatement::GetDouble(int Column) const
{
    if (!bValid)
    {
        return 0.0;
    }

    return Query->value(Column).toDouble();
}

bool FTStatement::IsNull(int Column) const
{
    if (!bValid)
    {
        return true;
    }

    return Query->isNull(Column);
}

void FTStatement::Reset()
{
    if (!bValid)
    {
        return;
    }

    Query->finish();

    for (const QString& Name : Query->boundValueNames())
    {
        Query->bindValue(Name, QVariant());
    }
}

// -------------------------------------------------------------------- FTDatabase

FTDatabase::FTDatabase(QSqlDatabase InConnection)
    : Connection(std::move(InConnection))
    , Notice(std::make_shared<FTCommitNotice>())
{
}


FTDatabase::~FTDatabase()
{
    if (Connection.isValid())
    {
        const QString Name = Connection.connectionName();
        const QString ClosedFilePath = Connection.databaseName();
        const bool bWasOpen = Connection.isOpen();

        Connection = QSqlDatabase();
        QSqlDatabase::removeDatabase(Name);

        qCInfo(LogDatabase) << "closed connection" << Name << "for" << ClosedFilePath
                            << "wasOpen =" << bWasOpen;
    }
}

FTDatabase::FTDatabase(FTDatabase&& Other) noexcept
    : Connection(std::move(Other.Connection))
    , LastChangeCount(Other.LastChangeCount)
    , LastError(std::move(Other.LastError))
    , Notice(std::move(Other.Notice))
{
    Other.Connection = QSqlDatabase();
}

FTDatabase& FTDatabase::operator=(FTDatabase&& Other) noexcept
{
    if (this != &Other)
    {
        if (Connection.isValid())
        {
            const QString Name = Connection.connectionName();
            const QString ClosedFilePath = Connection.databaseName();

            Connection = QSqlDatabase();
            QSqlDatabase::removeDatabase(Name);

            qCInfo(LogDatabase) << "closed connection" << Name << "for" << ClosedFilePath
                                << "(replaced by move-assignment)";
        }

        Connection = std::move(Other.Connection);
        LastChangeCount = Other.LastChangeCount;
        LastError = std::move(Other.LastError);
        Notice = std::move(Other.Notice);
        Other.Connection = QSqlDatabase();
    }

    return *this;
}

void FTDatabase::HardenConnection()
{
    Execute("PRAGMA foreign_keys = ON");

    QVariant Handle = Connection.driver()->handle();

    if (Handle.canConvert<sqlite3*>())
    {
        sqlite3* Raw = qvariant_cast<sqlite3*>(Handle);

        sqlite3_db_config(Raw, SQLITE_DBCONFIG_DQS_DDL, 0, nullptr);
        sqlite3_db_config(Raw, SQLITE_DBCONFIG_DQS_DML, 0, nullptr);
    }
}

std::optional<FTDatabase> FTDatabase::Open(const std::string& FilePath)
{
    // Every open gets its own connection name. QSqlDatabase keys connections by name in a global
    // registry; two opens with the same name would silently share (or clobber) one connection,
    // which in-memory tests especially depend on not happening.
    const QString ConnectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlDatabase Opened = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), ConnectionName);
    Opened.setDatabaseName(QString::fromStdString(FilePath));

    if (!Opened.open())
    {
        qCWarning(LogDatabase) << "failed to open" << QString::fromStdString(FilePath)
                               << ":" << Opened.lastError().text();

        QSqlDatabase::removeDatabase(ConnectionName);

        return std::nullopt;
    }

    FTDatabase Target(Opened);

    Target.HardenConnection();

    // Write-ahead logging survives a crash better than the rollback journal and lets a reader
    // work while a writer is busy. Ignored by in-memory databases.
    Target.Execute("PRAGMA journal_mode = WAL");

    qCInfo(LogDatabase) << "opened connection" << ConnectionName << "for"
                        << QString::fromStdString(FilePath);

    return Target;
}

std::optional<FTDatabase> FTDatabase::OpenReadOnly(const std::string& FilePath)
{
    const QString ConnectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlDatabase Opened = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), ConnectionName);
    Opened.setDatabaseName(QString::fromStdString(FilePath));

    // QSQLITE's own read-only option, so a missing file fails open() outright instead of the
    // driver's default create-if-absent behaviour leaving an empty database on disk before
    // anything gets to check whether FilePath was ever a real one.
    Opened.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));

    if (!Opened.open())
    {
        qCWarning(LogDatabase) << "failed to open" << QString::fromStdString(FilePath)
                               << "read-only:" << Opened.lastError().text();

        QSqlDatabase::removeDatabase(ConnectionName);

        return std::nullopt;
    }

    FTDatabase Target(Opened);

    Target.HardenConnection();

    qCInfo(LogDatabase) << "opened read-only connection" << ConnectionName << "for"
                        << QString::fromStdString(FilePath);

    return Target;
}

std::optional<FTDatabase> FTDatabase::OpenInMemory()
{
    return Open(":memory:");
}

bool FTDatabase::IsOpen() const
{
    return Connection.isValid() && Connection.isOpen();
}

namespace
{
    /** Splits a SQL script into individual statements on ';', the way FTDatabase::Execute needs
        to since QSqlQuery::exec() runs exactly one statement at a time. A semicolon inside a
        "--" line comment or a '...' string literal is not a statement boundary, so both are
        tracked while scanning rather than doing a plain string split, which would otherwise cut
        a script in the wrong place the moment either one contains a semicolon.

        Deliberately not calling into the raw sqlite3 handle to run the whole script in one
        sqlite3_exec(): Qt's QSQLITE driver bundles its own build of SQLite, not the one this
        project vendors and links directly, and the two are not guaranteed to be ABI-compatible
        beyond the narrow, already-vetted sqlite3_db_config() calls FTDatabase::Open makes on
        that handle. Running full statement execution through the wrong build's internals is
        exactly the kind of mismatch that segfaults instead of failing loudly. */
    std::vector<std::string> SplitStatements(std::string_view Sql)
    {
        std::vector<std::string> Statements;
        std::string Current;

        bool bInLineComment = false;
        bool bInString = false;

        for (size_t Index = 0; Index < Sql.size(); ++Index)
        {
            const char Character = Sql[Index];

            if (bInLineComment)
            {
                if (Character == '\n')
                {
                    bInLineComment = false;
                }

                continue;
            }

            if (bInString)
            {
                Current += Character;

                if (Character == '\'')
                {
                    bInString = false;
                }

                continue;
            }

            if (Character == '-' && Index + 1 < Sql.size() && Sql[Index + 1] == '-')
            {
                bInLineComment = true;

                continue;
            }

            if (Character == '\'')
            {
                bInString = true;
                Current += Character;

                continue;
            }

            if (Character == ';')
            {
                Statements.push_back(std::move(Current));
                Current.clear();

                continue;
            }

            Current += Character;
        }

        if (!Current.empty())
        {
            Statements.push_back(std::move(Current));
        }

        return Statements;
    }
}

bool FTDatabase::Execute(std::string_view Sql)
{
    if (!IsOpen())
    {
        qCWarning(LogDatabase) << "Execute called while not open, connection ="
                               << Connection.connectionName();

        return false;
    }

    LastChangeCount = 0;

    bool bWrote{false};

    for (const std::string& Statement : SplitStatements(Sql))
    {
        const QString Trimmed = QString::fromStdString(Statement).trimmed();

        if (Trimmed.isEmpty())
        {
            continue;
        }

        QSqlQuery Query(Connection);

        if (!Query.exec(Trimmed))
        {
            LastError = Query.lastError().text().toStdString();

            qCWarning(LogDatabase) << "statement failed:" << Query.lastError().text()
                                   << "--" << Trimmed;

            return false;
        }

        LastChangeCount = Query.numRowsAffected();
        bWrote = bWrote || !Query.isSelect();
    }

    if (bWrote && Notice != nullptr)
    {
        Notice->OnWrite();
    }

    return true;
}

FTStatement FTDatabase::Prepare(std::string_view Sql)
{
    if (!IsOpen())
    {
        qCWarning(LogDatabase) << "Prepare called while not open, connection ="
                               << Connection.connectionName();

        return FTStatement();
    }

    QSqlQuery Query(Connection);

    if (!Query.prepare(QString::fromStdString(ToNamedPlaceholders(Sql))))
    {
        LastError = Query.lastError().text().toStdString();

        qCWarning(LogDatabase) << "failed to prepare:" << Query.lastError().text()
                               << "--" << QString::fromUtf8(Sql.data(), static_cast<int>(Sql.size()));

        return FTStatement();
    }

    return FTStatement(std::move(Query), Notice);
}

bool FTDatabase::BeginTransaction()
{
    if (!IsOpen() || !Connection.transaction())
    {
        return false;
    }

    Notice->bInTransaction = true;
    Notice->bWroteInTransaction = false;

    return true;
}

bool FTDatabase::Commit()
{
    if (!IsOpen() || !Connection.commit())
    {
        return false;
    }

    const bool bWrote{Notice->bWroteInTransaction};

    Notice->bInTransaction = false;
    Notice->bWroteInTransaction = false;

    if (bWrote)
    {
        Notice->OnWrite();
    }

    return true;
}

bool FTDatabase::Rollback()
{
    if (!IsOpen() || !Connection.rollback())
    {
        return false;
    }

    Notice->bInTransaction = false;
    Notice->bWroteInTransaction = false;

    return true;
}

int FTDatabase::GetChangeCount() const
{
    return LastChangeCount;
}

std::string FTDatabase::GetDataRevision() const
{
    const QVariant Handle = Connection.driver() != nullptr ? Connection.driver()->handle() : QVariant();
    sqlite3* Raw = Handle.canConvert<sqlite3*>() ? qvariant_cast<sqlite3*>(Handle) : nullptr;
    const long long Changes = Raw != nullptr ? sqlite3_total_changes64(Raw) : -1;

    return Connection.connectionName().toStdString() + ":" + std::to_string(Changes);
}

void FTDatabase::SetCommitListener(std::function<void()> Listener)
{
    if (Notice == nullptr)
    {
        qCWarning(LogDatabase) << "Cannot listen for commits on a database that is not open";

        return;
    }

    Notice->Listener = std::move(Listener);
}

std::string FTDatabase::GetLastError() const
{
    if (!Connection.isValid())
    {
        return "database is not open";
    }

    if (!LastError.empty())
    {
        return LastError;
    }

    return Connection.lastError().text().toStdString();
}
