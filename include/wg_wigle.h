#ifndef WG_WIGLE_H
#define WG_WIGLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WG_MAX_APS 64
#define WG_PATH_MAX 512

typedef enum {
    WG_AUTH_OPEN = 0,
    WG_AUTH_WEP_40 = 1,
    WG_AUTH_WEP_104 = 2,
    WG_AUTH_WEP_128 = 3,
    WG_AUTH_WPA_TKIP = 4,
    WG_AUTH_WPA2_TKIP = 5,
    WG_AUTH_WPA_AES = 6,
    WG_AUTH_WPA2_AES = 7,
    WG_AUTH_3DS_UDS = 8
} wg_auth_mode_t;

typedef struct {
    char bssid[20];          /* "AA:BB:CC:DD:EE:FF" */
    char ssid[36];           /* Network SSID or 3DS UDS App Title */
    wg_auth_mode_t auth;
    int channel;             /* 1..13 */
    int rssi_dbm;            /* -90..-30 dBm */
    double lat;
    double lon;
    double alt_m;
    double accuracy_m;
    char first_seen_utc[24]; /* "YYYY-MM-DD HH:MM:SS" */
    uint32_t seen_count;
    bool is_3ds_uds;
} wg_ap_entry_t;

typedef struct {
    char username[36];
    char api_name[36];       /* "AID..." */
    uint32_t rank;
    uint32_t month_rank;
    uint64_t discovered_wifi;
    uint32_t total_uploads;
    char last_sync_utc[24];
    bool configured;
} wg_account_t;

typedef struct {
    char sd_root[WG_PATH_MAX];
    char session_csv_path[WG_PATH_MAX + 64];
    char account_json_path[WG_PATH_MAX + 64];
    wg_account_t account;
    double cur_lat;
    double cur_lon;
    double cur_alt_m;
    double cur_accuracy_m;
    bool gps_fix_active;
    uint32_t total_scans;
    uint32_t csv_rows_written;
    int ap_count;
    wg_ap_entry_t aps[WG_MAX_APS];
} wg_wardrive_state_t;

/* Load or save <sd_root>/3ds/wigle/wigle_account.json (synced by pc_companion/wigle_sync.py) */
bool wg_load_account(wg_wardrive_state_t *st);
bool wg_save_account(const wg_wardrive_state_t *st);

/* Map 3DS security mode to exact WiGLE v1.4 AuthMode capability string (e.g. "[WPA2-PSK-CCMP][ESS]") */
const char *wg_auth_to_wigle_caps(wg_auth_mode_t auth);

/* Short UI badge for security mode (e.g. "WPA2", "OPEN", "WEP", "UDS") */
const char *wg_auth_short_label(wg_auth_mode_t auth);

/* Format current UTC or system clock into "YYYY-MM-DD HH:MM:SS" */
void wg_format_current_utc(char out_ts[24]);

/* Map 2.4 GHz channel (1..14) to center frequency in MHz (2412..2484) */
int wg_channel_to_freq_mhz(int channel);

/* Clear all in-memory APs and start a fresh WiGLE-Wifi-1.4 CSV session file */
bool wg_reset_session(wg_wardrive_state_t *st);

/* Initialize wardriving state and create WiGLE-Wifi-1.4 pre-header + column header CSV if absent */
bool wg_init(wg_wardrive_state_t *st, const char *sd_root, const char *session_name);

/* Record or update a discovered Wi-Fi AP / 3DS UDS beacon and append to the WiGLE v1.4 CSV log */
bool wg_record_observation(wg_wardrive_state_t *st,
                           const char *bssid,
                           const char *ssid,
                           wg_auth_mode_t auth,
                           int channel,
                           int rssi_dbm,
                           const char *utc_timestamp,
                           bool is_3ds_uds);

/* Perform one scan sweep on 3DS (queries ACU connected AP, ACI saved slots, and NWM/UDS 2.4GHz beacons)
   or synthetic sweep on host */
int wg_perform_sweep(wg_wardrive_state_t *st, const char *utc_timestamp);

#endif /* WG_WIGLE_H */
