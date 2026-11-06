#include "AppState.h"

AppState& AppState::Get()
{
    static AppState Instance;

    return Instance;
}
