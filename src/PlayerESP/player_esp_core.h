/* Original implementation for project 335; no ConsoleXP code or offsets.
 * Portable logic only: never reads client memory or installs game hooks.
 */
#ifndef PLAYER_ESP_335_CORE_H
#define PLAYER_ESP_335_CORE_H
#include <stddef.h>
#include <stdint.h>

#define ESP335_MAX_PLAYERS 256u
#define ESP335_HORDE 1u
#define ESP335_ALLIANCE 2u
#define ESP335_BG_OPPONENT 2u
#define ESP335_REL_HOSTILE 2u

typedef struct { float x, y, z; } Esp335Vec3;
typedef struct {
    uint64_t guid;
    Esp335Vec3 position;
    unsigned faction;       /* 0 unknown, 1 Horde, 2 Alliance */
    unsigned relation;      /* 0 unknown, 1 friendly, 2 hostile */
    unsigned bg_team;       /* 0 unknown, 1 teammate, 2 opponent */
    unsigned health, max_health;
    unsigned level, class_id;
} Esp335Player;

typedef struct {
    unsigned factions;      /* bitmask ESP335_HORDE | ESP335_ALLIANCE */
    unsigned show_hostile;
    unsigned show_bg_opponents;
    float max_distance;     /* yards; 0 means no distance restriction */
    unsigned show_all; /* when metadata are UNKNOWN, still display players */
} Esp335Filter;

typedef struct {
    /* Row-major Direct3D view * projection; clip z must lie in [0,w].
     * Host must supply a coherent, verified game-frame snapshot. */
    float view_projection[16];
    float viewport_x, viewport_y, viewport_w, viewport_h;
    Esp335Vec3 local_position;
} Esp335Camera;

typedef struct {
    uint64_t guid, world_epoch;
    float screen_x, screen_y, depth, distance;
    unsigned health, max_health, level, class_id;
} Esp335Label;

typedef struct {
    uint64_t world_epoch;
    Esp335Player players[ESP335_MAX_PLAYERS];
    size_t count;
    unsigned frame_open;
} Esp335Core;

void esp335_reset(Esp335Core *core);
int esp335_begin(Esp335Core *core, uint64_t world_epoch);
int esp335_push(Esp335Core *core, const Esp335Player *player);
void esp335_end(Esp335Core *core);
int esp335_eligible(const Esp335Player *player, const Esp335Filter *filter,
                    int in_bg);
int esp335_project(const Esp335Camera *camera, Esp335Vec3 point,
                   float *x, float *y, float *depth);
size_t esp335_labels(const Esp335Core *core, const Esp335Camera *camera,
                     const Esp335Filter *filter, int in_bg,
                     Esp335Label *out, size_t capacity);
int esp335_pick_guid(const Esp335Core *core, const Esp335Label *labels,
                     size_t count, float mouse_x, float mouse_y,
                     float label_width, float label_height, uint64_t *guid);

#endif
