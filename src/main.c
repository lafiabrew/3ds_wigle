#include "pk_platform.h"
#include "wg_wigle.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WG_VISIBLE_ROWS 7

typedef enum {
    WG_VIEW_SPECTRUM = 0,
    WG_VIEW_RADAR = 1,
    WG_VIEW_ACCOUNT = 2
} wg_view_mode_t;

typedef struct {
    wg_wardrive_state_t state;
    int selected_ap;
    int scroll_ap;
    bool auto_scan;
    wg_view_mode_t view_mode;
    uint32_t frame_counter;
    char status_msg[128];
} wg_app_t;

static void sync_scroll(int selected, int total, int visible, int *inout_scroll) {
    if (total <= visible || selected < 0) {
        *inout_scroll = 0;
        return;
    }
    if (selected < *inout_scroll) {
        *inout_scroll = selected;
    } else if (selected >= *inout_scroll + visible) {
        *inout_scroll = selected - visible + 1;
    }
    if (*inout_scroll > total - visible) {
        *inout_scroll = total - visible;
    }
    if (*inout_scroll < 0) {
        *inout_scroll = 0;
    }
}

static void trigger_sweep(wg_app_t *app) {
    char ts[24];
    wg_format_current_utc(ts);
    int n = wg_perform_sweep(&app->state, ts);
    snprintf(app->status_msg, sizeof(app->status_msg),
             "SWEEP #%u: +%d AP(S) | %d UNIQUE | %u CSV ROWS",
             (unsigned)app->state.total_scans, n,
             app->state.ap_count, (unsigned)app->state.csv_rows_written);
    pk_log_event(app->state.sd_root, "wigle", "INFO", app->status_msg);
}

static void wg_app_init(wg_app_t *app, const char *sd_root) {
    memset(app, 0, sizeof(*app));
    wg_init(&app->state, sd_root != NULL ? sd_root : pk_platform_default_sd_root(),
            "wigle_3ds_session.csv");
    app->view_mode = WG_VIEW_SPECTRUM;
    trigger_sweep(app);
}

static void wg_app_step(wg_app_t *app, const pk_input_t *in) {
    app->frame_counter++;

    if (app->auto_scan && (app->frame_counter % 120u) == 0u) {
        trigger_sweep(app);
    }

    if (in->down & PK_BTN_L) {
        app->view_mode = (wg_view_mode_t)((app->view_mode + 2) % 3);
    }
    if ((in->down & PK_BTN_R) || (in->down & PK_BTN_Y)) {
        app->view_mode = (wg_view_mode_t)((app->view_mode + 1) % 3);
    }

    if ((in->down & PK_BTN_DUP) && app->state.ap_count > 0) {
        app->selected_ap = (app->selected_ap > 0) ? (app->selected_ap - 1) : (app->state.ap_count - 1);
    }
    if ((in->down & PK_BTN_DDOWN) && app->state.ap_count > 0) {
        app->selected_ap = (app->selected_ap + 1 < app->state.ap_count) ? (app->selected_ap + 1) : 0;
    }
    if (in->down & PK_BTN_DLEFT) {
        app->state.cur_lon -= 0.0005;
    }
    if (in->down & PK_BTN_DRIGHT) {
        app->state.cur_lon += 0.0005;
    }
    sync_scroll(app->selected_ap, app->state.ap_count, WG_VISIBLE_ROWS, &app->scroll_ap);

    if (in->down & PK_BTN_A) {
        trigger_sweep(app);
    }
    if (in->down & PK_BTN_B) {
        wg_load_account(&app->state);
        app->view_mode = WG_VIEW_ACCOUNT;
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "WIGLE USER: %s | RANK: #%u | WIFI: %llu",
                 app->state.account.username, (unsigned)app->state.account.rank,
                 (unsigned long long)app->state.account.discovered_wifi);
        pk_log_event(app->state.sd_root, "wigle", "INFO", app->status_msg);
    }
    if (in->down & PK_BTN_X) {
        app->auto_scan = !app->auto_scan;
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "AUTO-WARDRIVE: %s (EVERY 2S)", app->auto_scan ? "ON" : "OFF");
        pk_log_event(app->state.sd_root, "wigle", "INFO", app->status_msg);
    }
    if (in->down & PK_BTN_SELECT) {
        wg_reset_session(&app->state);
        app->selected_ap = 0;
        app->scroll_ap = 0;
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "NEW WIGLE CSV SESSION STARTED (0 ROWS)");
        pk_log_event(app->state.sd_root, "wigle", "INFO", app->status_msg);
    }

    if (in->touch_just_pressed) {
        if (in->touch_y >= 4 && in->touch_y <= 32) {
            if (in->touch_x < 106) {
                trigger_sweep(app);
            } else if (in->touch_x < 212) {
                app->auto_scan = !app->auto_scan;
                snprintf(app->status_msg, sizeof(app->status_msg),
                         "AUTO-WARDRIVE: %s", app->auto_scan ? "ON" : "OFF");
            } else {
                app->view_mode = (wg_view_mode_t)((app->view_mode + 1) % 3);
            }
        } else if (in->touch_y >= 36 && in->touch_y < 36 + WG_VISIBLE_ROWS * 24) {
            int vis_row = (in->touch_y - 36) / 24;
            int idx = app->scroll_ap + vis_row;
            if (idx >= 0 && idx < app->state.ap_count) {
                app->selected_ap = idx;
                sync_scroll(app->selected_ap, app->state.ap_count, WG_VISIBLE_ROWS, &app->scroll_ap);
            }
        }
    }
}

static void wg_app_render(const wg_app_t *app) {
    pk_render_begin_frame();

    /* Top Screen (400x240) */
    pk_render_begin_target(PK_TARGET_TOP_LEFT, PK_RGBA8(8, 18, 15, 255));
    pk_draw_rect(0, 0, 400, 26, PK_RGBA8(16, 46, 36, 255));
    pk_draw_text(8, 7, 0.48f, PK_RGBA8(85, 255, 165, 255),
                 app->view_mode == WG_VIEW_SPECTRUM
                     ? "WIGLE 3DS - 2.4GHZ SPECTRUM & CSV LOGGER"
                     : (app->view_mode == WG_VIEW_RADAR
                            ? "WIGLE 3DS - POLAR RADAR & BEACON TRACKER"
                            : "WIGLE 3DS - ACCOUNT RANK & UPLOAD SYNC"));

    char line[160];
    snprintf(line, sizeof(line), "USER:%s (#%u) | APS:%d | CSV:%u | AUTO:%s",
             app->state.account.username, (unsigned)app->state.account.rank,
             app->state.ap_count, (unsigned)app->state.csv_rows_written,
             app->auto_scan ? "ON" : "OFF");
    pk_draw_text(8, 32, 0.44f, PK_RGBA8(185, 240, 210, 255), line);

    if (app->view_mode == WG_VIEW_SPECTRUM) {
        pk_draw_rect(8, 48, 384, 84, PK_RGBA8(14, 28, 23, 255));
        pk_draw_rect_outline(8, 48, 384, 84, 1.0f, PK_RGBA8(50, 145, 108, 255));
        for (int ch = 1; ch <= 13; ch++) {
            float bx = 14.0f + (float)(ch - 1) * 28.5f;
            int best_rssi = -100;
            for (int i = 0; i < app->state.ap_count; i++) {
                if (app->state.aps[i].channel == ch && app->state.aps[i].rssi_dbm > best_rssi) {
                    best_rssi = app->state.aps[i].rssi_dbm;
                }
            }
            float bar_h = best_rssi > -95 ? (float)(best_rssi + 100) * 0.85f : 4.0f;
            if (bar_h > 54.0f) bar_h = 54.0f;
            uint32_t bar_col = (best_rssi > -60)
                                   ? PK_RGBA8(85, 255, 155, 255)
                                   : (best_rssi > -95 ? PK_RGBA8(255, 215, 85, 255)
                                                      : PK_RGBA8(32, 60, 50, 255));
            pk_draw_rect(bx, 112.0f - bar_h, 20.0f, bar_h, bar_col);
            char ch_lbl[8];
            snprintf(ch_lbl, sizeof(ch_lbl), "%02d", ch);
            pk_draw_text(bx + 2.0f, 116.0f, 0.40f, PK_RGBA8(160, 215, 190, 255), ch_lbl);
        }
    } else if (app->view_mode == WG_VIEW_RADAR) {
        pk_draw_rect(8, 48, 384, 84, PK_RGBA8(10, 24, 18, 255));
        pk_draw_rect_outline(8, 48, 384, 84, 1.0f, PK_RGBA8(65, 210, 135, 255));
        pk_draw_rect(200, 50, 1, 80, PK_RGBA8(35, 95, 65, 255));
        pk_draw_rect(12, 90, 376, 1, PK_RGBA8(35, 95, 65, 255));
        for (int i = 0; i < app->state.ap_count; i++) {
            const wg_ap_entry_t *ap = &app->state.aps[i];
            float px = 24.0f + (float)((ap->channel - 1) * 28) + (float)((i * 13) % 11);
            float py = 56.0f + (float)((-30 - ap->rssi_dbm) * 64) / 70.0f;
            if (py < 54.0f) py = 54.0f;
            if (py > 122.0f) py = 122.0f;
            bool sel = (i == app->selected_ap);
            pk_draw_rect(px - (sel ? 4.0f : 3.0f), py - (sel ? 4.0f : 3.0f),
                         sel ? 8.0f : 6.0f, sel ? 8.0f : 6.0f,
                         sel ? PK_RGBA8(255, 235, 85, 255) : PK_RGBA8(80, 255, 155, 255));
        }
    } else {
        /* WiGLE Account & Points Sync Card */
        pk_draw_rect(8, 48, 384, 84, PK_RGBA8(14, 34, 28, 255));
        pk_draw_rect_outline(8, 48, 384, 84, 1.0f, PK_RGBA8(95, 245, 165, 255));
        snprintf(line, sizeof(line), "WIGLE.NET ACCOUNT: %s  (API: %s)",
                 app->state.account.username, app->state.account.api_name);
        pk_draw_text(14, 56, 0.45f, PK_RGBA8(255, 235, 100, 255), line);
        snprintf(line, sizeof(line), "GLOBAL RANK: #%u | MONTH RANK: #%u | WIFI: %llu",
                 (unsigned)app->state.account.rank,
                 (unsigned)app->state.account.month_rank,
                 (unsigned long long)app->state.account.discovered_wifi);
        pk_draw_text(14, 76, 0.44f, PK_RGBA8(140, 255, 185, 255), line);
        snprintf(line, sizeof(line), "UPLOADS: %u | LAST SYNC: %s",
                 (unsigned)app->state.account.total_uploads,
                 app->state.account.last_sync_utc);
        pk_draw_text(14, 96, 0.43f, PK_RGBA8(190, 230, 255, 255), line);
        pk_draw_text(14, 114, 0.40f, PK_RGBA8(210, 220, 215, 255),
                     "SYNC PC: python3 pc_companion/wigle_sync.py --url http://<3ds-ip>:8766");
    }

    /* Selected AP Inspector Card on Top Screen (y: 136..210) */
    pk_draw_rect(8, 136, 384, 74, PK_RGBA8(14, 30, 25, 255));
    pk_draw_rect_outline(8, 136, 384, 74, 1.0f, PK_RGBA8(55, 145, 110, 255));
    if (app->state.ap_count > 0 && app->selected_ap < app->state.ap_count) {
        const wg_ap_entry_t *ap = &app->state.aps[app->selected_ap];
        snprintf(line, sizeof(line), "[%d/%d] SSID: %s  (%s)",
                 app->selected_ap + 1, app->state.ap_count, ap->ssid, ap->bssid);
        pk_draw_text(14, 142, 0.46f, PK_RGBA8(255, 235, 105, 255), line);

        snprintf(line, sizeof(line), "CH %02d (%d MHZ) | %d DBM | SEEN:%uX | %s",
                 ap->channel, wg_channel_to_freq_mhz(ap->channel),
                 ap->rssi_dbm, (unsigned)ap->seen_count,
                 wg_auth_to_wigle_caps(ap->auth));
        pk_draw_text(14, 162, 0.43f, PK_RGBA8(165, 235, 250, 255), line);

        snprintf(line, sizeof(line), "SEEN: %s | B:RELOAD WIGLE RANK | SEL:NEW CSV",
                 ap->first_seen_utc);
        pk_draw_text(14, 182, 0.42f, PK_RGBA8(170, 215, 190, 255), line);
    } else {
        pk_draw_text(14, 164, 0.46f, PK_RGBA8(255, 175, 120, 255),
                     "NO NETWORKS LOGGED YET - PRESS A OR TAP SWEEP");
    }

    pk_draw_rect(0, 216, 400, 24, PK_RGBA8(14, 38, 30, 255));
    pk_draw_text(8, 222, 0.44f, PK_RGBA8(110, 255, 175, 255), app->status_msg);

    /* Bottom Screen (320x240): Touch Action Bar + Scrollable AP Table + Scrollbar */
    pk_render_begin_target(PK_TARGET_BOTTOM, PK_RGBA8(8, 14, 12, 255));

    pk_draw_rect(4, 4, 100, 26, PK_RGBA8(32, 115, 80, 255));
    pk_draw_text(10, 11, 0.44f, PK_RGBA8(255, 255, 255, 255), "A: SWEEP NOW");

    pk_draw_rect(110, 4, 100, 26,
                 app->auto_scan ? PK_RGBA8(205, 135, 35, 255) : PK_RGBA8(26, 52, 42, 255));
    pk_draw_text(116, 11, 0.44f, PK_RGBA8(255, 255, 255, 255),
                 app->auto_scan ? "X: AUTO [ON]" : "X: AUTO [OFF]");

    pk_draw_rect(216, 4, 100, 26, PK_RGBA8(38, 78, 125, 255));
    pk_draw_text(222, 11, 0.44f, PK_RGBA8(255, 255, 255, 255),
                 app->view_mode == WG_VIEW_SPECTRUM
                     ? "Y: RADAR VIEW"
                     : (app->view_mode == WG_VIEW_RADAR ? "Y: ACCT RANK" : "Y: BARS VIEW"));

    for (int row = 0; row < WG_VISIBLE_ROWS; row++) {
        int idx = app->scroll_ap + row;
        if (idx >= app->state.ap_count) break;
        const wg_ap_entry_t *ap = &app->state.aps[idx];
        float y = 36.0f + (float)row * 24.0f;
        bool sel = (idx == app->selected_ap);
        pk_draw_rect(4.0f, y, 302.0f, 21.0f,
                     sel ? PK_RGBA8(34, 105, 78, 255) : PK_RGBA8(16, 28, 24, 255));

        uint32_t sig_col = (ap->rssi_dbm >= -60)
                               ? PK_RGBA8(80, 255, 145, 255)
                               : (ap->rssi_dbm >= -75 ? PK_RGBA8(255, 215, 80, 255)
                                                      : PK_RGBA8(255, 110, 95, 255));
        pk_draw_rect(6.0f, y + 2.0f, 4.0f, 17.0f, sig_col);

        snprintf(line, sizeof(line), "%02d.CH%02d %4ddBm %-13.13s [%s]",
                 idx + 1, ap->channel, ap->rssi_dbm, ap->ssid, wg_auth_short_label(ap->auth));
        pk_draw_text(14.0f, y + 6.0f, 0.43f,
                     sel ? PK_RGBA8(255, 245, 155, 255) : PK_RGBA8(225, 245, 235, 255),
                     line);
    }
    pk_draw_scrollbar(310.0f, 36.0f, 165.0f, app->state.ap_count, WG_VISIBLE_ROWS, app->scroll_ap,
                      PK_RGBA8(18, 34, 28, 255), PK_RGBA8(85, 245, 155, 255));

    pk_draw_rect(0, 214, 320, 26, PK_RGBA8(14, 26, 22, 255));
    pk_draw_text(8, 221, 0.42f, PK_RGBA8(165, 205, 185, 255),
                 "U/D:SCROLL | B:WIGLE ACCT | SEL:NEW CSV | START:EXIT");

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
