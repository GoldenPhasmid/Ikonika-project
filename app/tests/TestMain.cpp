#include <QCoreApplication>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QCoreApplication App(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("IkonikaAppTests"));
    QCoreApplication::setApplicationName(QStringLiteral("IkonikaAppTests"));

    return Catch::Session().run(argc, argv);
}
