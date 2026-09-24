/* Universal WoW335GUI: one Insert owner and native modeless in-game window.
 * Deliberately does NOT hook D3D9: PlayerESP remains the only Present owner.
 * Popup UI is supported in windowed/borderless only, not exclusive fullscreen.
 * All callbacks and Win32 HWND access stay on the verified game-window thread.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "w335_gui_api.h"
#define GUI_MSG "WoW335_SharedGUI_12340_v1"
#define GUI_CLASS "WoW335_SharedGUI_Window_v1"
#define MAX_MODS 32u
#define MAX_DLLS 512u
#define FIRST_FIELD 1000
#define FIRST_RANGE 2000
#define MAX_PAGE 11u
typedef struct {
    char key[48],label[88];
    unsigned kind;
    int min,max,value;
} Field;
typedef struct {
    char id[48],filename[96],title[96];
    unsigned count;
    Field fields[W335GUI_MAX_FIELDS];
    W335GUI_OnChange callback;
} Module;
static HINSTANCE g_instance;
static DWORD g_tid,g_last_refresh;
static HWND g_game,g_window,g_list,g_page_label,g_prev,g_next;
static HWND g_controls[W335GUI_MAX_FIELDS][4];
static Module g_modules[MAX_MODS];
static unsigned g_mod_count,g_selected,g_page,g_enabled;
static UINT g_msg;
static wchar_t g_ini[MAX_PATH];
static int on_thread(void) {return g_tid && GetCurrentThreadId()==g_tid;}
static void copy(char *out,size_t size,const char *input) {
    if (out && size) strncpy_s(out,size,input?input:"",_TRUNCATE);
}
static int valid_key(const char *key) {
    size_t i,n;
    if (!key || !(n=strlen(key)) || n>47u) return 0;
    for(i=0;i<n;++i) {
        char c=key[i];
        if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
              (c>='0'&&c<='9')||c=='_'||c=='-')) return 0;
    }
    return 1;
}
static void persist(const Module *m,const Field *f) {
    wchar_t section[48],key[48],value[40];
    if (!g_ini[0]) return;
    if (!MultiByteToWideChar(CP_ACP,0,m->id,-1,section,48) ||
        !MultiByteToWideChar(CP_ACP,0,f->key,-1,key,48)) return;
    swprintf_s(value,40,L"%d",f->value);
    WritePrivateProfileStringW(section,key,value,g_ini);
}
static int load_value(const Module *m,const Field *f) {
    wchar_t section[48],key[48];
    int v=f->value;
    if (g_ini[0] &&
        MultiByteToWideChar(CP_ACP,0,m->id,-1,section,48) &&
        MultiByteToWideChar(CP_ACP,0,f->key,-1,key,48))
        v=(int)GetPrivateProfileIntW(section,key,f->value,g_ini);
    if (v<f->min) v=f->min;
    if (v>f->max) v=f->max;
    return v;
}
static int find_id(const char *id) {
    unsigned i;
    for(i=0;i<g_mod_count;++i) if (!_stricmp(g_modules[i].id,id)) return (int)i;
    return -1;
}
static int find_file(const char *filename) {
    unsigned i;
    for(i=0;i<g_mod_count;++i)
        if (!_stricmp(g_modules[i].filename,filename)) return (int)i;
    return -1;
}
static void clear_fields(void) {
    unsigned i,k;
    for(i=0;i<W335GUI_MAX_FIELDS;++i)
        for(k=0;k<4u;++k) {
            if(g_controls[i][k]) DestroyWindow(g_controls[i][k]);
            g_controls[i][k]=NULL;
        }
}
static void populate_fields(void) {
    unsigned i,first,last;
    char value[128],page[72];
    Module *m;
    clear_fields();
    if (!g_window || !g_mod_count || g_selected>=g_mod_count) return;
    m=&g_modules[g_selected];
    first=g_page*MAX_PAGE;
    if(first>=m->count){g_page=0;first=0;}
    last=first+MAX_PAGE;
    if(last>m->count)last=m->count;
    for(i=first;i<last;++i) {
        const Field *f=&m->fields[i];
        int y=80+(int)(i-first)*33;
        HWND label=CreateWindowExA(0,"STATIC",f->label,WS_CHILD|WS_VISIBLE,
                270,y,190,26,g_window,NULL,g_instance,NULL);
        g_controls[i][0]=label;
        if(f->kind==W335GUI_TOGGLE) {
            HWND check=CreateWindowExA(0,"BUTTON","",
                WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
                485,y,32,23,g_window,(HMENU)(INT_PTR)(FIRST_FIELD+i),g_instance,NULL);
            SendMessageA(check,BM_SETCHECK,f->value?BST_CHECKED:BST_UNCHECKED,0);
            g_controls[i][1]=check;
        } else {
            _snprintf_s(value,sizeof(value),_TRUNCATE,"%d",f->value);
            g_controls[i][1]=CreateWindowExA(0,"BUTTON","-",
                WS_CHILD|WS_VISIBLE,465,y-1,35,25,g_window,
                (HMENU)(INT_PTR)(FIRST_RANGE+(int)i*2),g_instance,NULL);
            g_controls[i][2]=CreateWindowExA(0,"BUTTON","+",
                WS_CHILD|WS_VISIBLE,565,y-1,35,25,g_window,
                (HMENU)(INT_PTR)(FIRST_RANGE+(int)i*2+1),g_instance,NULL);
            g_controls[i][3]=CreateWindowExA(0,"STATIC",value,WS_CHILD|WS_VISIBLE,
                511,y,45,24,g_window,(HMENU)(INT_PTR)(3000+i),g_instance,NULL);
        }
    }
    _snprintf_s(page,sizeof(page),_TRUNCATE,"%s | page %u/%u",
        m->title,g_page+1u,(m->count+MAX_PAGE-1u)/MAX_PAGE?
        (m->count+MAX_PAGE-1u)/MAX_PAGE:1u);
    SetWindowTextA(g_page_label,page);
    EnableWindow(g_prev,g_page>0u);
    EnableWindow(g_next,(g_page+1u)*MAX_PAGE<m->count);
}
static void refresh_list(void) {
    HANDLE snapshot;
    MODULEENTRY32 e;
    char selected[96]={0};
    unsigned found=0;
    if (!g_list || !on_thread())return;
    if(g_mod_count && g_selected<g_mod_count)
        copy(selected,sizeof(selected),g_modules[g_selected].filename);
    SendMessageA(g_list,LB_RESETCONTENT,0,0);
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,
                                      GetCurrentProcessId());
    if(snapshot==INVALID_HANDLE_VALUE)return;
    memset(&e,0,sizeof(e));e.dwSize=sizeof(e);
    if(Module32First(snapshot,&e)) do {
        char title[160];
        int index,owner;
        size_t n=strlen(e.szModule);
        if(n<4u || _stricmp(e.szModule+n-4u,".dll"))continue;
        owner=find_file(e.szModule);
        _snprintf_s(title,sizeof(title),_TRUNCATE,"[%s] %s",
           owner>=0?"READY":"LOADED",e.szModule);
        index=(int)SendMessageA(g_list,LB_ADDSTRING,0,(LPARAM)title);
        if(index>=0 && index!=LB_ERRSPACE) {
            SendMessageA(g_list,LB_SETITEMDATA,(WPARAM)index,(LPARAM)owner);
            if(owner>=0 && selected[0] && !_stricmp(selected,e.szModule))
                found=(unsigned)index+1u;
        }
    }while(Module32Next(snapshot,&e));
    CloseHandle(snapshot);
    if(!found && g_mod_count) {
        int index;
        char match[160];
        _snprintf_s(match,sizeof(match),_TRUNCATE,"[READY] %s",
                     g_modules[g_selected].filename);
        index=(int)SendMessageA(g_list,LB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)match);
        if(index>=0) found=(unsigned)index+1u;
    }
    if(found) SendMessageA(g_list,LB_SETCURSEL,(WPARAM)(found-1u),0);
}
static void set_field(unsigned slot,int new_value) {
    Module *m;
    Field *f;
    if(!on_thread() || !g_mod_count || g_selected>=g_mod_count)return;
    m=&g_modules[g_selected];
    if(slot>=m->count)return;
    f=&m->fields[slot];
    if(new_value<f->min)new_value=f->min;
    if(new_value>f->max)new_value=f->max;
    if(new_value==f->value)return;
    f->value=new_value;
    persist(m,f);
    if(m->callback)m->callback(f->key,f->value);
    populate_fields();
}
static LRESULT CALLBACK gui_wndproc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_CREATE:
        g_list=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",
            WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY,
            12,53,242,417,window,(HMENU)10,g_instance,NULL);
        g_page_label=CreateWindowExA(0,"STATIC","Select an active module",
            WS_CHILD|WS_VISIBLE,270,53,422,22,window,NULL,g_instance,NULL);
        g_prev=CreateWindowExA(0,"BUTTON","<",
            WS_CHILD|WS_VISIBLE,270,452,42,27,window,(HMENU)11,g_instance,NULL);
        g_next=CreateWindowExA(0,"BUTTON",">",
            WS_CHILD|WS_VISIBLE,321,452,42,27,window,(HMENU)12,g_instance,NULL);
        return 0;
    case WM_COMMAND: {
        unsigned id=LOWORD(w);
        if(id==10u && HIWORD(w)==LBN_SELCHANGE) {
            int sel=(int)SendMessageA(g_list,LB_GETCURSEL,0,0);
            if(sel>=0) {
                int owner=(int)SendMessageA(g_list,LB_GETITEMDATA,(WPARAM)sel,0);
                if(owner>=0 && (unsigned)owner<g_mod_count) {
                    g_selected=(unsigned)owner;g_page=0;populate_fields();
                }else SetWindowTextA(g_page_label,"Loaded DLL (no registered controls)");
            }
        }else if(id==11u && g_page) {--g_page;populate_fields();}
        else if(id==12u) {++g_page;populate_fields();}
        else if(id>=FIRST_FIELD && id<FIRST_FIELD+W335GUI_MAX_FIELDS &&
                HIWORD(w)==BN_CLICKED) {
            unsigned field=id-FIRST_FIELD;
            HWND control=g_controls[field][1];
            if(control) set_field(field,
                SendMessageA(control,BM_GETCHECK,0,0)==BST_CHECKED?1:0);
        }else if(id>=FIRST_RANGE && id<FIRST_RANGE+2*W335GUI_MAX_FIELDS &&
                 HIWORD(w)==BN_CLICKED) {
            unsigned field=(id-FIRST_RANGE)/2u;
            int delta=(id-FIRST_RANGE)&1u?5:-5;
            if(g_selected<g_mod_count && field<g_modules[g_selected].count)
                set_field(field,g_modules[g_selected].fields[field].value+delta);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc=BeginPaint(window,&ps);
        RECT rc={0,0,718,500};
        HBRUSH bg=CreateSolidBrush(RGB(17,24,37));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(228,235,244));
        FillRect(dc,&rc,bg);DeleteObject(bg);
        TextOutA(dc,12,13,"WoW 335 | CONTROL CENTER",
            lstrlenA("WoW 335 | CONTROL CENTER"));
        EndPaint(window,&ps);return 0;
    }
    case WM_CLOSE: ShowWindow(window,SW_HIDE);SetForegroundWindow(g_game);return 0;
    case WM_DESTROY:
        clear_fields();g_window=NULL;g_list=NULL;g_page_label=NULL;
        g_prev=NULL;g_next=NULL;return 0;
    }
    return DefWindowProcA(window,message,w,l);
}
static void toggle_window(void) {
    WNDCLASSEXA wc;
    if(!on_thread() || !IsWindow(g_game))return;
    if(!g_window) {
        memset(&wc,0,sizeof(wc));wc.cbSize=sizeof(wc);
        wc.lpfnWndProc=gui_wndproc;wc.hInstance=g_instance;
        wc.lpszClassName=GUI_CLASS;wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        RegisterClassExA(&wc);
        g_window=CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
            GUI_CLASS,"WoW335 Control Center",WS_POPUP|WS_BORDER|WS_CLIPCHILDREN,
            120,110,718,500,g_game,NULL,g_instance,NULL);
        if(!g_window)return;
        populate_fields();refresh_list();
    }
    if(IsWindowVisible(g_window)) {
        ShowWindow(g_window,SW_HIDE);
        SetForegroundWindow(g_game);
    }else{
        RECT r;
        GetWindowRect(g_game,&r);
        SetWindowPos(g_window,HWND_TOPMOST,r.left+30,r.top+45,0,0,
            SWP_NOSIZE|SWP_SHOWWINDOW);
        SetForegroundWindow(g_window);
        refresh_list();populate_fields();
    }
}
__declspec(dllexport) int WINAPI W335GUI_Register(const W335GUI_Module *desc) {
    Module *m;
    unsigned i,j;
    int slot;
    if(!on_thread() || !g_enabled || !desc ||
       desc->size!=sizeof(*desc) || desc->abi_version!=W335GUI_ABI_VERSION ||
       !valid_key(desc->id) || !desc->filename ||
       !strstr(desc->filename,".dll") || !desc->title ||
       desc->field_count>W335GUI_MAX_FIELDS ||
       (desc->field_count && (!desc->fields || !desc->on_change)))return 0;
    slot=find_id(desc->id);
    if(slot<0 && g_mod_count>=MAX_MODS)return 0;
    for(i=0;i<desc->field_count;++i) {
        const W335GUI_Field *f=&desc->fields[i];
        if(!valid_key(f->key) || !f->label ||
           (f->kind!=W335GUI_TOGGLE && f->kind!=W335GUI_RANGE) ||
           f->maximum<f->minimum ||
           (f->kind==W335GUI_TOGGLE && (f->minimum!=0 || f->maximum!=1)))
            return 0;
        for(j=0;j<i;++j)
            if(!_stricmp(f->key,desc->fields[j].key))return 0;
    }
    if(slot<0)slot=(int)g_mod_count++;
    m=&g_modules[slot];
    memset(m,0,sizeof(*m));
    copy(m->id,sizeof(m->id),desc->id);
    copy(m->filename,sizeof(m->filename),desc->filename);
    copy(m->title,sizeof(m->title),desc->title);
    m->count=desc->field_count;m->callback=desc->on_change;
    for(i=0;i<m->count;++i) {
        const W335GUI_Field *in=&desc->fields[i];
        Field *out=&m->fields[i];
        copy(out->key,sizeof(out->key),in->key);
        copy(out->label,sizeof(out->label),in->label);
        out->kind=in->kind;out->min=in->minimum;out->max=in->maximum;
        out->value=in->initial;
        out->value=load_value(m,out);
        m->callback(out->key,out->value);
    }
    if(g_window) {refresh_list();populate_fields();}
    return 1;
}
__declspec(dllexport) void WINAPI W335GUI_Unregister(const char *id) {
    int index;
    if(!on_thread() || !id || (index=find_id(id))<0)return;
    if(g_window)clear_fields();
    if((unsigned)index+1u<g_mod_count)
        memmove(&g_modules[index],&g_modules[index+1],
                (g_mod_count-(unsigned)index-1u)*sizeof(g_modules[0]));
    --g_mod_count;
    if(g_selected>=g_mod_count)g_selected=0;
    if(g_window){refresh_list();populate_fields();}
}
__declspec(dllexport) UINT WINAPI W335_MessageId(void) {
    return RegisterWindowMessageA(GUI_MSG);
}
static void control(UINT msg,WPARAM command,HWND hwnd) {
    if(!g_msg)g_msg=RegisterWindowMessageA(GUI_MSG);
    if(msg!=g_msg)return;
    if(command==0u) {
        g_enabled=0u;
        if(on_thread() && g_window)DestroyWindow(g_window);
        g_mod_count=0u;g_selected=0u;g_page=0u;
        return;
    }
    if(command!=1u && command!=2u)return;
    if(!g_tid)g_tid=GetCurrentThreadId();
    if(!on_thread())return;
    if(hwnd && IsWindow(hwnd)) {
        HWND root=GetAncestor(hwnd,GA_ROOT);
        if(root && GetWindowThreadProcessId(root,NULL)==g_tid && root!=g_window)
            g_game=root;
    }
    if(!g_ini[0]) {
        wchar_t *slash;
        if(GetModuleFileNameW(NULL,g_ini,MAX_PATH) &&
           (slash=wcsrchr(g_ini,L'\\'))!=NULL)
            wcscpy_s(slash+1,MAX_PATH-(size_t)(slash+1-g_ini),L"WoW335GUI.ini");
        else g_ini[0]=0;
    }
    g_enabled=1u;
    if(g_window && (DWORD)(GetTickCount()-g_last_refresh)>=2000u) {
        g_last_refresh=GetTickCount();
        refresh_list();
    }
}
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code,WPARAM w,LPARAM l) {
    if(code>=0 && l) {
        const MSG *message=(const MSG *)l;
        control(message->message,message->wParam,message->hwnd);
        if(g_enabled && on_thread() && w==PM_REMOVE &&
           message->message==WM_KEYUP && message->wParam==VK_INSERT &&
           !(GetKeyState(VK_CONTROL)&0x8000) &&
           !(GetKeyState(VK_SHIFT)&0x8000) &&
           (message->hwnd==g_game || message->hwnd==g_window ||
            (g_window && IsChild(g_window,message->hwnd))))
            toggle_window();
    }
    return CallNextHookEx(NULL,code,w,l);
}
__declspec(dllexport) LRESULT CALLBACK W335_CallWndProc(int code,WPARAM w,LPARAM l) {
    if(code>=0 && l) {
        const CWPSTRUCT *m=(const CWPSTRUCT *)l;
        control(m->message,m->wParam,m->hwnd);
    }
    return CallNextHookEx(NULL,code,w,l);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) {
        g_instance=module;DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
