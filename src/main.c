#include "pk_platform.h"
#include "wg_wigle.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    wg_wardrive_state_t state;
    int selected_ap;
    bool auto_scan;
    char status_msg[128];
} wg_app_t;

static void wg_app_init(wg_app_t *app, const char *sd_root) {
    memset(app, 0, sizeof(*app));
    wg_init(&app->state, sd_root != NULL ? sd_root : pk_platform_default_sd_root(),
            "wigle_3ds_session.csv");
    int n = wg_perform_sweep(&app->state, "2026-11-02 05:30:00");
    snprintf(app->status_msg, sizeof(app->status_msg),
             "WIGLE-WIFI-1.4 READY: %d APS LOGGED TO CSV", n);
}

static void wg_app_step(wg_app_t *app, const pk_input_t *in) {
    if ((in->down & PK_BTN_DUP) && app->selected_ap > 0) {
        app->selected_ap--;
    }
    if ((in->down & PK_BTN_DDOWN) && app->selected_ap + 1 < app->state.ap_count) {
        app->selected_ap++;
    }
    if (in->down & PK_BTN_DLEFT) {
        app->state.cur_lon -= 0.0005;
    }
    if (in->down & PK_BTN_DRIGHT) {
        app->state.cur_lon += 0.0005;
    }
    if (in->down & PK_BTN_A) {
        int n = wg_perform_sweep(&app->state, "2026-11-02 05:31:00");
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "SWEEP COMPLETE: +%d OBSERVATIONS (TOTAL ROWS: %u)",
                 n, (unsigned)app->state.csv_rows_written);
    }
    if (in->down & PK_BTN_X) {
        app->auto_scan = !app->auto_scan;
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "CONTINUOUS WARDRIVE: %s", app->auto_scan ? "ON" : "OFF");
    }
}

static void wg_app_render(const wg_app_t *app) {
    pk_render_begin_frame();

    /* Top Screen (400x240): 2.4 GHz Channel Spectrum + Selected AP Telemetry */
    pk_render_begin_target(PK_TARGET_TOP_LEFT, PK_RGBA8(10, 20, 18, 255));
    pk_draw_rect(0, 0, 400, 26, PK_RGBA8(18, 48, 40, 255));
    pk_draw_text(8, 7, 0.55f, PK_RGBA8(90, 255, 170, 255),
                 "WIGLE 3DS WARDRIVER - 2.4GHZ & UDS RADAR (V1.4 CSV)");

    char line[160];
    snprintf(line, sizeof(line), "GPS: %.5f, %.5f (+/-%.1fM) | APS: %d | CSV ROWS: %u",
             app->state.cur_lat, app->state.cur_lon, app->state.cur_accuracy_m,
             app->state.ap_count, (unsigned)app->state.csv_rows_written);
    pk_draw_text(8, 34, 0.48f, PK_RGBA8(190, 245, 215, 255), line);

    /* 2.4 GHz Ch 1..13 Spectrum Bars */
    pk_draw_rect(8, 52, 384, 72, PK_RGBA8(16, 30, 26, 255));
    pk_draw_rect_outline(8, 52, 384, 72, 1.0f, PK_RGBA8(55, 150, 115, 255));
    for (int ch = 1; ch <= 13; ch++) {
        float bx = 16.0f + (float)(ch - 1) * 28.0f;
        int best_rssi = -100;
        for (int i = 0; i < app->state.ap_count; i++) {
            if (app->state.aps[i].channel == ch && app->state.aps[i].rssi_dbm > best_rssi) {
                best_rssi = app->state.aps[i].rssi_dbm;
            }
        }
        float bar_h = best_rssi > -95 ? (float)(best_rssi + 100) * 0.7f : 4.0f;
        if (bar_h > 48.0f) bar_h = 48.0f;
        pk_draw_rect(bx, 108.0f - bar_h, 18.0f, bar_h,
                     best_rssi > -95 ? PK_RGBA8(80, 245, 155, 255) : PK_RGBA8(35, 65, 55, 255));
    }

    if (app->state.ap_count > 0) {
        const wg_ap_entry_t *ap = &app->state.aps[app->selected_ap];
        snprintf(line, sizeof(line), "SSID: %s  [%s]", ap->ssid, ap->bssid);
        pk_draw_text(10, 134, 0.52f, PK_RGBA8(255, 235, 110, 255), line);
        snprintf(line, sizeof(line), "AUTH: %s  CH:%d (%d MHZ)  RSSI:%d DBM",
                 wg_auth_to_wigle_caps(ap->auth), ap->channel,
                 wg_channel_to_freq_mhz(ap->channel), ap->rssi_dbm);
        pk_draw_text(10, 154, 0.48f, PK_RGBA8(170, 235, 250, 255), line);
    }

    pk_draw_text(10, 188, 0.48f, PK_RGBA8(165, 205, 185, 255),
                 "A: SWEEP NOW | X: AUTO-WARDRIVE | D-PAD: SELECT AP / NUDGE GPS");

    pk_draw_rect(0, 216, 400, 24, PK_RGBA8(16, 36, 30, 255));
    pk_draw_text(8, 222, 0.48f, PK_RGBA8(110, 255, 175, 255), app->status_msg);

    /* Bottom Screen (320x240): Discovered AP List */
    pk_render_begin_target(PK_TARGET_BOTTOM, PK_RGBA8(10, 16, 15, 255));
    for (int i = 0; i < app->state.ap_count && i < 9; i++) {
        float y = 8.0f + (float)i * 22.0f;
        bool sel = (i == app->selected_ap);
        pk_draw_rect(6.0f, y, 308.0f, 19.0f,
                     sel ? PK_RGBA8(35, 110, 82, 255) : PK_RGBA8(18, 32, 28, 255));
        snprintf(line, sizeof(line), "CH%02d %ddBm %-16.16s %s",
                 app->state.aps[i].channel, app->state.aps[i].rssi_dbm,
                 app->state.aps[i].ssid, wg_auth_to_wigle_caps(app->state.aps[i].auth));
        pk_draw_text(10.0f, y + 5.0f, 0.45f, PK_RGBA8(230, 250, 240, 255), line);
    }

    pk_render_end_frame();
}

int main(int argc, char **argv) {
    const char *sd_root = NULL;
    bool one_shot = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sd-root") == 0 && i + 1 < argc) {
            sd_root = argv[++i];
        } else if (strcmp(argv[i], "--once") == 0) {
            one_shot = true;
        }
    }

    if (!pk_platform_init()) return 1;
    wg_app_t app;
    wg_app_init(&app, sd_root);

    while (pk_platform_main_loop()) {
        pk_input_t in;
        pk_platform_poll_input(&in);
        if (in.down & PK_BTN_START) break;
        wg_app_step(&app, &in);
        wg_app_render(&app);
        if (one_shot) break;
    }

    pk_platform_shutdown();
    return 0;
}
