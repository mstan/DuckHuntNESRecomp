/*
 * extras.c — Duck Hunt game-specific runner hooks
 * Implements game_extras.h.
 * Features: TCP debug server, verify mode via Nestopia oracle.
 */
#include "game_extras.h"
#include "nes_runtime.h"
#include "debug_server.h"
#include "verify_mode.h"
#ifdef ENABLE_NESTOPIA_ORACLE
#include "nestopia_bridge.h"
#endif
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Globals expected by the runner framework */
const char *g_rom_path_for_extras = NULL;
int         g_watchdog_triggered  = 0;
uint32_t    g_watchdog_frame      = 0;
const char *g_watchdog_stack_dump = "";

uint32_t game_get_expected_crc32(void) { return 0x24598791u; }

const char *game_get_name(void) { return "Duck Hunt"; }

void game_on_init(void) {
    int port = (g_run_mode == RUN_MODE_EMULATED) ? 4371 : 4370;
    debug_server_init(port);

    /* Duck Hunt requires Zapper (light gun) on port 2. */
    g_zapper_enabled = 1;
    g_zapper_x = 128;
    g_zapper_y = 120;

    /* Track $25 and $26 state changes */
    debug_server_add_follower(0x25, -1);
    debug_server_add_follower(0x26, -1);

    if (g_run_mode != RUN_MODE_NATIVE && g_rom_path_for_extras) {
        verify_mode_init(g_rom_path_for_extras);
    }
}

void game_on_frame(uint64_t frame_count) {
    (void)frame_count;
    if (frame_count < 15) {
        fprintf(stderr, "[STATE] f=%llu $25=%02X $26=%02X $1F=%02X $24=%02X\n",
                (unsigned long long)frame_count,
                g_ram[0x25], g_ram[0x26], g_ram[0x1F], g_ram[0x24]);
    }
}

void game_post_nmi(uint64_t frame_count) { (void)frame_count; }

int game_handle_arg(const char *key, const char *val) {
    if (strcmp(key, "--verify") == 0) {
        g_run_mode = RUN_MODE_VERIFY;
        return 1;
    }
    if (strcmp(key, "--emulated") == 0) {
        g_run_mode = RUN_MODE_EMULATED;
        return 1;
    }
    (void)val;
    return 0;
}

const char *game_arg_usage(void) {
    return "  --verify     Run both native + Nestopia, compare each frame\n"
           "  --emulated   Run Nestopia only (reference mode)\n";
}

void game_run_nmi(void) {
    verify_mode_run_nmi();
}

void game_run_main(void) {
    if (g_run_mode == RUN_MODE_EMULATED) {
#ifdef ENABLE_NESTOPIA_ORACLE
        printf("[Emulated] Nestopia driving main loop\n");
        static uint32_t emu_argb[256 * 240];
        extern void runner_present_framebuf(const uint32_t *argb_buf);

        for (;;) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) exit(0);
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) exit(0);
            }
            const uint8_t *keys = SDL_GetKeyboardState(NULL);
            uint8_t btn = 0;
            if (keys[SDL_SCANCODE_Z])      btn |= 0x80;
            if (keys[SDL_SCANCODE_X])      btn |= 0x40;
            if (keys[SDL_SCANCODE_TAB])    btn |= 0x20;
            if (keys[SDL_SCANCODE_RETURN]) btn |= 0x10;
            if (keys[SDL_SCANCODE_UP])     btn |= 0x08;
            if (keys[SDL_SCANCODE_DOWN])   btn |= 0x04;
            if (keys[SDL_SCANCODE_LEFT])   btn |= 0x02;
            if (keys[SDL_SCANCODE_RIGHT])  btn |= 0x01;
            g_controller1_buttons = btn;

            debug_server_poll();

            int ovr = debug_server_get_input_override();
            if (ovr >= 0) g_controller1_buttons = (uint8_t)ovr;

            nestopia_bridge_run_frame(g_controller1_buttons);
            nestopia_bridge_get_framebuf_argb(emu_argb);
            runner_present_framebuf(emu_argb);

            /* Mirror Nestopia PPU/CHR into runner globals so the
             * standard ring buffer captures oracle frames. After this block
             * a snapshot from --emulated is structurally identical to one
             * from --native, and the two can be byte-diffed for any frame. */
            nestopia_bridge_get_ram(g_ram);
            nestopia_bridge_get_chr_ram(g_chr_ram, sizeof(g_chr_ram));
            nestopia_bridge_get_nametable(g_ppu_nt, sizeof(g_ppu_nt));
            nestopia_bridge_get_palette(g_ppu_pal);
            nestopia_bridge_get_oam(g_ppu_oam);
            {
                NestopiaPpuRegs pr;
                nestopia_bridge_get_ppu_regs(&pr);
                g_ppuctrl     = pr.ctrl;
                g_ppumask     = pr.mask;
                g_ppuscroll_x = pr.scroll_x;
                g_ppuscroll_y = pr.scroll_y;
            }

            g_frame_count++;
            debug_server_record_frame();

            SDL_Delay(16);
        }
#else
        fprintf(stderr, "[Error] Nestopia not compiled in\n");
        func_RESET();
#endif
    } else {
        func_RESET();
    }
}

int game_dispatch_override(uint16_t addr) {
    (void)addr;
    return 0;
}

uint8_t game_ram_read_hook(uint16_t pc, uint16_t addr, uint8_t val) {
    (void)pc; (void)addr;
    return val;
}

void game_fill_frame_record(void *record) { (void)record; }

void game_post_render(uint32_t *framebuf) { (void)framebuf; }

int game_handle_debug_cmd(const char *cmd, int id, const char *json) {
    (void)json;

    if (strcmp(cmd, "game_info") == 0) {
        debug_server_send_fmt("{\"id\":%d,\"ok\":true,\"game\":\"Duck Hunt\",\"run_mode\":%d}", id, (int)g_run_mode);
        return 1;
    }

    return 0;
}
