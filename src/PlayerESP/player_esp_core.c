#include "player_esp_core.h"
#include <float.h>
#include <math.h>
#include <string.h>

static int finite3(Esp335Vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
void esp335_reset(Esp335Core *core) {
    if (core) memset(core, 0, sizeof(*core));
}
int esp335_begin(Esp335Core *core, uint64_t epoch) {
    if (!core || !epoch) return 0;
    /* Every complete scan replaces the prior cache; epoch is identity for
     * zone/instance/relog transitions, never a raw object-manager pointer. */
    core->count = 0;
    core->world_epoch = epoch;
    core->frame_open = 1;
    return 1;
}
int esp335_push(Esp335Core *core, const Esp335Player *p) {
    size_t i;
    if (!core || !core->frame_open || !p || !p->guid ||
        !finite3(p->position) || p->max_health < p->health ||
        (p->kind && p->kind != ESP335_KIND_PLAYER && p->kind != ESP335_KIND_NPC) ||
        p->faction > ESP335_ALLIANCE || p->relation > ESP335_REL_HOSTILE ||
        p->bg_team > ESP335_BG_OPPONENT) return 0;
    for (i = 0; i < core->count; ++i) {
        if (core->players[i].guid == p->guid) {
            core->players[i] = *p; /* replace duplicate GUID, never double draw */
            return 1;
        }
    }
    if (core->count >= ESP335_MAX_PLAYERS) return 0;
    core->players[core->count++] = *p;
    return 1;
}
void esp335_end(Esp335Core *core) {
    if (core) core->frame_open = 0;
}
int esp335_eligible(const Esp335Player *p, const Esp335Filter *filter,
                    int in_bg) {
    unsigned faction_mask;
    if (!p || !filter || !p->guid) return 0;
    if (p->kind == ESP335_KIND_NPC) {
        return filter->show_npc ||
            (filter->show_npc_hostile && p->relation == ESP335_REL_HOSTILE);
    }
    if (!filter->show_players && !filter->show_all &&
        !filter->factions && !filter->show_hostile &&
        !filter->show_bg_opponents) return 0;
    faction_mask = (p->faction == ESP335_HORDE) ? ESP335_HORDE :
                   (p->faction == ESP335_ALLIANCE) ? ESP335_ALLIANCE : 0u;
    return filter->show_all ||
           (in_bg && filter->show_bg_opponents &&
            p->bg_team == ESP335_BG_OPPONENT) ||
           (filter->show_hostile && p->relation == ESP335_REL_HOSTILE) ||
           ((filter->factions & faction_mask) != 0u) ||
           (filter->show_unknown && !faction_mask &&
            p->relation == 0u && p->bg_team == 0u);
}
int esp335_project(const Esp335Camera *c, Esp335Vec3 p,
                   float *sx, float *sy, float *depth) {
    const float *m;
    float x, y, z, w, nx, ny;
    unsigned i;
    if (!c || !sx || !sy || !depth || !finite3(p) ||
        !isfinite(c->viewport_x) || !isfinite(c->viewport_y) ||
        !isfinite(c->viewport_w) || !isfinite(c->viewport_h) ||
        c->viewport_w <= 0.f || c->viewport_h <= 0.f) return 0;
    m = c->view_projection;
    for (i = 0; i < 16; ++i) if (!isfinite(m[i])) return 0;
    x = m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3];
    y = m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7];
    z = m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11];
    w = m[12]*p.x + m[13]*p.y + m[14]*p.z + m[15];
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(w) ||
        w <= 1e-4f || z < 0.f || z > w) return 0;
    nx = x / w; ny = y / w;
    if (nx < -1.f || nx > 1.f || ny < -1.f || ny > 1.f) return 0;
    *sx = c->viewport_x + (nx + 1.f)*0.5f*c->viewport_w;
    *sy = c->viewport_y + (1.f - ny)*0.5f*c->viewport_h;
    *depth = z / w;
    return isfinite(*sx) && isfinite(*sy) && isfinite(*depth);
}
size_t esp335_labels(const Esp335Core *core, const Esp335Camera *camera,
                     const Esp335Filter *filter, int in_bg,
                     Esp335Label *out, size_t cap) {
    size_t i, n = 0;
    if (!core || !camera || !filter || !out || !cap ||
        !core->world_epoch || core->frame_open ||
        !finite3(camera->local_position)) return 0;
    for (i = 0; i < core->count && n < cap; ++i) {
        const Esp335Player *p = &core->players[i];
        float dx, dy, dz, distance, sx, sy, depth;
        if (!esp335_eligible(p, filter, in_bg)) continue;
        dx = p->position.x - camera->local_position.x;
        dy = p->position.y - camera->local_position.y;
        dz = p->position.z - camera->local_position.z;
        distance = sqrtf(dx*dx + dy*dy + dz*dz);
        if (!isfinite(distance) ||
            (filter->max_distance > 0.f && distance > filter->max_distance) ||
            !esp335_project(camera, p->position, &sx, &sy, &depth)) continue;
        out[n].guid = p->guid;
        out[n].world_epoch = core->world_epoch;
        out[n].screen_x = sx;
        out[n].screen_y = sy;
        out[n].depth = depth;
        out[n].distance = distance;
        out[n].health = p->health;
        out[n].max_health = p->max_health;
        out[n].level = p->level;
        out[n].class_id = p->class_id;
        out[n].kind = p->kind ? p->kind : ESP335_KIND_PLAYER;
        ++n;
    }
    return n;
}
int esp335_pick_guid(const Esp335Core *core, const Esp335Label *labels,
                     size_t count, float mx, float my, float w, float h,
                     uint64_t *guid) {
    size_t i, j;
    float best_depth = FLT_MAX;
    uint64_t best = 0;
    if (!core || core->frame_open || !core->world_epoch || !labels || !guid ||
        !isfinite(mx) || !isfinite(my) || !isfinite(w) || !isfinite(h) ||
        w <= 0.f || h <= 0.f) return 0;
    for (i = 0; i < count; ++i) {
        const Esp335Label *label = &labels[i];
        int present = 0;
        if (!label->guid || label->world_epoch != core->world_epoch ||
            !isfinite(label->depth) || label->depth < 0.f ||
            label->depth > 1.f || !isfinite(label->screen_x) ||
            !isfinite(label->screen_y)) continue;
        for (j = 0; j < core->count; ++j)
            if (core->players[j].guid == label->guid) { present = 1; break; }
        if (present && mx >= label->screen_x-w*0.5f &&
            mx <= label->screen_x+w*0.5f &&
            my >= label->screen_y-h && my <= label->screen_y &&
            label->depth < best_depth) {
            best = label->guid;
            best_depth = label->depth;
        }
    }
    if (!best) return 0;
    *guid = best; /* Host MUST resolve/validate GUID again before targeting. */
    return 1;
}
