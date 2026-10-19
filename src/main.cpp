/**
  * Copyright (C) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <https://www.gnu.org/licenses/>.
  */

#include "framework/controller.h"
#include "localization.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace
{
    const std::string OptionsRoot = "/etc/options_menu/";
    constexpr useconds_t PollIntervalUs = 50000;

    void WaitForB(Controller & controller)
    {
        // drop the press that opened this entry
        controller.Update();
        controller.GetButtonStatus(B);
        for(;;)
        {
            controller.Update();
            if(controller.GetButtonStatus(B))
                return;
            usleep(PollIntervalUs);
        }
    }
}

int main()
{
    // stdout is a pipe here, so it is fully buffered unless flushed
    setvbuf(stdout, nullptr, _IONBF, 0);

    LoadLanguageFromConfig(OptionsRoot);

    Controller controller(1);

    std::cout << Translate("FAVORITES_HELLO") << std::endl;
    std::cout << Translate("FAVORITES_PRESS_B") << std::endl;
    WaitForB(controller);
    return 0;
}
