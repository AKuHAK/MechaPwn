/*
 * Copyright (c) 2021 MechaResearch
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <tamtypes.h>
#include <libpad.h>
// #include <libmc.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sbv_patches.h>

#include "mecha.h"
#include "exploit.h"
#include "pad.h"
#include "binaries.h"
#include "ui.h"
#include "mass.h"

static unsigned int big_size = 50, reg_size = 36;
// TODO: store existing areas on nvram on boot

static void ResetIOP()
{
    SifInitRpc(0);
    SifIopReset("", 0);
    while (!SifIopSync())
        ;
    SifInitRpc(0);
    SifLoadFileInit();
}

char unlockNVM()
{
    uint8_t version[4];
    getMechaVersion(version);

    uint8_t build_date[5];
    getMechaBuildDate(build_date);

    gsKit_clear(gsGlobal, Black);

    struct GSTEXTURE_holder *versionTextures = ui_printf(8, 8 + big_size + big_size / 2 + 0 * (reg_size + 4), reg_size, 0xFFFFFF, "Mecha version: %d.%02d\n", version[1], (version[2] | 1) - 1);
    struct GSTEXTURE_holder *buildTextures   = ui_printf(8, 8 + big_size + big_size / 2 + 1 * (reg_size + 4), reg_size, 0xFFFFFF, "Mecha build date: 20%02x/%02x/%02x %02x:%02x\n", build_date[0], build_date[1], build_date[2], build_date[3], build_date[4]);

    const uint8_t *patch                     = getPatch(build_date);

    struct GSTEXTURE_holder *exploitTextures = draw_text(8, 8 + big_size + big_size / 2 + 2 * (reg_size + 4), reg_size, 0xFFFFFF, "Press O to install exploit.\n");
    struct GSTEXTURE_holder *exitTextures    = draw_text(8, 8 + big_size + big_size / 2 + 3 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to exit.\n");

    drawFrame();

    while (1)
    {
        u32 new_pad = ReadCombinedPadStatus();

        if (new_pad & PAD_CIRCLE)
        {
            break;
        }
        else if (new_pad & PAD_CROSS)
        {
            gsKit_clear(gsGlobal, Black);
            ResetIOP();
            LoadExecPS2("rom0:OSDSYS", 0, NULL);

            return 0;
        }
    }

    // TODO: check original patch before, so it will be possible to restore it
    if (!installPatch(patch))
    {
        struct GSTEXTURE_holder *errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 4 * (reg_size + 4), reg_size, 0xFFFFFF, "Failed to install the patch.\n");

        drawFrame();

        freeGSTEXTURE_holder(versionTextures);
        freeGSTEXTURE_holder(buildTextures);
        freeGSTEXTURE_holder(exploitTextures);
        freeGSTEXTURE_holder(exitTextures);
        freeGSTEXTURE_holder(errorTextures);

        return 0;
    }

    freeGSTEXTURE_holder(versionTextures);
    freeGSTEXTURE_holder(buildTextures);
    freeGSTEXTURE_holder(exploitTextures);
    freeGSTEXTURE_holder(exitTextures);

    return 1;
}

struct MENU
{
    const char *title;
    const char *x_text;
    const char *o_text;
    uint8_t option_count;
    const char *options[10];
};

int drawMenu(struct MENU *menu)
{
    int selected_menu = 0;
    gsKit_clear(gsGlobal, Black);

    while (1)
    {
        u32 new_pad = ReadCombinedPadStatus();

        if (new_pad & PAD_UP)
        {
            selected_menu--;
        }
        else if (new_pad & PAD_DOWN)
        {
            selected_menu++;
        }
        else if (new_pad & PAD_CIRCLE)
        {
            return -1;
        }
        else if (new_pad & PAD_CROSS)
        {
            return selected_menu;
        }

        if (selected_menu < 0)
            selected_menu = menu->option_count - 1;
        else if (selected_menu > menu->option_count - 1)
            selected_menu = 0;

        gsKit_clear(gsGlobal, Black);

        int title_x, title_y;
        getTextSize(big_size, menu->title, &title_x, &title_y);

        struct GSTEXTURE_holder *TitleTexture = draw_text((gsGlobal->Width - title_x) / 2, 8, big_size, 0xFFFFFF, menu->title);
        struct GSTEXTURE_holder *oTexture     = draw_text(8, -reg_size - 8, reg_size, 0xFFFFFF, menu->o_text);
        struct GSTEXTURE_holder *xTexture     = draw_text(-8, -reg_size - 8, reg_size, 0xFFFFFF, menu->x_text);

        struct GSTEXTURE_holder *menus[256];
        memset(menus, 0, sizeof(menus));

        for (int i = 0; i < menu->option_count; i++)
        {
            int menu_x, menu_y;
            getTextSize(reg_size, menu->options[i], &menu_x, &menu_y);

            menus[i] = draw_text((gsGlobal->Width - menu_x) / 2, 8 + big_size + big_size / 2 + i * (reg_size + 4), reg_size, selected_menu == i ? 0xFF6040 : 0xFFFFFF, menu->options[i]);
        }

        drawFrame();

        for (int i = 0; i < 256; i++)
            if (menus[i])
                freeGSTEXTURE_holder(menus[i]);

        freeGSTEXTURE_holder(xTexture);
        freeGSTEXTURE_holder(oTexture);
        freeGSTEXTURE_holder(TitleTexture);
    }
}

// HKkorJAG
// ||||||||
// |||||||G - rom1:DVDVER last byte (only in PS3) (possible values: " UEAGDCM", space symbol for Japan)
// ||||||A  - rom1:DVDID last byte (possible values: "JUEAORCM")
// |||||J   - rom0:VERSTR (0x22 byte: "System ROM Version 5.0 06/23/03 J") (possible values: "JAE") ps1 games, best region - A (no restrictions)
// |Kkor    - rom0:OSDVER (5-8th byte) ("0190Csch"), allows to change console language set, possible values: Jjpn, Aeng, Eeng, Heng, Reng, Csch, Kkor, Htch, Aspa
// H        - rom0:ROMVER (4th byte 0220HD20060905) (possible values: "JAEHC") ps2 games, best region - A (no restrictions)
uint8_t region_keyseed[]                = {0x4d, 0x65, 0x63, 0x68, 0x61, 0x50, 0x77, 0x6e, 0x00, 0xec}; // MechaPwn +00 EC
uint8_t region_ciphertext_dex[]         = {0x05, 0x0D, 0x36, 0x04, 0x6F, 0x69, 0xB6, 0x76, 0x00, 0xAF}; // Region 00130000

// 0 - Japan
uint8_t region_params_japan[]           = {0x4a, 0x4a, 0x6a, 0x70, 0x6e, 0x4a, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00}; // JJjpnJJ
uint8_t region_ciphertext_japan_cex[]   = {0xf1, 0xef, 0x50, 0xec, 0x1c, 0x2e, 0xc5, 0x69, 0x00, 0x6b};

// 1 - USA
uint8_t region_params_usa[]             = {0x41, 0x41, 0x65, 0x6e, 0x67, 0x41, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00}; // AAengAU
uint8_t region_ciphertext_usa_cex[]     = {0xb8, 0x42, 0xd5, 0xbc, 0x53, 0xa1, 0x96, 0xe6, 0x00, 0x04};

// 2 - Oceania
uint8_t region_params_oceania[]         = {0x45, 0x45, 0x65, 0x6e, 0x67, 0x45, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00}; // EEengEO
uint8_t region_ciphertext_oceania_cex[] = {0x64, 0xaa, 0x23, 0x25, 0x28, 0x5c, 0x14, 0xf3, 0x00, 0x1e};

// 3 - UK
// same as europe

// 4 - Europe
uint8_t region_params_europe[]          = {0x45, 0x45, 0x65, 0x6e, 0x67, 0x45, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00}; // EEengEE
uint8_t region_ciphertext_europe_cex[]  = {0x54, 0x88, 0x4f, 0x90, 0x76, 0x4d, 0xcb, 0xeb, 0x00, 0xcb};

// 5 - Korea
uint8_t region_params_korea[]           = {0x48, 0x4b, 0x6b, 0x6f, 0x72, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}; // HKkorJA
uint8_t region_ciphertext_korea_cex[]   = {0x2a, 0xf0, 0x38, 0x2d, 0xef, 0xe5, 0x48, 0xa4, 0x00, 0xc0};

// 6 - Asia
uint8_t region_params_asia[]            = {0x48, 0x48, 0x65, 0x6e, 0x67, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}; // HHengJA
uint8_t region_ciphertext_asia_cex[]    = {0x2a, 0xf0, 0x38, 0x2d, 0xef, 0xe5, 0x48, 0xa4, 0x00, 0xc0};

// 7 - Taiwan
uint8_t region_params_taiwan[]          = {0x48, 0x48, 0x74, 0x63, 0x68, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}; // HHtchJA
// region_ciphertext is same as asia

// 8 - Russia
uint8_t region_params_russia[]          = {0x45, 0x52, 0x65, 0x6e, 0x67, 0x45, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00}; // ERengER
uint8_t region_ciphertext_russia_cex[]  = {0x3e, 0x11, 0xc1, 0xbb, 0xfb, 0x26, 0x2b, 0x4c, 0x00, 0x9c};


// 9 - China
uint8_t region_params_china[]           = {0x43, 0x43, 0x73, 0x63, 0x68, 0x4A, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00}; // CCschJC
uint8_t region_ciphertext_china_cex[]   = {0x0f, 0x5d, 0xb8, 0xa2, 0x9d, 0x85, 0xcc, 0x76, 0x00, 0xd5};

// 11 - Mexico / ???
uint8_t region_params_mexico[]          = {0x41, 0x41, 0x73, 0x70, 0x61, 0x41, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00}; // AAspaAM
uint8_t region_ciphertext_mexico_cex[]  = {0x35, 0x1a, 0x92, 0x9a, 0x05, 0x5b, 0xdc, 0x40, 0x00, 0x08};

// 12 - ???

void sum_buffer(uint8_t *buffer, int length)
{
    uint8_t sum = 0;
    for (int i = 0; i < length - 2; i++)
        sum += buffer[i];
    buffer[length - 2] = 0;
    buffer[length - 1] = ~sum;
}

char write_region(uint8_t *region_params, uint8_t *region_ciphertext)
{
    uint8_t version[4];
    uint8_t isSlim = 0;
    getMechaVersion(version);
    if (version[1] == 5)
        isSlim = 0;
    else if (version[1] == 6)
        isSlim = 1;

    if (isSlim)
    {
        for (int i = 0; i < 12; i += 2)
            if (!WriteNVM(192 + i / 2, *(uint16_t *)&region_params[i]))
                break;
    }

    if (region_ciphertext)
    {
        for (int i = 0; i < 10; i += 2)
            if (!WriteNVM(227 + i / 2, *(uint16_t *)&region_keyseed[i]))
                break;

        for (int i = 0; i < 10; i += 2)
            if (!WriteNVM(232 + i / 2, *(uint16_t *)&region_ciphertext[i]))
                break;
    }

    return 1;
}

void selectCexDex(char *isDex)
{
    gsKit_clear(gsGlobal, Black);

    struct MENU menu;
    menu.title        = "Select console type";
    menu.x_text       = "X Select";
    menu.o_text       = "O Exit";
    menu.option_count = 2;
    menu.options[0]   = "Retail-DEX";
    menu.options[1]   = "CEX (retail)";
    int selected      = drawMenu(&menu);

    gsKit_clear(gsGlobal, Black);

    drawFrame();

    if (selected == -1)
    {
        ResetIOP();
        LoadExecPS2("rom0:OSDSYS", 0, NULL);
        SleepThread();
    }

    *isDex = selected == 0;
}

void selectRegion(char isDex, uint8_t **region_params, uint8_t **region_ciphertext)
{
    gsKit_clear(gsGlobal, Black);

    struct MENU menu;
    menu.title  = "Select region";
    menu.x_text = "X Select";
    menu.o_text = "O Exit";

    uint8_t version[4];
    uint8_t isSlim = 0;
    getMechaVersion(version);
    if (version[1] == 5)
        isSlim = 0;
    else if (version[1] == 6)
        isSlim = 1;

    if (!(isSlim) && isDex)
    {
        menu.option_count = 1;
        menu.options[0]   = "FAT-DEX region not supported"; // Aeng*U
    }
    else
    {
        menu.option_count = 9;
        menu.options[0]   = "USA (Multi-7)"; // Aeng*U
        menu.options[1]   = "Japan";         // Jjpn*J
        menu.options[2]   = "Russia";        // Reng*R
        menu.options[3]   = "Korea";         // Kkor*A
        menu.options[4]   = "Taiwan";        // Htch*A
        // menu.options[5]   = "China";             // Csch*C
        menu.options[5]   = "Asia (Multi-7)";    // Heng*A
        menu.options[6]   = "Mexico (Multi-7)";  // Aspa*M
        menu.options[7]   = "Europe (Multi-7)";  // Eeng*E
        menu.options[8]   = "Oceania (Multi-7)"; // Eeng*O
    }


    int selected = drawMenu(&menu);

    if (selected == -1)
    {
        ResetIOP();
        LoadExecPS2("rom0:OSDSYS", 0, NULL);
        SleepThread();
    }
    else
    {
        if (selected == 0)
        {
            *region_params     = region_params_usa;
            *region_ciphertext = region_ciphertext_usa_cex;
        }
        else if (selected == 1)
        {
            *region_params     = region_params_japan;
            *region_ciphertext = region_ciphertext_japan_cex;
        }
        else if (selected == 2)
        {
            *region_params     = region_params_russia;
            *region_ciphertext = region_ciphertext_russia_cex;
        }
        else if (selected == 3)
        {
            *region_params     = region_params_korea;
            *region_ciphertext = region_ciphertext_korea_cex;
        }
        else if (selected == 4)
        {
            *region_params     = region_params_taiwan;
            *region_ciphertext = region_ciphertext_asia_cex;
        }
        /* else if (selected == 5)
        {
            *region_params     = region_params_china;
            *region_ciphertext = region_ciphertext_china_cex;
        } */
        else if (selected == 5)
        {
            *region_params     = region_params_asia;
            *region_ciphertext = region_ciphertext_asia_cex;
        }
        else if (selected == 6)
        {
            *region_params     = region_params_mexico;
            *region_ciphertext = region_ciphertext_mexico_cex;
        }
        else if (selected == 7)
        {
            *region_params     = region_params_europe;
            *region_ciphertext = region_ciphertext_europe_cex;
        }
        else if (selected == 8)
        {
            *region_params     = region_params_oceania;
            *region_ciphertext = region_ciphertext_oceania_cex;
        }
        if (isDex)
        {
            *region_ciphertext  = region_ciphertext_dex;
            region_params[0][0] = 0x41; // A - PS2 disable checks
            region_params[0][5] = 0x41; // A - PS1 disable checks
        }
    }
}

void setRegion(char *isDex)
{
    selectCexDex(isDex);

    uint8_t *region_params     = 0;
    uint8_t *region_ciphertext = 0;
    selectRegion(*isDex, &region_params, &region_ciphertext);

    write_region(region_params, region_ciphertext);
}

uint8_t *frames[] = {
    &frame_001, &frame_002, &frame_003, &frame_004, &frame_005, &frame_006, &frame_007, &frame_008,
    &frame_009, &frame_010, &frame_011, &frame_012, &frame_013, &frame_014, &frame_015, &frame_016,
    &frame_017, &frame_018, &frame_019, &frame_020, &frame_021, &frame_022, &frame_023, &frame_024,
    &frame_025, &frame_026, &frame_027, &frame_028, &frame_029, &frame_030, &frame_031, &frame_032,
    &frame_033, &frame_034, &frame_035, &frame_036, &frame_037, &frame_038, &frame_039, &frame_040,
    &frame_041, &frame_042, &frame_043, &frame_044, &frame_045, &frame_046, &frame_047, &frame_048,
    &frame_049, &frame_050, &frame_051, &frame_052, &frame_053, &frame_054, &frame_055, &frame_056,
    &frame_057, &frame_058, &frame_059, &frame_060, &frame_061, &frame_062};

void drawLogoFrame(uint8_t frame, char *text2)
{
    gsKit_clear(gsGlobal, Black);

    struct GSTEXTURE_holder *logoTextures = drawImage((gsGlobal->Width - 480) / 2, (gsGlobal->Height - 270) / 2, 480, 270, frames[frame]);

    char text[]                           = "MechaPwn";
    int x, y;
    getTextSize(big_size, text, &x, &y);
    y += big_size;
    struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - 270) / 2 - big_size - 8, big_size, 0xFFFFFF, text);

    int x2, y2;
    getTextSize(big_size, text2, &x2, &y2);
    y2 += big_size;
    struct GSTEXTURE_holder *text2Textures = draw_text((gsGlobal->Width - x2) / 2, (gsGlobal->Height - 270) / 2 + 270, big_size, 0xFFFFFF, text2);

    drawFrame();

    freeGSTEXTURE_holder(text2Textures);
    freeGSTEXTURE_holder(textTextures);
    freeGSTEXTURE_holder(logoTextures);
}

void drawLogo()
{
    uint8_t frame = 0;
    while (!MassCheck())
    {
        drawLogoFrame(frame++, "Waiting for USB drive...");

        if (frame >= 62)
            frame = 0;
    }

    while (1)
    {
        drawLogoFrame(frame++, "Press X to continue.");

        if (frame >= 62)
            frame = 0;

        u32 new_pad = ReadCombinedPadStatus();

        if (new_pad & PAD_CROSS)
        {
            break;
        }
    }
}

char backupNVM()
{
    // TODO: maybe save 2 backups? one to the memory card, ont ot the flash?
    uint32_t serial[1];
    getSerial(serial);
    uint8_t version[4];
    getMechaVersion(version);

    char nvm_path[256];
    sprintf(nvm_path, "mass:/nvm_%d.%02d_%07ld.bin", version[1], (version[2] | 1) - 1, serial[0]);

    FILE *f = fopen(nvm_path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        int len = ftell(f);
        fclose(f);
        if (len == 1024)
            return 1;
        else
        {
            gsKit_clear(gsGlobal, Black);

            char text[] = "NVRAM backup is corrupted!";

            int x, y;
            getTextSize(reg_size, text, &x, &y);
            y += reg_size;

            struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, text);
            struct GSTEXTURE_holder *exitTextures = draw_text(8, 8 + big_size + big_size / 2 + 6 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to exit.\n");
            drawFrame();
            freeGSTEXTURE_holder(textTextures);
            freeGSTEXTURE_holder(exitTextures);
            while (1)
            {
                u32 new_pad = ReadCombinedPadStatus();

                if (new_pad & PAD_CROSS)
                {
                    ResetIOP();
                    LoadExecPS2("rom0:OSDSYS", 0, NULL);
                    SleepThread();
                }
            }
            return 0;
        }
    }

    int x, y;
    getTextSize(reg_size, "Backing up NVRAM...", &x, &y);
    y += reg_size;

    gsKit_clear(gsGlobal, Black);
    struct GSTEXTURE_holder *textTextures = ui_printf((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, "Backing up NVRAM...");
    drawFrame();
    freeGSTEXTURE_holder(textTextures);

    f = fopen(nvm_path, "wb");
    for (int i = 0; i < 0x200; i++)
    {
        uint16_t data;
        if (!ReadNVM(i, &data))
            break;
        fwrite(&data, 1, 2, f);
    }

    // TODO: open file again, check if its size isnt zero, or maybe even compare with nvm again
    // TODO: add more messages to the screen about backuping
    fclose(f);

    return 1;
}

char applyPatches(char isDex)
{
    uint8_t build_date[5];
    getMechaBuildDate(build_date);

    char applyOriginalPatch     = 0;
    char applyForceUnlock       = 0;
    const uint8_t *force_unlock = getForceUnlock(build_date);
    const uint8_t *orig_patch   = getOrigPatch(build_date);

    if (force_unlock)
    {
        gsKit_clear(gsGlobal, Black);

        struct MENU menu;
        menu.title        = "Patch menu";
        menu.x_text       = "X Select";
        menu.o_text       = "O Exit";
        menu.option_count = (isDex ? 3 : 2); // isDex

        menu.options[0]   = "Keep current patch";
        menu.options[1]   = "Restore factory defaults";
        if (isDex)
            menu.options[2] = "Install force unlock";

        int selected = drawMenu(&menu);
        if (selected == -1)
        {
            ResetIOP();
            LoadExecPS2("rom0:OSDSYS", 0, NULL);
            SleepThread();
        }
        else if (selected == 1)
        {
            applyOriginalPatch = 1;
        }
        else if (selected == 2)
        {
            applyForceUnlock = 1;
        }
    }

    if (applyOriginalPatch)
    {
        int x, y;
        getTextSize(reg_size, "Applying factory defaults...", &x, &y);
        y += reg_size;

        gsKit_clear(gsGlobal, Black);
        struct GSTEXTURE_holder *textTextures = ui_printf((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, "Applying original patch...");
        drawFrame();
        freeGSTEXTURE_holder(textTextures);

        for (int i = 0; i < 112; i++)
        {
            if (!WriteNVM(400 + i, *(uint16_t *)&orig_patch[i * 2]))
                break;
        }
    }
    else if (applyForceUnlock)
    {
        int x, y;
        getTextSize(reg_size, "Applying force unlock...", &x, &y);
        y += reg_size;

        gsKit_clear(gsGlobal, Black);
        struct GSTEXTURE_holder *textTextures = ui_printf((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, "Applying force unlock...");
        drawFrame();
        freeGSTEXTURE_holder(textTextures);

        for (int i = 0; i < 112; i++)
        {
            if (!WriteNVM(400 + i, *(uint16_t *)&force_unlock[i * 2]))
                break;
        }
    }
    else
    {
        uint32_t serial[1];
        getSerial(serial);
        uint8_t version[4];
        getMechaVersion(version);

        char nvm_path[256];
        sprintf(nvm_path, "mass:/nvm_%d.%02d_%07ld.bin", version[1], (version[2] | 1) - 1, serial[0]);
        FILE *f = fopen(nvm_path, "rb");
        if (!f)
        {
            gsKit_clear(gsGlobal, Black);

            char text[] = "Failed to open NVRAM backup!";

            int x, y;
            getTextSize(reg_size, text, &x, &y);
            y += reg_size;

            struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, text);
            struct GSTEXTURE_holder *exitTextures = draw_text(8, 8 + big_size + big_size / 2 + 6 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to exit.\n");
            drawFrame();
            freeGSTEXTURE_holder(textTextures);
            freeGSTEXTURE_holder(exitTextures);
            while (1)
            {
                u32 new_pad = ReadCombinedPadStatus();

                if (new_pad & PAD_CROSS)
                {
                    ResetIOP();
                    LoadExecPS2("rom0:OSDSYS", 0, NULL);
                    SleepThread();
                }
            }
            return 0;
        }
        else
        {
            fseek(f, 0, SEEK_END);
            int len = ftell(f);
            fclose(f);
            if (len != 1024)
            {
                gsKit_clear(gsGlobal, Black);

                char text[] = "NVRAM backup is corrupted!";

                int x, y;
                getTextSize(reg_size, text, &x, &y);
                y += reg_size;

                struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, text);
                struct GSTEXTURE_holder *exitTextures = draw_text(8, 8 + big_size + big_size / 2 + 6 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to exit.\n");
                drawFrame();
                freeGSTEXTURE_holder(textTextures);
                freeGSTEXTURE_holder(exitTextures);
                while (1)
                {
                    u32 new_pad = ReadCombinedPadStatus();

                    if (new_pad & PAD_CROSS)
                    {
                        ResetIOP();
                        LoadExecPS2("rom0:OSDSYS", 0, NULL);
                        SleepThread();
                    }
                }
                return 0;
            }
        }

        int x, y;
        getTextSize(reg_size, "Restoring patches...", &x, &y);
        y += reg_size;

        gsKit_clear(gsGlobal, Black);
        struct GSTEXTURE_holder *textTextures = ui_printf((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, "Restoring patches...");
        drawFrame();
        freeGSTEXTURE_holder(textTextures);

        fseek(f, 400 * 2, SEEK_SET);
        for (int i = 0; i < 112; i++)
        {

            uint16_t data;
            fread(&data, 1, 2, f);
            if (!WriteNVM(400 + i, data))
                break;
        }

        fclose(f);
    }
    return 1;
}

uint8_t *getPowerTexture()
{
    uint8_t version[4];
    getMechaVersion(version);

    if (version[1] == 6)
    {
        // TODO: FIXME: 79k and 90k has the same mecha 6.12 but different power cord
        /* if (version[2] < 12)
            return &pwr70k;
        else
            return &pwr90k; */
        uint16_t ModelId;
        ReadNVM(0xF8, &ModelId);
        if (ModelId < 0xd477) // TODO: or < 0xd474 ??
            return &pwr70k;
        else
            // TODO: add picture for ps2 bravia
            // if (ModelId != 0xd48f)
            return &pwr90k;
    }

    // TODO: add picture for DESR
    // if (version[3] == 1) // DESR indicator
    return &pwr50k;
}

char isPatchKnown()
{
    uint8_t build_date[5];
    getMechaBuildDate(build_date);

    uint8_t current_patch[224];

    for (int i = 0; i < 112; i++)
    {
        if (!ReadNVM(400 + i, (uint16_t *)&current_patch[i * 2]))
            break;
    }

    const uint8_t *patch = getPatch(build_date); // check upgrade patch
    char ret             = memcmp(current_patch, patch, 224) == 0;
    if (!ret)
    {
        patch = getForceUnlock(build_date); // check Force Unlock
        ret   = memcmp(current_patch, patch, 224) == 0;
        if (!ret)
        {
            patch = getOrigPatch(build_date); // Check original patch
            ret   = memcmp(current_patch, patch, 224) == 0;
            if (!ret)
            {
                ret = memcmp(current_patch, orig_patch610_A, 224) == 0; // 6.10 has 2 different patches
            }
        }
    }


    return ret;
}

void checkUnsupportedVersion()
{
    uint8_t version[4];
    uint8_t build_date[5];
    struct GSTEXTURE_holder *versionTextures;
    struct GSTEXTURE_holder *errorTextures;

    gsKit_clear(gsGlobal, Black);

    if (!getMechaBuildDate(build_date))
    {
        if (getMechaVersion(version))
            versionTextures = ui_printf(8, 8 + big_size + big_size / 2 + 0 * (reg_size + 4), reg_size, 0xFFFFFF, "Mecha version: %d.%02d\n", version[1], (version[2] | 1) - 1);

        errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 5 * (reg_size + 4), reg_size, 0xFFFFFF, "This MechaCon isn't supported!\n");

        drawFrame();

        if (getMechaVersion(version))
            freeGSTEXTURE_holder(versionTextures);
        freeGSTEXTURE_holder(errorTextures);

        SleepThread();
        return;
    }
    else
    {
        uint16_t ModelId;
        uint32_t serial[1];
        ReadNVM(0xF8, &ModelId);
        getSerial(serial);
        getMechaVersion(version);
        struct GSTEXTURE_holder *serialTextures  = ui_printf(8, 8 + big_size + big_size / 2 + 2 * (reg_size + 4), reg_size, 0xFFFFFF, "S/N: %07d\n", serial[0]);

        struct GSTEXTURE_holder *ModelIDTextures = ui_printf(8, 8 + big_size + big_size / 2 + 3 * (reg_size + 4), reg_size, 0xFFFFFF, "Model ID: 0x%X\n", ModelId);

        versionTextures                          = ui_printf(8, 8 + big_size + big_size / 2 + 0 * (reg_size + 4), reg_size, 0xFFFFFF, "Mecha version: %d.%02d\n", version[1], (version[2] | 1) - 1);
        struct GSTEXTURE_holder *buildTextures   = ui_printf(8, 8 + big_size + big_size / 2 + 1 * (reg_size + 4), reg_size, 0xFFFFFF, "Mecha build date: 20%02x/%02x/%02x %02x:%02x\n", build_date[0], build_date[1], build_date[2], build_date[3], build_date[4]);

        // ModelID whitelist
        char RealModelName[20];
        if (ModelId == 0xd301)
            sprintf(RealModelName, "DTL-H50000");
        else if (ModelId == 0xd302)
            sprintf(RealModelName, "DTL-H50001");
        else if (ModelId == 0xd303)
            sprintf(RealModelName, "DTL-H50002");
        else if (ModelId == 0xd304)
            sprintf(RealModelName, "DTL-H50009");
        // d305 - d321 ??
        else if (ModelId == 0xd322)
            sprintf(RealModelName, "DTL-H75000A");
        /* else if (ModelId == 0xd323)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd324)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd325)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd326)
            sprintf(RealModelName, "DTL-H90000(a)");
        else if (ModelId == 0xd380)
            sprintf(RealModelName, "DESR-7000");
        else if (ModelId == 0xd381)
            sprintf(RealModelName, "DESR-5000");
        else if (ModelId == 0xd382)
            sprintf(RealModelName, "DESR-7100");
        else if (ModelId == 0xd383)
            sprintf(RealModelName, "DESR-5100");
        else if (ModelId == 0xd384)
            sprintf(RealModelName, "DESR-5100/S");
        else if (ModelId == 0xd385)
            sprintf(RealModelName, "DESR-7500");
        else if (ModelId == 0xd386)
            sprintf(RealModelName, "DESR-5500");
        else if (ModelId == 0xd387)
            sprintf(RealModelName, "DESR-7700");
        else if (ModelId == 0xd388)
            sprintf(RealModelName, "DESR-5700");
        // d389 - d400 ??
        else if (ModelId == 0xd401)
            sprintf(RealModelName, "SCPH-50001/N");
        else if (ModelId == 0xd402)
            sprintf(RealModelName, "SCPH-50010/N");
        /* else if (ModelId == 0xd403)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd404)
            sprintf(RealModelName, "SCPH-50000 MB/NH");
        else if (ModelId == 0xd405)
            sprintf(RealModelName, "SCPH-50002");
        else if (ModelId == 0xd406)
            sprintf(RealModelName, "SCPH-50003");
        else if (ModelId == 0xd407)
            sprintf(RealModelName, "SCPH-50004");
        else if (ModelId == 0xd408)
            sprintf(RealModelName, "SCPH-50002 SS");
        else if (ModelId == 0xd409)
            sprintf(RealModelName, "SCPH-50003");
        else if (ModelId == 0xd40a)
            sprintf(RealModelName, "SCPH-50004 SS");
        else if (ModelId == 0xd40b)
            sprintf(RealModelName, "SCPH-50001");
        /* else if (ModelId == 0xd40c)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd40d)
            sprintf(RealModelName, "SCPH-50006");
        /* else if (ModelId == 0xd40e)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd40f)
            sprintf(RealModelName, "SCPH-50008");
        /* else if (ModelId == 0xd410)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd411)
            sprintf(RealModelName, "SCPH-50000 NB");
        /* else if (ModelId == 0xd412)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd413)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd414)
            sprintf(RealModelName, "SCPH-55000 GT");
        else if (ModelId == 0xd415)
            sprintf(RealModelName, "SCPH-50009 SS");
        /* else if (ModelId == 0xd416)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd417)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd418)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd419)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd41a)
            sprintf(RealModelName, "SCPH-50008 SS");
        else if (ModelId == 0xd41b)
            sprintf(RealModelName, "SCPH-50004 AQ");
        else if (ModelId == 0xd41c)
            sprintf(RealModelName, "SCPH-50005 SS/N");
        /* else if (ModelId == 0xd41d)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd41e)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd41f)
            sprintf(RealModelName, "SCPH-50000 SA");
        else if (ModelId == 0xd420)
            sprintf(RealModelName, "SCPH-50004 SS");
        /* else if (ModelId == 0xd421)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd422)
            sprintf(RealModelName, "SCPH-50002 SS");
        else if (ModelId == 0xd423)
            sprintf(RealModelName, "SCPH-50003 SS");
        /* else if (ModelId == 0xd424)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd425)
            sprintf(RealModelName, "SCPH-50011");
        else if (ModelId == 0xd426)
            sprintf(RealModelName, "SCPH-70004");
        else if (ModelId == 0xd427)
            sprintf(RealModelName, "SCPH-70003");
        else if (ModelId == 0xd428)
            sprintf(RealModelName, "SCPH-70002");
        else if (ModelId == 0xd429)
            sprintf(RealModelName, "SCPH-70011");
        else if (ModelId == 0xd42a)
            sprintf(RealModelName, "SCPH-70012");
        else if (ModelId == 0xd42b)
            sprintf(RealModelName, "SCPH-70000");
        else if (ModelId == 0xd42c)
            sprintf(RealModelName, "SCPH-70005");
        else if (ModelId == 0xd42d)
            sprintf(RealModelName, "SCPH-70006");
        else if (ModelId == 0xd42e)
            sprintf(RealModelName, "SCPH-70007");
        /* else if (ModelId == 0xd42f)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd430)
            sprintf(RealModelName, "SCPH-70008");
        /* else if (ModelId == 0xd431)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd432)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd433)
            sprintf(RealModelName, "SCPH-70004 SS");
        /* else if (ModelId == 0xd434)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd435)
            sprintf(RealModelName, "SCPH-70001");
        /* else if (ModelId == 0xd436)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd437)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd438)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd439)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd43a)
            sprintf(RealModelName, "SCPH-70008");
        else if (ModelId == 0xd43b)
            sprintf(RealModelName, "SCPH-75001");
        else if (ModelId == 0xd43c)
            sprintf(RealModelName, "SCPH-75002");
        else if (ModelId == 0xd43d)
            sprintf(RealModelName, "SCPH-75003");
        else if (ModelId == 0xd43e)
            sprintf(RealModelName, "SCPH-75004");
        /* else if (ModelId == 0xd43f)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd440)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd441)
            sprintf(RealModelName, "SCPH-75003");
        else if (ModelId == 0xd442)
            sprintf(RealModelName, "SCPH-75004 SS");
        /* else if (ModelId == 0xd443)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd444)
            sprintf(RealModelName, "SCPH-75000 CW");
        else if (ModelId == 0xd445)
            sprintf(RealModelName, "SCPH-75006");
        /* else if (ModelId == 0xd446)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd447)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd448)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd449)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd44a)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd44b)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd44c)
            sprintf(RealModelName, "SCPH-75008");
        /* else if (ModelId == 0xd44d)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd44e)
            sprintf(RealModelName, "SCPH-77001");
        /* else if (ModelId == 0xd44f)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd450)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd451)
            sprintf(RealModelName, "SCPH-77004");
        else if (ModelId == 0xd452)
            sprintf(RealModelName, "SCPH-77002 CW");
        /* else if (ModelId == 0xd453)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd454)
            sprintf(RealModelName, "SCPH-77004 SS");
        /* else if (ModelId == 0xd455)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd456)
            sprintf(RealModelName, "SCPH-77000 CW");
        /* else if (ModelId == 0xd457)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd458)
            sprintf(RealModelName, "SCPH-77006");
        else if (ModelId == 0xd459)
            sprintf(RealModelName, "SCPH-77007");
        else if (ModelId == 0xd45a)
            sprintf(RealModelName, "SCPH-77008");
        /* else if (ModelId == 0xd45b)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd45c)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd45d)
            sprintf(RealModelName, "SCPH-77001 SS");
        else if (ModelId == 0xd45e)
            sprintf(RealModelName, "SCPH-77003");
        else if (ModelId == 0xd45f)
            sprintf(RealModelName, "SCPH-77004 PK");
        /* else if (ModelId == 0xd460)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd461)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd462)
            sprintf(RealModelName, "SCPH-77000 PK");
        /* else if (ModelId == 0xd463)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd464)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd465)
            sprintf(RealModelName, "SCPH-79001");
        /* else if (ModelId == 0xd466)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd467)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd468)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd469)
            sprintf(RealModelName, "SCPH-79001");
        /* else if (ModelId == 0xd46a)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd46b)
            sprintf(RealModelName, "SCPH-79006");
        /* else if (ModelId == 0xd46c)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd46d)
            sprintf(RealModelName, "SCPH-79000 SS");
        else if (ModelId == 0xd46e)
            sprintf(RealModelName, "SCPH-79003");
        /* else if (ModelId == 0xd46f)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd470)
            sprintf(RealModelName, "SCPH-79010");
        /* else if (ModelId == 0xd471)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd472)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd473)
            sprintf(RealModelName, "SCPH-79008");
        /* else if (ModelId == 0xd474)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd475)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd476)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd477)
            sprintf(RealModelName, "SCPH-90000 SS");
        else if (ModelId == 0xd478)
            sprintf(RealModelName, "SCPH-90006");
        else if (ModelId == 0xd479)
            sprintf(RealModelName, "SCPH-90006 CW");
        /* else if (ModelId == 0xd47a)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd47b)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd47c)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd47d)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd47e)
            sprintf(RealModelName, "SCPH-90007");
        /* else if (ModelId == 0xd47f)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd480)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd481)
            sprintf(RealModelName, "SCPH-90001");
        /* else if (ModelId == 0xd482)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd483)
            sprintf(RealModelName, "SCPH-90004");
        else if (ModelId == 0xd484)
            sprintf(RealModelName, "SCPH-90004 SS");
        else if (ModelId == 0xd485)
            sprintf(RealModelName, "SCPH-90002");
        /* else if (ModelId == 0xd486)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd487)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd488)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd489)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd48a)
            sprintf(RealModelName, "SCPH-90010");
        else if (ModelId == 0xd48b)
            sprintf(RealModelName, "SCPH-90000 CR");
        else if (ModelId == 0xd48c)
            sprintf(RealModelName, "SCPH-90008");
        /* else if (ModelId == 0xd48b)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd48c)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd48d)
               sprintf(RealModelName, "???");
           else if (ModelId == 0xd48e)
               sprintf(RealModelName, "???"); */
        else if (ModelId == 0xd48f)
            sprintf(RealModelName, "PX300-1");
        else
        {
            errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 5 * (reg_size + 4), reg_size, 0xFFFFFF, "Model ID unknown, please report!\n");
            drawFrame();

            freeGSTEXTURE_holder(versionTextures);
            freeGSTEXTURE_holder(buildTextures);
            freeGSTEXTURE_holder(errorTextures);
            freeGSTEXTURE_holder(serialTextures);
            freeGSTEXTURE_holder(ModelIDTextures);

            SleepThread();
            return;
        }
        struct GSTEXTURE_holder *modelnameTextures = ui_printf(8, 8 + big_size + big_size / 2 + 4 * (reg_size + 4), reg_size, 0xFFFFFF, "Real Model Name: %s\n", RealModelName);

        if (!getPatch(build_date))
        {
            errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 5 * (reg_size + 4), reg_size, 0xFFFFFF, "MechaCon unknown, please report!\n");
            drawFrame();

            freeGSTEXTURE_holder(versionTextures);
            freeGSTEXTURE_holder(buildTextures);
            freeGSTEXTURE_holder(errorTextures);
            freeGSTEXTURE_holder(serialTextures);
            freeGSTEXTURE_holder(ModelIDTextures);
            freeGSTEXTURE_holder(modelnameTextures);

            SleepThread();
            return;
        }
        else if (!isPatchKnown())
        {
            errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 5 * (reg_size + 4), reg_size, 0xFFFFFF, "Unknown patch, please report!\n");
            drawFrame();

            freeGSTEXTURE_holder(versionTextures);
            freeGSTEXTURE_holder(buildTextures);
            freeGSTEXTURE_holder(errorTextures);
            freeGSTEXTURE_holder(serialTextures);
            freeGSTEXTURE_holder(ModelIDTextures);
            freeGSTEXTURE_holder(modelnameTextures);

            SleepThread();
            return;
        }
        else if ((ModelId >= 0xd300) && (ModelId < 0xd380))
        {
            errorTextures = draw_text(8, 8 + big_size + big_size / 2 + 5 * (reg_size + 4), reg_size, 0xFFFFFF, "TEST/DTL unit, aborting!\n");
            drawFrame();

            freeGSTEXTURE_holder(versionTextures);
            freeGSTEXTURE_holder(buildTextures);
            freeGSTEXTURE_holder(errorTextures);
            freeGSTEXTURE_holder(serialTextures);
            freeGSTEXTURE_holder(ModelIDTextures);
            freeGSTEXTURE_holder(modelnameTextures);

            SleepThread();
            return;
        }
        else
        {
            struct GSTEXTURE_holder *exitTextures = draw_text(8, 8 + big_size + big_size / 2 + 6 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to continue.\n");
            drawFrame();

            freeGSTEXTURE_holder(versionTextures);
            freeGSTEXTURE_holder(buildTextures);
            freeGSTEXTURE_holder(exitTextures);
            freeGSTEXTURE_holder(serialTextures);
            freeGSTEXTURE_holder(ModelIDTextures);
            freeGSTEXTURE_holder(modelnameTextures);
        }
    }

    while (1)
    {
        u32 new_pad = ReadCombinedPadStatus();

        if (new_pad & PAD_CROSS)
            break;
    }
}

char isPatchAlreadyInstalled()
{
    uint8_t build_date[5];
    getMechaBuildDate(build_date);

    uint8_t current_patch[224];

    for (int i = 0; i < 112; i++)
    {
        if (!ReadNVM(400 + i, (uint16_t *)&current_patch[i * 2]))
            break;
    }

    const uint8_t *patch = getPatch(build_date);

    return memcmp(current_patch, patch, 224) == 0;
}

char restoreBackup()
{
    // TODO: maybe we can keep mechapwn setting in ciphertext?
    uint32_t serial[1];
    getSerial(serial);
    uint8_t version[4];
    getMechaVersion(version);

    char nvm_path[256];
    sprintf(nvm_path, "mass:/nvm_%d.%02d_%07ld.bin", version[1], (version[2] | 1) - 1, serial[0]);

    FILE *f = fopen(nvm_path, "rb");
    if (!f)
    {
        gsKit_clear(gsGlobal, Black);

        char text[] = "Failed to open NVRAM backup!";

        int x, y;
        getTextSize(reg_size, text, &x, &y);
        y += reg_size;

        struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, text);
        struct GSTEXTURE_holder *exitTextures = draw_text(8, 8 + big_size + big_size / 2 + 6 * (reg_size + 4), reg_size, 0xFFFFFF, "Press X to exit.\n");
        drawFrame();
        freeGSTEXTURE_holder(textTextures);
        freeGSTEXTURE_holder(exitTextures);
        while (1)
        {
            u32 new_pad = ReadCombinedPadStatus();

            if (new_pad & PAD_CROSS)
            {
                ResetIOP();
                LoadExecPS2("rom0:OSDSYS", 0, NULL);
                SleepThread();
            }
        }
        return 0;
    }
    else
    {
        fseek(f, 0, SEEK_END);
        int len = ftell(f);
        fclose(f);
        if (len != 1024)
        {
            gsKit_clear(gsGlobal, Black);

            char text[] = "Failed to open NVRAM backup!";

            int x, y;
            getTextSize(reg_size, text, &x, &y);
            y += reg_size;

            struct GSTEXTURE_holder *textTextures = draw_text((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, text);
            drawFrame();
            freeGSTEXTURE_holder(textTextures);
            return 0;
        }
    }


    int x, y;
    getTextSize(reg_size, "Restoring NVRAM backup...", &x, &y);
    y += reg_size;

    gsKit_clear(gsGlobal, Black);
    struct GSTEXTURE_holder *textTextures = ui_printf((gsGlobal->Width - x) / 2, (gsGlobal->Height - y) / 2, reg_size, 0xFFFFFF, "Restoring NVRAM backup...");
    drawFrame();
    freeGSTEXTURE_holder(textTextures);

    for (int i = 0; i < 0x200; i++)
    {

        uint16_t data;
        fread(&data, 1, 2, f);
        if (!WriteNVM(i, data))
            break;
    }

    fclose(f);
    return 1;
}

int main()
{
    init_ui();

    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    gsKit_clear(gsGlobal, Black);

    struct GSTEXTURE_holder *loadingTextures = draw_text(-8, -reg_size - 8, reg_size, 0xFFFFFF, "Loading...");
    drawFrame();
    freeGSTEXTURE_holder(loadingTextures);

    ResetIOP();

    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    sbv_patch_fileio();

    SifExecModuleBuffer(&iomanX, size_iomanX, 0, NULL, NULL);
    SifExecModuleBuffer(&fileXio, size_fileXio, 0, NULL, NULL);
    SifExecModuleBuffer(&freesio2, size_freesio2, 0, NULL, NULL);
    SifExecModuleBuffer(&freepad, size_freepad, 0, NULL, NULL);
    // SifExecModuleBuffer(&mcman, size_mcman, 0, NULL, NULL);
    // SifExecModuleBuffer(&mcserv, size_mcserv, 0, NULL, NULL);
    SifExecModuleBuffer(&USBD, size_USBD, 0, NULL, NULL);
    SifExecModuleBuffer(&USBHDFSD, size_USBHDFSD, 0, NULL, NULL);
    SifExecModuleBuffer(&MECHAPROXY_irx, size_MECHAPROXY_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&MASSWATCHER_irx, size_MASSWATCHER_irx, 0, NULL, NULL);

    MechaInit();
    MassInit();

    // mcInit(MC_TYPE_XMC);

    PadInitPads();

    // ---

    drawLogo();

    checkUnsupportedVersion();

    uint8_t *powerTexture = getPowerTexture();

    char rerun            = 0;
    if (!IsNVMUnlocked())
    {
        rerun = 1;
        if (!isPatchAlreadyInstalled())
        {
            if (backupNVM())
                unlockNVM();
        }
    }
    else
    {
        gsKit_clear(gsGlobal, Black);

        struct MENU menu;
        menu.title        = "MechaPwn";
        menu.x_text       = "X Select";
        menu.o_text       = "O Exit";
        menu.option_count = 2;

        menu.options[0]   = "Change region";
        menu.options[1]   = "Restore NVRAM backup";

        int selected      = drawMenu(&menu);

        if (selected == -1)
        {
            ResetIOP();
            LoadExecPS2("rom0:OSDSYS", 0, NULL);
            SleepThread();
        }
        else if (selected == 0)
        {
            char isDex = 0;
            setRegion(&isDex);
            applyPatches(isDex);
        }
        else if (selected == 1)
        {
            restoreBackup();
        }
    }

    gsKit_clear(gsGlobal, Black);

    struct GSTEXTURE_holder *imageTextures = drawImage((gsGlobal->Width - 400) / 2, (gsGlobal->Height - (225 + 60)) / 2, 400, 225, powerTexture);

    const char *text1                      = "Unplug the power cord.";
    int x1, y1;
    getTextSize(reg_size, text1, &x1, &y1);
    y1 += reg_size;
    struct GSTEXTURE_holder *unplugTextures = draw_text((gsGlobal->Width - x1) / 2, ((gsGlobal->Height - (225 + 60)) / 2) + 225, reg_size, 0xFFFFFF, text1);

    struct GSTEXTURE_holder *rerunTextures  = 0;
    if (rerun)
    {
        const char *text2 = "Run MechaPwn again.";
        int x2, y2;
        getTextSize(reg_size, text2, &x2, &y2);
        y2 += reg_size;
        rerunTextures = draw_text((gsGlobal->Width - x2) / 2, ((gsGlobal->Height - (195)) / 2) + 225, reg_size, 0xFFFFFF, text2);
    }

    drawFrame();
    if (rerunTextures)
        freeGSTEXTURE_holder(rerunTextures);
    freeGSTEXTURE_holder(unplugTextures);
    freeGSTEXTURE_holder(imageTextures);


    // ---

    SleepThread();

    return 0;
}
