#include "player_esp_lua.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#define ESP335_SCRIPT_CAP 12288u
#define ESP335_LUA_POINTS 24u
#define ESP335_LUA_CANDIDATES ESP335_MAX_PLAYERS
#define ESP335_LUA_MAX_DISTANCE 120.f
/* Approximate 3.3.5 label anchor; do not treat as native model-head data. */
#define ESP335_NPC_LABEL_Z 1.60f
#define ESP335_PLAYER_LABEL_Z 2.30f

static const char g_ui_init[] =
    "if not UIParent then return end\n"
    "if ESP335HUD then return end\n"
    "local e={}\n"
    "e.opt={true,true,false,false,false,false,true,false,true}\n"
    "e.open=true\n"
    "e.range=40\n"
    "local frame=CreateFrame(\"Frame\",nil,UIParent)\n"
    "e.frame=frame\n"
    "frame:SetWidth(245)\n"
    "frame:SetHeight(314)\n"
    "frame:SetPoint(\"TOPLEFT\",UIParent,\"TOPLEFT\",24,-56)\n"
    "frame:SetFrameStrata(\"TOOLTIP\")\n"
    "frame:EnableMouse(true)\n"
    "local bg=frame:CreateTexture(nil,\"BACKGROUND\")\n"
    "bg:SetAllPoints(frame)\n"
    "bg:SetTexture(0.025,0.055,0.09,0.93)\n"
    "local head=frame:CreateFontString(nil,\"OVERLAY\",\"GameFontNormal\")\n"
    "head:SetPoint(\"TOPLEFT\",frame,\"TOPLEFT\",12,-10)\n"
    "head:SetText(\"ESP 335  |  INSERT\")\n"
    "head:SetTextColor(0.35,0.90,0.97)\n"
    "local names={\"ESP ON\",\"PLAYERS ALL\",\"HORDE\",\"ALLIANCE\",\"HOSTILE\",\"BG ENEMY\",\"NPC ALL\",\"NPC HOSTILE\",\"UNKNOWN\"}\n"
    "e.buttons={}\n"
    "local function sync(i)\n"
    " local b=e.buttons[i]\n"
    " if not b then return end\n"
    " b.text:SetText((e.opt[i] and \"[X] \" or \"[ ] \")..names[i])\n"
    " b.text:SetTextColor(e.opt[i] and 0.35 or 0.66,e.opt[i] and 0.95 or 0.66,e.opt[i] and 0.56 or 0.66)\n"
    "end\n"
    "for i=1,#names do\n"
    " local b=CreateFrame(\"Button\",nil,frame)\n"
    " b:SetWidth(220)\n"
    " b:SetHeight(22)\n"
    " b:SetPoint(\"TOPLEFT\",frame,\"TOPLEFT\",12,-33-(i-1)*23)\n"
    " b.text=b:CreateFontString(nil,\"OVERLAY\",\"GameFontNormalSmall\")\n"
    " b.text:SetPoint(\"LEFT\",b,\"LEFT\",3,0)\n"
    " b.text:SetJustifyH(\"LEFT\")\n"
    " e.buttons[i]=b\n"
    " b:SetScript(\"OnClick\",function(self)\n"
    "  e.opt[i]=not e.opt[i]\n"
    "  if i>=3 and i<=6 and e.opt[i] then e.opt[2]=false end\n"
    "  if i==2 and e.opt[i] then\n"
    "   for j=3,6 do e.opt[j]=false end\n"
    "  end\n"
    "  for j=1,#names do sync(j) end\n"
    " end)\n"
    " sync(i)\n"
    "end\n"
    "local rangeBtn=CreateFrame(\"Button\",nil,frame)\n"
    "rangeBtn:SetWidth(220)\n"
    "rangeBtn:SetHeight(22)\n"
    "rangeBtn:SetPoint(\"TOPLEFT\",frame,\"TOPLEFT\",12,-246)\n"
    "local rangeText=rangeBtn:CreateFontString(nil,\"OVERLAY\",\"GameFontNormalSmall\")\n"
    "rangeText:SetPoint(\"LEFT\",rangeBtn,\"LEFT\",3,0)\n"
    "rangeText:SetJustifyH(\"LEFT\")\n"
    "rangeText:SetText(\"RANGE 40y  [click to change]\")\n"
    "rangeBtn:SetScript(\"OnClick\",function()\n"
    " if e.range==25 then e.range=40 elseif e.range==40 then e.range=80\n"
    " elseif e.range==80 then e.range=120 else e.range=25 end\n"
    " rangeText:SetText(\"RANGE \"..e.range..\"y  [click to change]\")\n"
    "end)\n"
    "local status=frame:CreateFontString(nil,\"OVERLAY\",\"GameFontNormalSmall\")\n"
    "status:SetPoint(\"BOTTOMLEFT\",frame,\"BOTTOMLEFT\",12,10)\n"
    "status:SetText(\"Scanner awaiting game data\")\n"
    "status:SetTextColor(0.94,0.84,0.46)\n"
    "e.status=status\n"
    "e.markers={}\n"
    "for i=1,24 do\n"
    " local m=CreateFrame(\"Frame\",nil,WorldFrame)\n"
    " m:SetWidth(125)\n"
    " m:SetHeight(18)\n"
    " m:SetFrameStrata(\"HIGH\")\n"
    " m.text=m:CreateFontString(nil,\"OVERLAY\",\"GameFontNormalSmall\")\n"
    " m.text:SetPoint(\"CENTER\",m,\"CENTER\",0,0)\n"
    " m:Hide()\n"
    " e.markers[i]=m\n"
    "end\n"
    "function e:SetVisible(visible)\n"
    " self.open=(visible==1)\n"
    " if self.open then self.frame:Show() else self.frame:Hide() end\n"
    "end\n"
    "function e:Paint(rows,scan,p,n,c)\n"
    " self.status:SetText(\"SCAN \"..scan..\" P \"..p..\" N \"..n..\" C \"..c..\" | \"..#rows)\n"
    " for i=1,#self.markers do self.markers[i]:Hide() end\n"
    " if not self.opt[1] then return end\n"
    " local count=0\n"
    " local occupied={}\n"
    " local world=WorldFrame\n"
    " if not world or not world:IsShown() then return end\n"
    " local width,height=world:GetWidth(),world:GetHeight()\n"
    " if not width or not height or width<64 or height<64 then return end\n"
    " local worldLeft,worldBottom=world:GetLeft() or 0,world:GetBottom() or 0\n"
    " for _,r in ipairs(rows) do\n"
    "  local kind,faction,relation,bg=r[3],r[4],r[5],r[6]\n"
    "  local ok\n"
    "  if kind==3 then ok=self.opt[7] or (self.opt[8] and relation==2)\n"
    "  else\n"
    "   ok=self.opt[2] or (self.opt[3] and faction==1) or (self.opt[4] and faction==2)\n"
    "      or (self.opt[5] and relation==2) or (self.opt[6] and bg==2)\n"
    "      or (self.opt[9] and faction==0 and relation==0 and bg==0)\n"
    "  end\n"
    "  if ok and r[7]<=self.range and count<#self.markers then\n"
    "   local x,y=r[1]*width,(1-r[2])*height\n"
    "   local overlap=self.open and x+worldLeft<282 and y+worldBottom>UIParent:GetHeight()-352\n"
    "   if not overlap then\n"
    "    for _,p in ipairs(occupied) do\n"
    "     if math.abs(p[1]-x)<98 and math.abs(p[2]-y)<24 then overlap=true; break end\n"
    "    end\n"
    "   end\n"
    "   if not overlap then\n"
    "    count=count+1\n"
    "    occupied[count]={x,y}\n"
    "    local m=self.markers[count]\n"
    "    m:ClearAllPoints()\n"
    "    m:SetPoint(\"CENTER\",world,\"BOTTOMLEFT\",x,y)\n"
    "    m.text:SetText((kind==3 and \"NPC \" or \"PLAYER \")..r[7]..\"y\")\n"
    "    if kind==3 then m.text:SetTextColor(0.35,0.85,1)\n"
    "    elseif faction==1 then m.text:SetTextColor(1,0.32,0.3)\n"
    "    elseif faction==2 then m.text:SetTextColor(0.4,0.6,1)\n"
    "    else m.text:SetTextColor(1,0.92,0.4) end\n"
    "    m:Show()\n"
    "   end\n"
    "  end\n"
    " end\n"
    "end\n"
    "ESP335HUD=e\n"
    "\n"
;
int esp335_lua_create(Esp335LuaGate run) {
    return run && run(g_ui_init,"ESP335:gui");
}
int esp335_lua_visibility(Esp335LuaGate run,unsigned visible) {
    if (!run) return 0;
    return run(visible ?
        "if ESP335HUD then ESP335HUD:SetVisible(1) end" :
        "if ESP335HUD then ESP335HUD:SetVisible(0) end","ESP335:visible");
}

typedef struct {
    const Esp335Player *player;
    float x, y, distance;
} Esp335LuaCandidate;

static int compare_nearest(const void *left, const void *right) {
    const Esp335LuaCandidate *a=(const Esp335LuaCandidate *)left;
    const Esp335LuaCandidate *b=(const Esp335LuaCandidate *)right;
    if (a->distance<b->distance) return -1;
    if (a->distance>b->distance) return 1;
    if (a->player->guid<b->player->guid) return -1;
    if (a->player->guid>b->player->guid) return 1;
    return 0;
}
int esp335_lua_update(Esp335LuaGate run,const Esp335Core *core,
                       const Esp335Camera *cam,unsigned scan_ok,
                       unsigned players,unsigned npcs,unsigned camera_ok,
                       Esp335NativeProject project,void *project_context) {
    char script[ESP335_SCRIPT_CAP];
    Esp335LuaCandidate candidates[ESP335_LUA_CANDIDATES];
    size_t n=0u,i,found=0u,shown=0u;
    int written;
    if (!run || !core || !cam) return 0;
    written=_snprintf_s(script,sizeof(script),_TRUNCATE,
        "if ESP335HUD then ESP335HUD:Paint({");
    if (written<0) return 0;
    n=(size_t)written;
    if (scan_ok && camera_ok && project && core->world_epoch &&
        !core->frame_open) {
        /* Object-manager iteration order is NOT screen proximity: collect,
         * sort nearest first and let the Lua GUI apply its own range/filters.
         * In particular do not let distant NPCs exhaust the label budget. */
        for (i=0u;i<core->count && i<ESP335_MAX_PLAYERS;++i) {
            const Esp335Player *p=&core->players[i];
            Esp335Vec3 anchor;
            float dx,dy,dz,distance,px,py;
            if (!p->guid || !p->max_health) continue;
            dx=p->position.x-cam->local_position.x;
            dy=p->position.y-cam->local_position.y;
            dz=p->position.z-cam->local_position.z;
            distance=sqrtf(dx*dx+dy*dy+dz*dz);
            if (!isfinite(distance) || distance>ESP335_LUA_MAX_DISTANCE)
                continue;
            anchor=p->position;
            anchor.z+=(p->kind==ESP335_KIND_NPC)
                ? ESP335_NPC_LABEL_Z : ESP335_PLAYER_LABEL_Z;
            /* Never rebuild a camera projection: call pinned client's W2S. */
            if (!project(project_context,anchor,&px,&py)) continue;
            if (!isfinite(px) || !isfinite(py) ||
                px<0.f || px>1.f || py<0.f || py>1.f) continue;
            candidates[found].player=p;
            candidates[found].x=px;
            candidates[found].y=py;
            candidates[found].distance=distance;
            ++found;
        }
        qsort(candidates,found,sizeof(candidates[0]),compare_nearest);
        for (i=0u;i<found && shown<ESP335_LUA_POINTS;++i) {
            const Esp335Player *p=candidates[i].player;
            written=_snprintf_s(script+n,sizeof(script)-n,_TRUNCATE,
                "%s{%.5f,%.5f,%u,%u,%u,%u,%u}",
                shown?",":"",candidates[i].x,candidates[i].y,
                p->kind,p->faction,p->relation,p->bg_team,
                (unsigned)(candidates[i].distance+0.5f));
            if (written<0) return 0;
            n+=(size_t)written;
            ++shown;
        }
    }
    written=_snprintf_s(script+n,sizeof(script)-n,_TRUNCATE,
        "},%u,%u,%u,%u) end",scan_ok?1u:0u,players,npcs,camera_ok?1u:0u);
    if (written<0) return 0;
    return run(script,"ESP335:markers");
}
