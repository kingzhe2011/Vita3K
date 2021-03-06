// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "vpk.h"

#include <host/functions.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread_functions.h>

#include <SDL.h>

#include <cassert>
#include <iostream>

typedef std::unique_ptr<const void, void (*)(const void *)> SDLPtr;

enum ExitCode {
    Success = 0,
    IncorrectArgs,
    SDLInitFailed,
    HostInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed
};

static void error(const char *message, SDL_Window *window) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, window) < 0) {
        std::cerr << message << std::endl;
    }
}

static void term_sdl(const void *succeeded) {
    assert(succeeded != nullptr);

    SDL_Quit();
}

int main(int argc, char *argv[]) {
    std::cout << window_title << std::endl;

    if (argc < 2) {
        std::string message = "Usage: ";
        message += argv[0];
        message += " <path to VPK file>";
        error(message.c_str(), nullptr);
        return IncorrectArgs;
    }

    const SDLPtr sdl(reinterpret_cast<const void *>(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) >= 0), term_sdl);
    if (!sdl) {
        error("SDL initialisation failed.", nullptr);
        return SDLInitFailed;
    }

    HostState host;
    if (!init(host)) {
        error("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    Ptr<const void> entry_point;
    const char *const path = argv[1];
    if (!load_vpk(entry_point, host.io, host.mem, path)) {
        std::string message = "Failed to load \"";
        message += path;
        message += "\".";
        error(message.c_str(), host.window.get());
        return ModuleLoadFailed;
    }

    // TODO This is hacky. Belongs in kernel?
    const SceUID main_thread_id = host.kernel.next_uid++;

    const CallImport call_import = [&host, main_thread_id](uint32_t nid) {
        ::call_import(host, nid, main_thread_id);
    };

    const size_t stack_size = MB(1); // TODO Get main thread stack size from somewhere?
    const bool log_code = false;
    const ThreadStatePtr main_thread = init_thread(entry_point, stack_size, log_code, host.mem, call_import);
    if (!main_thread) {
        error("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    // TODO Move this to kernel.
    host.kernel.threads.emplace(main_thread_id, main_thread);

    host.t1 = SDL_GetTicks();
    if (!run_thread(*main_thread)) {
        error("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    return Success;
}
