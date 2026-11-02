#include "wg_wigle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __3DS__
#include <3ds.h>
#endif

static void ensure_dir_p(const char *dir_path) {
    char tmp[WG_PATH_MAX + 64];
    snprintf(tmp, sizeof(tmp), "%s", dir_path);
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

const char *wg_auth_to_wigle_caps(wg_auth_mode_t auth) {
    switch (auth) {
    case WG_AUTH_OPEN:      return "[ESS]";
    case WG_AUTH_WEP_40:
    case WG_AUTH_WEP_104:
    case WG_AUTH_WEP_128:   return "[WEP][ESS]";
    case WG_AUTH_WPA_TKIP:  return "[WPA-PSK-TKIP][ESS]";
    case WG_AUTH_WPA2_TKIP: return "[WPA2-PSK-TKIP][ESS]";
    case WG_AUTH_WPA_AES:   return "[WPA-PSK-CCMP][ESS]";
    case WG_AUTH_WPA2_AES:  return "[WPA2-PSK-CCMP][ESS]";
    case WG_AUTH_3DS_UDS:   return "[NINTENDO-3DS-UDS][IBSS]";
    default:                return "[WPA2-PSK-CCMP][ESS]";
    }
}

const char *wg_auth_short_label(wg_auth_mode_t auth) {
    switch (auth) {
    case WG_AUTH_OPEN:      return "OPEN";
    case WG_AUTH_WEP_40:
    case WG_AUTH_WEP_104:
    case WG_AUTH_WEP_128:   return "WEP";
    case WG_AUTH_WPA_TKIP:
    case WG_AUTH_WPA_AES:   return "WPA";
    case WG_AUTH_WPA2_TKIP:
    case WG_AUTH_WPA2_AES:  return "WPA2";
    case WG_AUTH_3DS_UDS:   return "3DS-UDS";
    default:                return "WPA2";
    }
}

void wg_format_current_utc(char out_ts[24]) {
    if (out_ts == NULL) return;
    time_t now = time(NULL);
    struct tm *tm_utc = gmtime(&now);
    if (tm_utc != NULL && tm_utc->tm_year >= 120 &&
        strftime(out_ts, 24, "%Y-%m-%d %H:%M:%S", tm_utc) == 19) {
        return;
    }
    memcpy(out_ts, "2026-11-02 08:30:00", 20);
}

bool wg_reset_session(wg_wardrive_state_t *st) {
    if (st == NULL || st->session_csv_path[0] == '\0') return false;
    st->ap_count = 0;
    st->total_scans = 0;
    st->csv_rows_written = 0;
    memset(st->aps, 0, sizeof(st->aps));

    FILE *fp = fopen(st->session_csv_path, "wb");
    if (fp == NULL) return false;
    fprintf(fp,
            "WigleWifi-1.4,appRelease=1.0,model=Nintendo3DS,release=11.17.0-50U,"
            "device=3DS_WiGLE_Radar,display=DualScreen_400x240,board=ARM11_MPCore,brand=Nintendo\n"
            "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,"
            "AltitudeMeters,AccuracyMeters,Type\n");
    fflush(fp);
    fclose(fp);
    return true;
}

int wg_channel_to_freq_mhz(int channel) {
    if (channel == 14) return 2484;
    if (channel >= 1 && channel <= 13) return 2407 + channel * 5;
    return 2412;
}

static void sanitize_csv_field(const char *in, char *out, size_t cap) {
    if (out == NULL || cap == 0) return;
    size_t oi = 0;
    if (in != NULL) {
        for (size_t i = 0; in[i] != '\0' && oi + 1 < cap; i++) {
            char c = in[i];
            if (c == ',' || c == '\r' || c == '\n' || c == '"') {
                out[oi++] = '_';
            } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
                out[oi++] = c;
            }
        }
    }
    out[oi] = '\0';
}

bool wg_init(wg_wardrive_state_t *st, const char *sd_root, const char *session_name) {
    if (st == NULL) return false;
    memset(st, 0, sizeof(*st));
    snprintf(st->sd_root, sizeof(st->sd_root), "%s", sd_root != NULL ? sd_root : "./sdmc");

    /* Default starting coordinates (can be updated via D-Pad, touch, or phone GPS JSON bridge) */
    st->cur_lat = 37.774900;
    st->cur_lon = -122.419400;
    st->cur_alt_m = 18.0;
    st->cur_accuracy_m = 6.5;
    st->gps_fix_active = true;

    char wigle_dir[WG_PATH_MAX + 32];
    snprintf(wigle_dir, sizeof(wigle_dir), "%s/3ds/wigle", st->sd_root);
    ensure_dir_p(wigle_dir);

    snprintf(st->session_csv_path, sizeof(st->session_csv_path),
             "%s/%s", wigle_dir,
             (session_name != NULL && session_name[0] != '\0') ? session_name : "wigle_3ds_session.csv");

    /* Write official WiGLE-Wifi-1.4 2-line header if file does not exist yet */
    struct stat sb;
    if (stat(st->session_csv_path, &sb) != 0 || sb.st_size == 0) {
        FILE *fp = fopen(st->session_csv_path, "wb");
        if (fp == NULL) return false;
        fprintf(fp,
                "WigleWifi-1.4,appRelease=1.0,model=Nintendo3DS,release=11.17.0-50U,"
                "device=3DS_WiGLE_Radar,display=DualScreen_400x240,board=ARM11_MPCore,brand=Nintendo\n"
                "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,"
                "AltitudeMeters,AccuracyMeters,Type\n");
        fflush(fp);
        fclose(fp);
    }
    return true;
}

bool wg_record_observation(wg_wardrive_state_t *st,
                           const char *bssid,
                           const char *ssid,
                           wg_auth_mode_t auth,
                           int channel,
                           int rssi_dbm,
                           const char *utc_timestamp,
                           bool is_3ds_uds) {
    if (st == NULL || bssid == NULL || bssid[0] == '\0') return false;

    char clean_bssid[20];
    char clean_ssid[36];
    sanitize_csv_field(bssid, clean_bssid, sizeof(clean_bssid));
    sanitize_csv_field(ssid != NULL ? ssid : "<HIDDEN>", clean_ssid, sizeof(clean_ssid));
    const char *ts = (utc_timestamp != NULL && utc_timestamp[0] != '\0')
                         ? utc_timestamp
                         : "2026-11-02 05:30:00";

    /* Update existing AP entry or insert a new one */
    int found_idx = -1;
    for (int i = 0; i < st->ap_count; i++) {
        if (strcmp(st->aps[i].bssid, clean_bssid) == 0) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < 0 && st->ap_count < WG_MAX_APS) {
        found_idx = st->ap_count++;
        wg_ap_entry_t fresh;
        memset(&fresh, 0, sizeof(fresh));
        memcpy(fresh.bssid, clean_bssid, sizeof(fresh.bssid));
        size_t tlen = strlen(ts);
        if (tlen >= sizeof(fresh.first_seen_utc)) tlen = sizeof(fresh.first_seen_utc) - 1;
        memcpy(fresh.first_seen_utc, ts, tlen);
        fresh.first_seen_utc[tlen] = '\0';
        st->aps[found_idx] = fresh;
    }
    if (found_idx >= 0 && found_idx < WG_MAX_APS) {
        wg_ap_entry_t *entry = &st->aps[found_idx];
        memcpy(entry->ssid, clean_ssid, sizeof(entry->ssid));
        entry->auth = auth;
        entry->channel = (channel >= 1 && channel <= 14) ? channel : 6;
        entry->rssi_dbm = rssi_dbm;
        entry->lat = st->cur_lat;
        entry->lon = st->cur_lon;
        entry->alt_m = st->cur_alt_m;
        entry->accuracy_m = st->cur_accuracy_m;
        entry->seen_count++;
        entry->is_3ds_uds = is_3ds_uds;
    }

    /* Append WiGLE v1.4 CSV row */
    FILE *fp = fopen(st->session_csv_path, "ab");
    if (fp == NULL) return false;
    fprintf(fp, "%s,%s,%s,%s,%d,%d,%.6f,%.6f,%.1f,%.1f,WIFI\n",
            clean_bssid,
            clean_ssid,
            wg_auth_to_wigle_caps(auth),
            ts,
            (channel >= 1 && channel <= 14) ? channel : 6,
            rssi_dbm,
            st->cur_lat,
            st->cur_lon,
            st->cur_alt_m,
            st->cur_accuracy_m);
    fflush(fp);
    fclose(fp);
    st->csv_rows_written++;
    return true;
}

int wg_perform_sweep(wg_wardrive_state_t *st, const char *utc_timestamp) {
    if (st == NULL) return 0;
    st->total_scans++;
    int added = 0;

#ifdef __3DS__
    char now_ts[24];
    if (utc_timestamp == NULL || utc_timestamp[0] == '\0') {
        wg_format_current_utc(now_ts);
        utc_timestamp = now_ts;
    }

    /* 1. Query active connected 3DS Wi-Fi AP via ACU service */
    u32 wifi_status = 0;
    if (R_SUCCEEDED(ACU_GetStatus(&wifi_status)) && wifi_status == 3) {
        char ssid[36];
        memset(ssid, 0, sizeof(ssid));
        ACU_GetSSID(ssid);
        acSecurityMode sec = AC_WPA2_AES;
        ACU_GetSecurityMode(&sec);
        u8 strength = osGetWifiStrength(); /* 0..3 bars */
        int rssi = -82 + (int)strength * 14;
        if (wg_record_observation(st, "3C:3DS:AC:00:00:01",
                                  ssid[0] ? ssid : "3DS_ACTIVE_AP",
                                  (wg_auth_mode_t)sec, 6, rssi, utc_timestamp, false)) {
            added++;
        }
    }

    /* 2. Also inspect saved 3DS System Settings Wi-Fi slots 0..2 via ACI so the user
          always sees their configured networks even when out of range */
    for (u32 slot = 0; slot < 3; slot++) {
        if (R_SUCCEEDED(ACI_LoadNetworkSetting(slot))) {
            char slot_ssid[36];
            memset(slot_ssid, 0, sizeof(slot_ssid));
            if (R_SUCCEEDED(ACI_GetNetworkWirelessEssidSecuritySsid(slot_ssid)) && slot_ssid[0] != '\0') {
                char slot_mac[20];
                snprintf(slot_mac, sizeof(slot_mac), "3C:3DS:SL:00:00:0%u", (unsigned)(slot + 1));
                int slot_ch = (slot == 0) ? 1 : (slot == 1 ? 6 : 11);
                if (wg_record_observation(st, slot_mac, slot_ssid,
                                          WG_AUTH_WPA2_AES, slot_ch, -62 - (int)slot * 6,
                                          utc_timestamp, false)) {
                    added++;
                }
            }
        }
    }

    /* 3. Scan nearby 2.4 GHz 3DS UDS / NWM beacon frames */
    size_t tmp_size = 0x4000;
    u32 *tmp_buf = (u32 *)malloc(tmp_size);
    if (tmp_buf != NULL) {
        udsNetworkScanInfo *networks = NULL;
        size_t total_nets = 0;
        u8 wlancommID[1] = { 0 };
        Result rc = udsScanBeacons(tmp_buf, tmp_size, &networks, &total_nets,
                                   0x00040000u, wlancommID[0], NULL, false);
        if (R_SUCCEEDED(rc) && networks != NULL) {
            for (size_t i = 0; i < total_nets && i < 16; i++) {
                const u8 *mac = networks[i].datareply_entry.mac_address;
                char mac_str[20];
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                int ch = networks[i].datareply_entry.channel;
                if (wg_record_observation(st, mac_str, "NINTENDO_3DS_LOCAL_WLAN",
                                          WG_AUTH_3DS_UDS, ch, -58, utc_timestamp, true)) {
                    added++;
                }
            }
            free(networks);
        }
        free(tmp_buf);
    }
#else
    /* Host / simulation sweep: record deterministic realistic 2.4 GHz APs + 3DS UDS beacon */
    static const struct {
        const char *bssid;
        const char *ssid;
        wg_auth_mode_t auth;
        int channel;
        int rssi;
        bool uds;
    } k_sim_aps[] = {
        { "A4:2B:B0:11:22:33", "LAFIA_HOME_2.4G",     WG_AUTH_WPA2_AES, 1,  -46, false },
        { "70:4F:57:88:99:AA", "POKECENTER_FREE_WIFI", WG_AUTH_OPEN,    6,  -64, false },
        { "9C:E6:35:33:44:55", "NINTENDO_3DS_UDS_BEACON", WG_AUTH_3DS_UDS, 11, -52, true },
        { "18:D6:C7:AA:BB:CC", "SYLPH_CO_CORP_WPA",   WG_AUTH_WPA_TKIP, 11, -76, false }
    };
    for (size_t i = 0; i < sizeof(k_sim_aps) / sizeof(k_sim_aps[0]); i++) {
        st->cur_lat += 0.00012;
        st->cur_lon += 0.00015;
        if (wg_record_observation(st, k_sim_aps[i].bssid, k_sim_aps[i].ssid,
                                  k_sim_aps[i].auth, k_sim_aps[i].channel,
                                  k_sim_aps[i].rssi, utc_timestamp, k_sim_aps[i].uds)) {
            added++;
        }
    }
#endif
    return added;
}
