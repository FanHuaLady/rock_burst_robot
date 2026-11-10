#include "stm32f4xx.h"
#include "app.h"

int main(void)
{
    App_Init();

    while (1)
    {
        App_Poll();
    }
}
