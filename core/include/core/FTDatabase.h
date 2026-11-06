#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct FTCommitNotice;

class FTStatement
{
public:
    FTStatement() = default;

    FTStatement(FTStatement&& Other) noexcept = default;
    FTStatement& operator=(FTStatement&& Other) noexcept = default;

    FTStatement(const FTStatement&) = delete;
    FTStatement& operator=(const FTStatement&) = delete;

    bool IsValid() const;

    void BindText(int Index, std::string_view Value);
    void BindInt(int Index, int Value);
    void BindInt64(int Index, int64_t Value);
    void BindDouble(int Index, double Value);
    void BindNull(int Index);

    /** True when a row is ready to read. False when the statement finished or failed. */
    bool Step();

    bool HasFailed() const;

    /** Runs a statement expected to produce no rows. True on success. */
    bool Run();

    /** How many rows the last Run() changed. Zero when it changed nothing or failed, which is
        what every caller reads to tell "the row was there" from "it was not". */
    int GetChangeCount() const;

    std::string GetText(int Column) const;
    std::string GetBlob(int Column) const;
    int GetInt(int Column) const;
    int64_t GetInt64(int Column) const;
    double GetDouble(int Column) const;
    bool IsNull(int Column) const;

    void Reset();

private:
    friend class FTDatabase;

    FTStatement(QSqlQuery&& InQuery, std::shared_ptr<FTCommitNotice> InNotice);

    // Positional placeholders (?1, ?2, ...) are rewritten to Qt's named form (:p1, :p2, ...) at
    // prepare time, since QSqlQuery's own positional binding does not support repeating the same
    // placeholder or skipping around by number the way every call site here already does.
    std::optional<QSqlQuery> Query;
    std::shared_ptr<FTCommitNotice> Notice;
    bool bValid = false;

    int LastChangeCount = 0;
};

class FTDatabase
{
public:
    FTDatabase() = default;
    ~FTDatabase();

    FTDatabase(FTDatabase&& Other) noexcept;
    FTDatabase& operator=(FTDatabase&& Other) noexcept;

    FTDatabase(const FTDatabase&) = delete;
    FTDatabase& operator=(const FTDatabase&) = delete;

    /** Opens or creates a database file. Nothing on failure. */
    static std::optional<FTDatabase> Open(const std::string& FilePath);
    
    static std::optional<FTDatabase> OpenReadOnly(const std::string& FilePath);

    /** A private database that never touches disk. What the tests run against. */
    static std::optional<FTDatabase> OpenInMemory();

    bool IsOpen() const;

    /** For statements with no parameters and no results, including DDL. */
    bool Execute(std::string_view Sql);

    /** An invalid FTStatement on failure, so check IsValid. */
    FTStatement Prepare(std::string_view Sql);

    bool BeginTransaction();
    bool Commit();
    bool Rollback();

    /** How many rows the last Execute() changed. A prepared statement carries its own count; see
        FTStatement::GetChangeCount. */
    int GetChangeCount() const;

    std::string GetDataRevision() const;

    void SetCommitListener(std::function<void()> Listener);

    std::string GetLastError() const;

private:
    explicit FTDatabase(QSqlDatabase InConnection);
    
    void HardenConnection();

    QSqlDatabase Connection;
    
    int LastChangeCount = 0;
    
    std::string LastError;

    std::shared_ptr<FTCommitNotice> Notice;
};
