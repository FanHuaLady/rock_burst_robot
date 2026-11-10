#include "app.h"

#include "control.h"
#include "control_server.h"
#include "GP8403.h"
#include "max22216.h"
#include "network.h"
#include "relay.h"

static uint8_t robot_app_initialized;

void App_Init(void)
{
    if (robot_app_initialized != 0U)
    {
        return;
    }

    (void)GP8403_Init();
    Relay_Init();
    MAX22216_Init();
    Control_Init();
    Network_Init();
    ControlServer_Init();

    robot_app_initialized = 1U;
}

void App_Poll(void)
{
    if (robot_app_initialized == 0U)
    {
        return;
    }

    Control_Poll();
    Network_Poll();
    ControlServer_Process();
}

void App_Tick1ms(void)
{
    Control_Tick1ms();
    Network_Tick1ms();
}
