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

uint32_t game_get_expected_crc32(void) { return 0x05CAF5DBu; }

const char *game_get_name(void) { return "Duck Hunt"; }

void game_on_init(void) {
    int port = (g_run_mode == RUN_MODE_EMULATED) ? 4371 : 4370;
    debug_server_init(port);

    if (g_run_mode != RUN_MODE_NATIVE && g_rom_path_for_extras) {
        verify_mode_init(g_rom_path_for_extras);
    }
}

void game_on_frame(uint64_t frame_count) { (void)frame_count; }

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

#ifdef ENABLE_NESTOPIA_ORACLE
    if (g_run_mode == RUN_MODE_NATIVE) return 0;

    if (strcmp(cmd, "emu_ppu_state") == 0) {
        NestopiaPpuRegs pr;
        nestopia_bridge_get_ppu_regs(&pr);
        int mirror = nestopia_bridge_get_mirroring();
        debug_server_send_fmt(
            "{\"id\":%d,\"ok\":true,"
            "\"ppuctrl\":\"0x%02X\",\"ppumask\":\"0x%02X\","
            "\"scroll_x\":%d,\"scroll_y\":%d,"
            "\"mirroring\":%d}",
            id, pr.ctrl, pr.mask, pr.scroll_x, pr.scroll_y, mirror);
        return 1;
    }

    if (strcmp(cmd, "read_emu_ppu") == 0) {
        /* Same format as built-in read_ppu but reads from Nestopia.
         * Params: addr (hex), len (int, max 256) */
        char addr_str[32] = {0};
        /* Inline minimal JSON string extraction */
        const char *p = strstr(json, "\"addr\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p == ' ' || *p == '"') p++;
                int i = 0;
                while (*p && *p != '"' && *p != ',' && *p != '}' && i < 31)
                    addr_str[i++] = *p++;
                addr_str[i] = '\0';
            }
        }
        if (!addr_str[0]) {
            debug_server_send_fmt("{\"id\":%d,\"ok\":false,\"error\":\"missing addr\"}", id);
            return 1;
        }
        uint32_t addr = (uint32_t)strtoul(addr_str, NULL, 0);

        /* Parse len */
        int len = 1;
        p = strstr(json, "\"len\"");
        if (p) {
            p = strchr(p, ':');
            if (p) len = atoi(p + 1);
        }
        if (len < 1) len = 1;
        if (len > 256) len = 256;

        /* Read from Nestopia's PPU memory */
        static uint8_t emu_nt[0x1000];
        static uint8_t emu_pal[0x20];
        static uint8_t emu_chr[0x2000];
        nestopia_bridge_get_nametable(emu_nt, sizeof(emu_nt));
        nestopia_bridge_get_palette(emu_pal);
        nestopia_bridge_get_chr_ram(emu_chr, sizeof(emu_chr));

        char hex[513];
        for (int i = 0; i < len; i++) {
            uint32_t a = addr + i;
            uint8_t v = 0;
            if (a < 0x2000)
                v = emu_chr[a];
            else if (a < 0x3000)
                v = emu_nt[a - 0x2000];
            else if (a >= 0x3F00 && a < 0x3F20)
                v = emu_pal[a - 0x3F00];
            snprintf(hex + i * 2, 3, "%02x", v);
        }

        debug_server_send_fmt(
            "{\"id\":%d,\"ok\":true,\"addr\":\"0x%04X\",\"len\":%d,\"hex\":\"%s\"}",
            id, addr, len, hex);
        return 1;
    }

    if (strcmp(cmd, "read_emu_oam") == 0) {
        static uint8_t emu_oam[0x100];
        nestopia_bridge_get_oam(emu_oam);
        char hex[513];
        int len = 256;
        for (int i = 0; i < len; i++)
            snprintf(hex + i * 2, 3, "%02x", emu_oam[i]);
        debug_server_send_fmt(
            "{\"id\":%d,\"ok\":true,\"len\":%d,\"hex\":\"%s\"}",
            id, len, hex);
        return 1;
    }
#endif
    return 0;
}
