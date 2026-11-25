#pragma once

#include <QString>

#include <optional>
#include <string>
#include <vector>

struct FTPdfTextPiece
{
    QString Text;
    qsizetype PageStart = 0;
};

struct FTPdfTextLine
{
    std::vector<FTPdfTextPiece> Pieces;
    double Left = 0.0;
    double Right = 0.0;
    bool bMeasured = false;
    bool bSoftHyphen = false;
};

struct FTPdfTextPage
{
    std::vector<FTPdfTextLine> Lines;
};

struct FTPdfNarrationSpan
{
    int Page = -1;
    qsizetype PageStart = 0;
    qsizetype UnitStart = 0;
    qsizetype Length = 0;
};

struct FTPdfNarration
{
    std::vector<std::string> Units;
    std::vector<std::vector<FTPdfNarrationSpan>> Spans;
};

struct FTPdfPagePlace
{
    int Page = -1;
    qsizetype Character = 0;
};

struct FTPdfUnitPlace
{
    int Unit = -1;
    qsizetype Character = 0;
};

double FindPdfTextWidth(const std::vector<FTPdfTextPage>& Pages);

FTPdfNarration BuildPdfNarration(const std::vector<FTPdfTextPage>& Pages, int FirstPage, double TextWidth);

FTPdfNarration BuildPdfNarration(const std::vector<FTPdfTextPage>& Pages);

std::optional<FTPdfPagePlace> FindPdfPagePlace(const FTPdfNarration& Narration, int Unit, qsizetype Character);

std::optional<FTPdfUnitPlace> FindPdfUnitPlace(const FTPdfNarration& Narration, int Page, qsizetype Character);

std::vector<FTPdfNarrationSpan> FindPdfSpans(const FTPdfNarration& Narration, int Unit, qsizetype Start, qsizetype End);
