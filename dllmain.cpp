#include <Windows.h>
#include <d3d9.h>
#include <math.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "../images/stb_image.h"
#include "../images/crosshair.h"
#include "../images/eye.h"
#include "../images/localplayer.h"
#include "../images/players.h"
#include "../images/vehicles.h"
#include "../images/keybind.h"
#include "../images/server.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx9.h"
#include "../includes/minhook/include/MinHook.h"
#pragma comment(lib, "d3d9.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
typedef HRESULT(WINAPI* tEndScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(WINAPI* tReset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
typedef HRESULT(WINAPI* tPresent)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef BOOL(WINAPI* tSetCursorPos)(int, int);
typedef BOOL(WINAPI* tClipCursor)(const RECT*);
typedef UINT(WINAPI* tGetRawInputData)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
tEndScene oEndScene=nullptr;
tReset oReset=nullptr;
tPresent oPresent=nullptr;
tSetCursorPos oSetCursorPos=nullptr;
tClipCursor oClipCursor=nullptr;
tGetRawInputData oGetRawInputData=nullptr;
WNDPROC oWndProc=nullptr;
HWND gWindow=nullptr;
HMODULE gModule=nullptr;
bool gInitialized=false;
bool gMenuOpen=true;
bool gShouldUnload=false;
bool gShutdownComplete=false;
static int current_tab=0;
static float anim_slide_y=0.0f,anim_slide_target=0.0f;
static bool anim_first=true;
static IDirect3DTexture9* tex_crosshair=nullptr;
static IDirect3DTexture9* tex_eye=nullptr;
static IDirect3DTexture9* tex_localplayer=nullptr;
static IDirect3DTexture9* tex_players=nullptr;
static IDirect3DTexture9* tex_vehicles=nullptr;
static IDirect3DTexture9* tex_keybind=nullptr;
static IDirect3DTexture9* tex_server=nullptr;
struct Vec3{float x,y,z;};
struct Player{char name[28];float hp,armor;Vec3 pos;float dist;bool valid,isLocal;DWORD ped;int id,weaponId,vehicleModelId,skinId;};
struct NearbyVehicle{DWORD ptr;int modelId;float dist;Vec3 pos;bool hasDriver;char driverName[28];};
struct SampPlayer{int id;char name[28];float hp,armor;DWORD pData;};
struct AdminTrack{int id;DWORD flyStartTime;bool inAir;bool confirmedAdmin;Vec3 lastPos;};
static DWORD g_sampBase=0;
static int g_screenW=800,g_screenH=600;
static float g_vpMat[16]={};
static Vec3 g_camPos={},g_myPos={};
static bool g_haveMe=false,g_camOk=false;
static float g_fov=70.f,g_crossX=0.5f,g_crossY=0.5f;
static std::vector<Player> g_players;
static std::vector<NearbyVehicle> g_nearbyVehicles;
static std::vector<SampPlayer> g_sampList;
static std::vector<std::string> g_friends;
static std::vector<AdminTrack> g_adminTracks;
static int g_selPlayerId=-1,g_actorCPedOff=-1,g_online=0,g_drawn=0;
static int g_spectatingId=-1;
static bool g_isSpectating=false;
static Vec3 g_specLastPos={};
static bool g_tpPending=false;
static int g_tpPhase=0;
static DWORD g_tpTimer=0;
static Vec3 g_tpTargetPos={};
static int g_tpTargetPlayerId=-1;
static bool g_tpToWaypoint=false;
static constexpr float kPi=3.14159265358979323846f;
#define COL_ACCENT IM_COL32(139,92,246,255)
#define COL_ACCENT_HOV IM_COL32(159,112,255,255)
#define COL_ACCENT_ACT IM_COL32(120,72,226,255)
#define COL_ACCENT_DIM IM_COL32(139,92,246,60)
#define COL_BG IM_COL32(15,17,23,255)
#define COL_SIDEBAR IM_COL32(20,22,30,255)
#define COL_CARD IM_COL32(26,28,38,255)
#define COL_CARD_HOVER IM_COL32(34,36,48,255)
#define COL_TEXT IM_COL32(230,230,240,255)
#define COL_TEXT_DIM IM_COL32(140,140,160,255)
#define COL_TOGGLE_OFF IM_COL32(50,52,65,255)
static bool esp_p=false,esp_p_box=false,esp_p_box_fill=false,esp_p_name=false,esp_p_hp=false,esp_p_hp_dynamic=true,esp_p_dist=false,esp_p_weapon=false,esp_p_lines=false,esp_p_skeleton=false;
static float esp_p_box_col[4]={1,1,1,1},esp_p_name_col[4]={1,1,1,1},esp_p_hp_col[4]={0,1,0,1},esp_p_dist_col[4]={1,1,1,1},esp_p_weapon_col[4]={1,0.8f,0,1},esp_p_lines_col[4]={1,1,1,0.5f},esp_p_skel_col[4]={1,1,1,1};
static bool esp_l=false,esp_l_box=false,esp_l_box_fill=false,esp_l_name=false,esp_l_hp=false,esp_l_hp_dynamic=true,esp_l_dist=false,esp_l_skeleton=false;
static float esp_l_box_col[4]={0,1,0.4f,1},esp_l_name_col[4]={0,1,0.4f,1},esp_l_hp_col[4]={0,1,0,1},esp_l_dist_col[4]={0,1,0.4f,1},esp_l_skel_col[4]={0,1,0.4f,1};
static bool esp_v=false,esp_v_box=false,esp_v_box_fill=false,esp_v_name=false,esp_v_dist=false,esp_v_driver=false;
static float esp_v_max_dist=300.0f,esp_v_box_col[4]={1,0.6f,0,1},esp_v_name_col[4]={1,0.6f,0,1},esp_v_dist_col[4]={1,0.6f,0,1},esp_v_driver_col[4]={1,1,1,1};
static bool esp_admin=false,esp_admin_box=true,esp_admin_box_fill=false,esp_admin_name=true,esp_admin_dist=true,esp_admin_hp=false,esp_admin_lines=false,esp_admin_skeleton=false,esp_admin_popup=true;
static bool aim_on=false,aim_show_fov=false,wait_aim_key=false,aim_visicheck=false;
static int aim_smooth=1,aim_fov_size=150,aim_key=0,aim_target=0,aim_priority=0;
static float aim_fov_col[4]={139/255.f,92/255.f,246/255.f,1};
static bool boost_on=false,wait_boost_key=false;
static int boost_key=0;
static float boost_amount=250.0f,boost_max=1000.0f;
static bool magnet_on=false,magnet_show_fov=false,wait_magnet_key=false;
static int magnet_key=0,magnet_target=0,magnet_priority=0;
static float magnet_fov=250.0f,magnet_distance=3.0f,magnet_offsetX=0.4f;
static float magnet_fov_col[4]={1,0.3f,0.3f,1};
static int magnet_lockedId=-1;
static bool magnet_pull_all=false;
static bool proaim_on=false,proaim_show_fov=false,wait_proaim_key=false;
static int proaim_key=0,proaim_target=0,proaim_priority=0;
static int proaim_fov=250;
static float proaim_stickiness=0.85f;
static float proaim_fov_col[4]={1,0.2f,0.2f,1};
static bool proaim_hasTarget=false;
static float proaim_origCX=0.5f,proaim_origCY=0.5f;
static bool proaim_savedPos=false;
static bool flycar_on=false,wait_flycar_key=false;
static int flycar_key=0;
static float flycar_speed=5.0f;
static bool fly_on=false,wait_fly_key=false;
static int fly_key=0;
static float fly_speed=2.0f;
static bool godmode_on=false;
static bool noclip_on=false;
static float noclip_speed=5.0f;
static bool tpkill_active=false;
static Vec3 tpkill_myPos={0,0,0};
static DWORD tpkill_timer=0;
static bool tp_way_on=false;
static DWORD tp_way_timer=0;
static bool tp_way_active=false;
static Vec3 tp_way_target={0,0,0};
static const int ADMIN_SKIN_ID=217;
static bool slingshot_on = false;
static bool wait_slingshot_key = false;
static int slingshot_key = 0;
static float slingshot_speed = 130.0f;
static Vec3 slingshot_startPos = {0, 0, 0};
static bool slingshot_active = false;
static DWORD slingshot_timer = 0;
static int slingshot_phase = 0;
static Vec3 slingshot_forward = {0, 0, 0};
static float slingshot_startYaw = 0.0f;
static bool slingshot_isBike = false;
static bool insta_brake_on=false,wait_insta_brake_key=false;
static int insta_brake_key=0;
static inline bool Valid(DWORD p){return p>=0x400000&&p<0x80000000u;}
static inline bool IsFiniteF(float v){return isfinite(v)!=0;}
static inline bool IsValidPos(const Vec3&v){return IsFiniteF(v.x)&&IsFiniteF(v.y)&&IsFiniteF(v.z)&&fabsf(v.x)<30000&&fabsf(v.y)<30000&&fabsf(v.z)<30000;}
static inline ImU32 Col(const float c[4]){return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0],c[1],c[2],c[3]));}
static bool SafeRead(DWORD a,void*b,SIZE_T s){if(!a||IsBadReadPtr((void*)a,s))return false;memcpy(b,(void*)a,s);return true;}
template<class T>static bool RV(DWORD a,T&v){return SafeRead(a,&v,sizeof(T));}
static bool Rd(DWORD a,void*b,SIZE_T s){return SafeRead(a,b,s);}
static bool RP(DWORD a,DWORD&o){o=0;if(!RV(a,o))return false;return Valid(o);}
static bool g_antiACHooked=false;

static const char* g_weaponNames[]={"Fist","Brass Knuckles","Golf Club","Nightstick","Knife","Baseball Bat","Shovel","Pool Cue","Katana","Chainsaw","Purple Dildo","Dildo","Vibrator","Silver Vibrator","Flowers","Cane","Grenade","Tear Gas","Molotov","_","_","_","Colt 45","Silenced","Desert Eagle","Shotgun","Sawnoff","Combat Shotgun","Micro SMG","MP5","AK-47","M4","Tec-9","Country Rifle","Sniper Rifle","RPG","HS Rocket","Flamethrower","Minigun","Satchel Charge","Detonator","Spraycan","Fire Extinguisher","Camera","Night Vision","Thermal","Parachute"};
static const char* GetWeaponName(int id){return(id>=0&&id<47)?g_weaponNames[id]:"Unknown";}
static const char* g_vehicleNames[]={"Landstalker","Bravura","Buffalo","Linerunner","Perennial","Sentinel","Dumper","Firetruck","Trashmaster","Stretch","Manana","Infernus","Voodoo","Pony","Mule","Cheetah","Ambulance","Leviathan","Moonbeam","Esperanto","Taxi","Washington","Bobcat","Mr Whoopee","BF Injection","Hunter","Premier","Enforcer","Securicar","Banshee","Predator","Bus","Rhino","Barracks","Hotknife","Trailer 1","Previon","Coach","Cabbie","Stallion","Rumpo","RC Bandit","Romero","Packer","Monster","Admiral","Squalo","Seasparrow","Pizzaboy","Tram","Trailer 2","Turismo","Speeder","Reefer","Tropic","Flatbed","Yankee","Caddy","Solair","Berkley","Skimmer","PCJ-600","Faggio","Freeway","RC Baron","RC Raider","Glendale","Oceanic","Sanchez","Sparrow","Patriot","Quad","Coastguard","Dinghy","Hermes","Sabre","Rustler","ZR-350","Walton","Regina","Comet","BMX","Burrito","Camper","Marquis","Baggage","Dozer","Maverick","News Chopper","Rancher","FBI Rancher","Virgo","Greenwood","Jetmax","Hotring Racer","Sandking","Blista Compact","Police Maverick","Boxville","Benson","Mesa","RC Goblin","Hotring Racer A","Hotring Racer B","Bloodring Banger","Rancher","Super GT","Elegant","Journey","Bike","Mountain Bike","Beagle","Cropduster","Stuntplane","Tanker","Roadtrain","Nebula","Majestic","Buccaneer","Shamal","Hydra","FCR-900","NRG-500","HPV1000","Cement Truck","Tow Truck","Fortune","Cadrona","FBI Truck","Willard","Forklift","Tractor","Combine","Feltzer","Remington","Slamvan","Blade","Freight","Streak","Vortex","Vincent","Bullet","Clover","Sadler","Firetruck LA","Hustler","Intruder","Primo","Cargobob","Tampa","Sunrise","Merit","Utility","Nevada","Yosemite","Windsor","Monster A","Monster B","Uranus","Jester","Sultan","Stratum","Elegy","Raindance","RC Tiger","Flash","Tahoma","Savanna","Bandito","Freight Flat","Streak Carriage","Kart","Mower","Duneride","Sweeper","Broadway","Tornado","AT-400","DFT-30","Huntley","Stafford","BF-400","Newsvan","Tug","Trailer 3","Emperor","Wayfarer","Euros","Hotdog","Club","Freight Carriage","Trailer 3","Andromada","Dodo","RC Cam","Launch","Police Car LS","Police Car SF","Police Car LV","Police Ranger","Picador","S.W.A.T.","Alpha","Phoenix","Glendale","Sadler","Luggage Trailer A","Luggage Trailer B","Stair Trailer","Boxville","Farm Plow","Utility Trailer"};
static const char* GetVehicleName(int m){int i=m-400;return(i>=0&&i<212)?g_vehicleNames[i]:"Unknown";}
static bool IsFriend(const char*n){if(!n||!*n)return false;for(const auto&f:g_friends)if(f==n)return true;return false;}
static void ToggleFriend(const char*n){if(!n||!*n)return;for(auto it=g_friends.begin();it!=g_friends.end();++it)if(*it==n){g_friends.erase(it);return;}g_friends.push_back(n);}
static bool PedPos(DWORD p,Vec3&v){if(!Valid(p))return false;DWORD m=0;if(!RP(p+0x14,m)||!Valid(m))return false;Vec3 pp;if(!Rd(m+0x30,&pp,12))return false;if(!IsValidPos(pp))return false;v=pp;return true;}
static bool MyPos(Vec3&v){DWORD me=0;if(!RP(0xB6F5F0,me)||!Valid(me))return false;return PedPos(me,v);}
static int GetPedWeaponId(DWORD p){if(!Valid(p))return 0;BYTE s=0;if(!RV(p+0x718,s)||s>12)return 0;DWORD w=0;if(!RV(p+0x5A0+(DWORD)s*0x1C,w))return 0;return(int)w;}
static int GetPedVehicleModelId(DWORD p){if(!Valid(p))return 0;DWORD v=0;if(!RP(p+0x58C,v)||!Valid(v))return 0;WORD m=0;if(!RV(v+0x22,m))return 0;return(int)m;}
static int GetPedSkinId(DWORD p){if(!Valid(p))return -1;WORD s=0;if(!RV(p+0x22,s))return -1;return(int)s;}
static bool UpdateCamera(){float m[16];if(!Rd(0xB6FA2C,m,64)){g_camOk=false;return false;}bool z=true;for(int i=0;i<16;i++)if(m[i]!=0.f){z=false;break;}if(z){g_camOk=false;return false;}memcpy(g_vpMat,m,64);struct{Vec3 r,u,a,p;}cm;if(Rd(0xB6F028+0x974,&cm,48)&&IsValidPos(cm.p))g_camPos=cm.p;BYTE idx=0;if(RV(0xB6F028+0x59,idx)&&idx<3){float f=70;if(RV(0xB6F028+0x174+(DWORD)idx*0x238+0xB4,f)&&IsFiniteF(f)&&f>0.01f){g_fov=(f<=kPi)?f:f*kPi/180;if(g_fov<.1f||g_fov>3.1f)g_fov=70*kPi/180;}}g_camOk=true;float cx=0.5f,cy=0.5f;RV(0xB6EC14,cx);RV(0xB6EC10,cy);if(IsFiniteF(cx)&&IsFiniteF(cy)&&cx>=0&&cx<=1&&cy>=0&&cy<=1){g_crossX=cx;g_crossY=cy;}return true;}
static bool W2S(const Vec3&w,ImVec2&o){if(!g_camOk||g_screenW<=0||g_screenH<=0||!IsValidPos(w))return false;float cx=w.x*g_vpMat[0]+w.y*g_vpMat[4]+w.z*g_vpMat[8]+g_vpMat[12];float cy=w.x*g_vpMat[1]+w.y*g_vpMat[5]+w.z*g_vpMat[9]+g_vpMat[13];float cz=w.x*g_vpMat[2]+w.y*g_vpMat[6]+w.z*g_vpMat[10]+g_vpMat[14];if(cz<0.01f)return false;float inv=1.f/cz;float sx=cx*inv*(float)g_screenW,sy=cy*inv*(float)g_screenH;if(!isfinite(sx)||!isfinite(sy))return false;o.x=sx;o.y=sy;return(o.x>-400&&o.x<g_screenW+400&&o.y>-400&&o.y<g_screenH+400);}
static bool ScanActorForCPed(DWORD a,DWORD&o){if(!Valid(a))return false;if(g_actorCPedOff>=0){DWORD p=0;if(RP(a+g_actorCPedOff,p)&&Valid(p)){Vec3 pos;if(PedPos(p,pos)){o=p;return true;}}g_actorCPedOff=-1;}for(DWORD off=0;off<0x348;off+=4){if(off==0x44)continue;DWORD c=0;if(!RV(a+off,c)||!Valid(c))continue;DWORD m=0;if(!RP(c+0x14,m)||!Valid(m))continue;Vec3 pos;if(!Rd(m+0x30,&pos,12)||!IsValidPos(pos))continue;if(fabsf(pos.x)<.1f&&fabsf(pos.y)<.1f&&fabsf(pos.z)<.1f)continue;if(g_haveMe){float dx=pos.x-g_myPos.x,dy=pos.y-g_myPos.y,dz=pos.z-g_myPos.z;if(sqrtf(dx*dx+dy*dy+dz*dz)>50000)continue;}g_actorCPedOff=(int)off;o=c;return true;}return false;}
static bool HasLineOfSight(const Vec3&from,const Vec3&to){
    if(!IsValidPos(from)||!IsValidPos(to))return true;
    typedef bool(__cdecl*tProcessLineOfSight)(Vec3*,Vec3*,Vec3*,void*,bool,bool,bool,bool,bool,bool,bool,bool);
    tProcessLineOfSight fn=(tProcessLineOfSight)0x56BA00;
    if(IsBadCodePtr((FARPROC)fn))return true;
    Vec3 fromCopy=from,toCopy=to;
    Vec3 colPoint={0,0,0};
    char colData[0x30]={0};
    bool blocked=fn(&fromCopy,&toCopy,&colPoint,colData,true,false,false,true,true,false,false,false);
    return !blocked;
}
static bool GetPedBonePos(DWORD ped,int boneId,Vec3&out){if(!Valid(ped))return false;if(IsBadReadPtr((void*)ped,0x800))return false;DWORD skeleton=0;if(!RV(ped+0x18,skeleton)||!Valid(skeleton))return false;if(IsBadReadPtr((void*)skeleton,0x40))return false;DWORD animBlend=0;if(!RV(skeleton+0x10,animBlend)||!Valid(animBlend))return false;if(IsBadReadPtr((void*)animBlend,0x20))return false;typedef bool(__thiscall*tGetBone)(DWORD,Vec3*,int,bool);tGetBone GetBonePosition=(tGetBone)0x5E4280;if(IsBadCodePtr((FARPROC)GetBonePosition))return false;Vec3 pos={0,0,0};if(!IsBadWritePtr(&pos,12)){bool ok=GetBonePosition(ped,&pos,boneId,true);if(!ok)return false;}else return false;if(!IsValidPos(pos))return false;if(fabsf(pos.x)<0.01f&&fabsf(pos.y)<0.01f&&fabsf(pos.z)<0.01f)return false;out=pos;return true;}
static bool IsPedOnGround(DWORD ped){
    if(!Valid(ped))return false;
    BYTE flags=0;
    if(RV(ped+0x46,flags)){if(flags&0x02)return true;}
    DWORD standingOn=0;
    if(RP(ped+0x53C,standingOn)&&Valid(standingOn))return true;
    Vec3 vel;
    if(Rd(ped+0x44,&vel,12)){if(fabsf(vel.z)<0.01f&&sqrtf(vel.x*vel.x+vel.y*vel.y)<0.5f)return true;}
    return false;
}
static void ReadSampPlayers(std::vector<SampPlayer>&o){
    o.clear();if(!g_sampBase)return;
    DWORD info=0,pools=0,pp=0;
    DWORD sampOff1=0x21A0F8,sampOff2=0x3CD,sampOff3=0x18,sampOff4=0x2E;
    if(!RP(g_sampBase+sampOff1,info)||!RP(info+sampOff2,pools)||!RP(pools+sampOff3,pp))return;
    int mx=0;Rd(pp,&mx,4);if(mx<=0||mx>1004)mx=1004;
    for(int i=0;i<=mx;i++){
        DWORD rp=0;if(!RP(pp+sampOff4+(DWORD)i*4,rp)||!Valid(rp))continue;
        DWORD pd=0;if(!RP(rp,pd)||!Valid(pd))continue;
        float hp=0;if(!RV(pd+0x1BC,hp)||!IsFiniteF(hp)||hp<=0||hp>200)continue;
        SampPlayer sp={};sp.id=i;sp.hp=(hp>100)?100:hp;sp.pData=pd;
        float ar=0;if(RV(pd+0x1B8,ar)&&IsFiniteF(ar)&&ar>0&&ar<=100)sp.armor=ar;
        char nm[25]={0};
        if(Rd(rp+0xC,nm,24)){
            int l=0;while(l<24&&nm[l]>=32&&nm[l]<=126)l++;
            bool ok=false;for(int j=0;j<l;j++)if((nm[j]>='A'&&nm[j]<='Z')||(nm[j]>='a'&&nm[j]<='z')){ok=true;break;}
            if(ok&&l>=2){memcpy(sp.name,nm,l);sp.name[l]=0;}else snprintf(sp.name,24,"ID_%u",(unsigned)i);
        }else snprintf(sp.name,24,"ID_%u",(unsigned)i);
        o.push_back(sp);
    }
}
static void LoadPlayers(){static DWORD lt=0;DWORD now=GetTickCount();if(now-lt<16)return;lt=now;g_players.clear();g_haveMe=MyPos(g_myPos);if(g_haveMe){DWORD lp=0;RP(0xB6F5F0,lp);if(Valid(lp)){float hp=0;RV(lp+0x540,hp);Player pl={};strcpy(pl.name,"YOU");pl.hp=(hp>0&&hp<=200)?(hp>100?100:hp):100;pl.pos=g_myPos;pl.ped=lp;pl.valid=true;pl.isLocal=true;pl.weaponId=GetPedWeaponId(lp);pl.vehicleModelId=GetPedVehicleModelId(lp);pl.skinId=GetPedSkinId(lp);g_players.push_back(pl);}}if(!g_sampBase)return;std::vector<SampPlayer>sp;ReadSampPlayers(sp);g_online=(int)sp.size();g_sampList=sp;for(auto&s:sp){DWORD a=0;if(!RP(s.pData+0,a)||!Valid(a))continue;DWORD ped=0;if(!ScanActorForCPed(a,ped))continue;Vec3 pos;if(!PedPos(ped,pos))continue;Player pl={};memcpy(pl.name,s.name,28);pl.hp=s.hp;pl.armor=s.armor;pl.pos=pos;pl.ped=ped;pl.valid=true;pl.id=s.id;pl.weaponId=GetPedWeaponId(ped);pl.vehicleModelId=GetPedVehicleModelId(ped);pl.skinId=GetPedSkinId(ped);if(g_haveMe){float dx=pos.x-g_myPos.x,dy=pos.y-g_myPos.y,dz=pos.z-g_myPos.z;pl.dist=sqrtf(dx*dx+dy*dy+dz*dz);}g_players.push_back(pl);}}
static void UpdateAdminTracking(){
    DWORD now=GetTickCount();
    for(auto&p:g_players){
        if(p.isLocal||!p.valid||!Valid(p.ped))continue;
        AdminTrack*track=nullptr;
        for(auto&t:g_adminTracks)if(t.id==p.id){track=&t;break;}
        if(!track){AdminTrack nt={p.id,0,false,false,{0,0,0}};g_adminTracks.push_back(nt);track=&g_adminTracks.back();}
        if(p.skinId==ADMIN_SKIN_ID){track->confirmedAdmin=true;continue;}
        bool inVehicle=false;DWORD pv=0;
        if(RP(p.ped+0x58C,pv)&&Valid(pv))inVehicle=true;
        bool onGround=IsPedOnGround(p.ped);
        bool floating=false;
        if(!inVehicle&&!onGround){
            Vec3 vel;
            if(Rd(p.ped+0x44,&vel,12)&&IsValidPos(vel)){
                float velMag=sqrtf(vel.x*vel.x+vel.y*vel.y+vel.z*vel.z);
                if(fabsf(vel.z)<0.05f&&velMag<0.5f){
                    if(track->flyStartTime==0){track->flyStartTime=now;track->inAir=true;}
                    else{if(now-track->flyStartTime>3000)floating=true;}
                }else{track->flyStartTime=0;track->inAir=false;}
            }
        }else{
            track->flyStartTime=0;track->inAir=false;
            if(p.skinId!=ADMIN_SKIN_ID)track->confirmedAdmin=false;
        }
        if(floating&&p.pos.z>15.0f)track->confirmedAdmin=true;
    }
    for(auto it=g_adminTracks.begin();it!=g_adminTracks.end();){
        bool found=false;
        for(const auto&p:g_players)if(p.id==it->id&&!p.isLocal){found=true;break;}
        if(!found)it=g_adminTracks.erase(it);else ++it;
    }
}
static bool IsAdmin(int pid){if(!esp_admin)return false;for(const auto&t:g_adminTracks)if(t.id==pid)return t.confirmedAdmin;return false;}
static void ScanNearbyVehicles(){static DWORD ls=0;DWORD now=GetTickCount();if(now-ls<500)return;ls=now;g_nearbyVehicles.clear();DWORD myPed=0;if(!RP(0xB6F5F0,myPed)||!Valid(myPed))return;Vec3 myPos;if(!PedPos(myPed,myPos))return;DWORD vp=0;if(!RV(0xB74494,vp)||!Valid(vp))return;DWORD obj=0,bm=0;int sz=0;if(!RV(vp+0,obj)||!RV(vp+4,bm)||!RV(vp+8,sz))return;if(!Valid(obj)||!Valid(bm)||sz<=0||sz>2000)return;for(int i=0;i<sz;i++){BYTE f=0;if(!Rd(bm+i,&f,1)||f&0x80)continue;DWORD veh=obj+(DWORD)i*0xA18;if(!Valid(veh))continue;DWORD vm=0;if(!RV(veh+0x14,vm)||!Valid(vm))continue;Vec3 vpos;if(!Rd(vm+0x30,&vpos,12)||!IsValidPos(vpos))continue;WORD mid=0;if(!RV(veh+0x22,mid)||mid<400||mid>611)continue;float dx=vpos.x-myPos.x,dy=vpos.y-myPos.y,dz=vpos.z-myPos.z;float dist=sqrtf(dx*dx+dy*dy+dz*dz);if(dist>1000.0f)continue;NearbyVehicle nv={};nv.ptr=veh;nv.modelId=mid;nv.dist=dist;nv.pos=vpos;DWORD dr=0;if(RV(veh+0x460,dr)&&Valid(dr))for(const auto&pl:g_players)if(pl.ped==dr&&!pl.isLocal){nv.hasDriver=true;strncpy(nv.driverName,pl.name,27);break;}g_nearbyVehicles.push_back(nv);}for(size_t i=0;i<g_nearbyVehicles.size();i++)for(size_t j=i+1;j<g_nearbyVehicles.size();j++)if(g_nearbyVehicles[j].dist<g_nearbyVehicles[i].dist){NearbyVehicle t=g_nearbyVehicles[i];g_nearbyVehicles[i]=g_nearbyVehicles[j];g_nearbyVehicles[j]=t;}}
static void JackVehicle(DWORD v){if(!Valid(v))return;DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;DWORD vm=0;if(!RP(v+0x14,vm)||!Valid(vm))return;Vec3 vp;if(!Rd(vm+0x30,&vp,12)||!IsValidPos(vp))return;Vec3 tp={vp.x,vp.y,vp.z+1.5f};DWORD mm=0;if(RP(mp+0x14,mm)&&Valid(mm))if(!IsBadWritePtr((void*)(mm+0x30),12))memcpy((void*)(mm+0x30),&tp,12);if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}}
static void RepairVehicle(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;DWORD v=0;if(!RP(mp+0x58C,v)||!Valid(v))return;if(!IsBadWritePtr((void*)(v+0x4C0),4))*(float*)(v+0x4C0)=1000.0f;if(!IsBadWritePtr((void*)(v+0x428),4))*(DWORD*)(v+0x428)=1000;if(!IsBadWritePtr((void*)(v+0x4A8),1))*(BYTE*)(v+0x4A8)=0;for(int i=0;i<4;i++){DWORD wheelOff=v+0x5A0+i*0x4;if(!IsBadWritePtr((void*)wheelOff,4))*(BYTE*)wheelOff=0;}}
static void ExplodeMyVehicle(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;DWORD v=0;if(!RP(mp+0x58C,v)||!Valid(v))return;if(!IsBadWritePtr((void*)(v+0x4C0),4))*(float*)(v+0x4C0)=0.0f;if(!IsBadWritePtr((void*)(v+0x428),4))*(DWORD*)(v+0x428)=0;if(!IsBadWritePtr((void*)(v+0x4A8),1))*(BYTE*)(v+0x4A8)=3;}
static void GiveFullHealth(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;if(!IsBadWritePtr((void*)(mp+0x540),4))*(float*)(mp+0x540)=100.0f;}
static void GiveFullArmor(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;if(!IsBadWritePtr((void*)(mp+0x548),4))*(float*)(mp+0x548)=100.0f;}
static void ResetAimState(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;if(!IsBadWritePtr((void*)(mp+0x79C),4))*(DWORD*)(mp+0x79C)=0;if(!IsBadWritePtr((void*)(0xB6F028+0x8),4))*(DWORD*)(0xB6F028+0x8)=0;DWORD pd=0;if(RP(mp+0x480,pd)&&Valid(pd))if(!IsBadWritePtr((void*)(pd+0x2C),4))*(DWORD*)(pd+0x2C)=0;if(!IsBadWritePtr((void*)(mp+0x530),4))*(DWORD*)(mp+0x530)=0;if(!IsBadWritePtr((void*)(0xB7CD68),1))*(BYTE*)(0xB7CD68)=0;}
static void RawWriteMyPos(Vec3 tp){
    DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;
    DWORD mv=0;RP(mp+0x58C,mv);
    if(Valid(mv)){DWORD m=0;if(RP(mv+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12))memcpy((void*)(m+0x30),&tp,12);if(!IsBadWritePtr((void*)(mv+0x44),12)){*(float*)(mv+0x44)=0;*(float*)(mv+0x48)=0;*(float*)(mv+0x4C)=0;}}
    else{DWORD m=0;if(RP(mp+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12))memcpy((void*)(m+0x30),&tp,12);}
    if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}
}
static void StartSpectateGhost(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;if(!IsBadReadPtr((void*)(mp+0x40),1)){BYTE f=*(BYTE*)(mp+0x40);*(BYTE*)(mp+0x40)=f|0x20;}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}}
static void StopSpectateGhost(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;if(!IsBadReadPtr((void*)(mp+0x40),1)){BYTE f=*(BYTE*)(mp+0x40);*(BYTE*)(mp+0x40)=f&~0x20;}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}}
static void TeleportToPlayer(int pid){
    for(const auto&pl:g_players){
        if(pl.id!=pid||!pl.valid)continue;
        Vec3 tp;if(!Valid(pl.ped)||!PedPos(pl.ped,tp)||!IsValidPos(tp))return;
        g_tpTargetPos={tp.x+2.0f,tp.y+2.0f,tp.z+1.0f};g_tpTargetPlayerId=pid;
        g_tpToWaypoint=false;g_tpPending=true;g_tpPhase=0;g_tpTimer=GetTickCount();return;
    }
}
static void ProcessTeleportSequence(){
    if(!g_tpPending)return;
    DWORD now=GetTickCount();
    if(g_tpPhase==0){
        StartSpectateGhost();
        g_tpPhase=1;
        g_tpTimer=now;
        return;
    }
    if(g_tpPhase==1){
        if(now-g_tpTimer<150)return;
        Vec3 finalPos;
        if(g_tpToWaypoint){
            finalPos=g_tpTargetPos;
        }else{
            bool found=false;
            for(const auto&pl:g_players){
                if(pl.id!=g_tpTargetPlayerId||!pl.valid)continue;
                Vec3 tp;
                if(Valid(pl.ped)&&PedPos(pl.ped,tp)&&IsValidPos(tp)){
                    finalPos={tp.x+2.0f,tp.y+2.0f,tp.z+1.0f};
                    found=true;
                }
                break;
            }
            if(!found){g_tpPending=false;g_tpPhase=0;return;}
        }
        RawWriteMyPos(finalPos);
        g_tpPhase=2;
        g_tpTimer=now;
        return;
    }
    if(g_tpPhase==2){
        if(now-g_tpTimer<100)return;
        Vec3 finalPos;
        if(g_tpToWaypoint){
            finalPos=g_tpTargetPos;
        }else{
            for(const auto&pl:g_players){
                if(pl.id!=g_tpTargetPlayerId||!pl.valid)continue;
                Vec3 tp;
                if(Valid(pl.ped)&&PedPos(pl.ped,tp)&&IsValidPos(tp)){
                    finalPos={tp.x+2.0f,tp.y+2.0f,tp.z+1.0f};
                }
                break;
            }
        }
        RawWriteMyPos(finalPos);
        StopSpectateGhost();
        ResetAimState();
        DWORD mp=0;
        if(RP(0xB6F5F0,mp)&&Valid(mp)){
            if(!IsBadWritePtr((void*)(mp+0x4C0),12)){
                *(float*)(mp+0x4C0)=0;
                *(float*)(mp+0x4C4)=0;
                *(float*)(mp+0x4C8)=0;
            }
            DWORD veh=0;
            if(RP(mp+0x58C,veh)&&Valid(veh)){
                if(!IsBadWritePtr((void*)(veh+0x44),12)){
                    *(float*)(veh+0x44)=0;
                    *(float*)(veh+0x48)=0;
                    *(float*)(veh+0x4C)=0;
                }
            }
        }
        g_tpPending=false;
        g_tpPhase=0;
    }
}
static void TeleportToWaypoint(){DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;Vec3 myPos;if(!PedPos(mp,myPos))return;Vec3 wp={0,0,0};bool found=false;for(int i=0;i<175&&!found;i++){DWORD b=0xBA86F0+i*0x28;BYTE act=0;if(!RV(b+0x0,act)||!act)continue;BYTE ty=0;if(!RV(b+0x1,ty)||ty!=1)continue;float x=0,y=0,z=0;if(!RV(b+0x4,x)||!RV(b+0x8,y)||!RV(b+0xC,z))continue;if(!IsFiniteF(x)||!IsFiniteF(y)||fabsf(x)<0.1f&&fabsf(y)<0.1f)continue;wp={x,y,(IsFiniteF(z)&&fabsf(z)>0.1f)?z:myPos.z};found=true;}for(int i=0;i<175&&!found;i++){DWORD b=0xBA0110+i*0x38;Vec3 pos;if(!Rd(b+0x4,&pos,12)||!IsValidPos(pos))continue;if(fabsf(pos.x)<0.1f&&fabsf(pos.y)<0.1f)continue;BYTE disp=0,bt=0;RV(b+0x28,disp);RV(b+0x2C,bt);if((bt==4&&disp>0)||bt==1){wp=pos;if(fabsf(wp.z)<0.1f)wp.z=myPos.z;found=true;}}if(!found&&g_sampBase)for(DWORD off=0x2AC000;off<0x2AD000&&!found;off+=4){BYTE fl=0;if(!RV(g_sampBase+off,fl)||fl!=1)continue;float x=0,y=0;if(!RV(g_sampBase+off+4,x)||!RV(g_sampBase+off+8,y))continue;if(IsFiniteF(x)&&IsFiniteF(y)&&(fabsf(x)>1||fabsf(y)>1)){wp={x,y,myPos.z};found=true;}}if(!found)return;DWORD mv=0;RP(mp+0x58C,mv);Vec3 tp={wp.x,wp.y,wp.z+5};if(Valid(mv)){DWORD m=0;if(RP(mv+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12))memcpy((void*)(m+0x30),&tp,12);if(!IsBadWritePtr((void*)(mv+0x44),12)){float*vel=(float*)(mv+0x44);vel[0]=0;vel[1]=0;vel[2]=0;}}else{DWORD m=0;if(RP(mp+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12))memcpy((void*)(m+0x30),&tp,12);}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}ResetAimState();}
static ImU32 GetDynamicHpColor(float hp){float r,g,b;if(hp>50){float t=(hp-50)/50;r=1-t;g=1;b=0;}else{float t=hp/50;r=1;g=t;b=0;}return IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),255);}
static void DrawSkeleton(ImDrawList*bg,DWORD ped,ImU32 color){if(!Valid(ped))return;Vec3 pedPos;if(!PedPos(ped,pedPos))return;float dx=pedPos.x-g_myPos.x,dy=pedPos.y-g_myPos.y,dz=pedPos.z-g_myPos.z;float dist=sqrtf(dx*dx+dy*dy+dz*dz);if(dist>200.0f)return;struct BoneLine{int a,b;};static const BoneLine bones[]={{7,4},{4,3},{3,1},{4,22},{22,23},{23,24},{4,32},{32,33},{33,34},{1,52},{52,53},{53,54},{1,42},{42,43},{43,44}};int bc=sizeof(bones)/sizeof(BoneLine);for(int i=0;i<bc;i++){Vec3 pa,pb;if(!GetPedBonePos(ped,bones[i].a,pa))continue;if(!GetPedBonePos(ped,bones[i].b,pb))continue;if(!IsValidPos(pa)||!IsValidPos(pb))continue;ImVec2 sa,sb;if(!W2S(pa,sa)||!W2S(pb,sb))continue;bg->AddLine(sa,sb,color,1.5f);}}
static void DrawTextOutlined(ImDrawList*dl,ImVec2 p,ImU32 c,const char*t){ImU32 blk=IM_COL32(0,0,0,255);dl->AddText(ImVec2(p.x-1,p.y),blk,t);dl->AddText(ImVec2(p.x+1,p.y),blk,t);dl->AddText(ImVec2(p.x,p.y-1),blk,t);dl->AddText(ImVec2(p.x,p.y+1),blk,t);dl->AddText(p,c,t);}
static void DrawVehicleESP(ImDrawList*bg){
    if(!esp_v||!g_camOk)return;
    ImU32 boxCol=Col(esp_v_box_col);ImU32 boxFill=(boxCol&0x00FFFFFF)|0x40000000;
    ImU32 nameCol=Col(esp_v_name_col);ImU32 distCol=Col(esp_v_dist_col);ImU32 driverCol=Col(esp_v_driver_col);
    for(const auto&nv:g_nearbyVehicles){
        if(nv.dist>esp_v_max_dist)continue;
        Vec3 bot={nv.pos.x,nv.pos.y,nv.pos.z-1.2f};Vec3 top={nv.pos.x,nv.pos.y,nv.pos.z+0.8f};
        ImVec2 sB,sT;if(!W2S(bot,sB)||!W2S(top,sT))continue;
        float bH=fabsf(sB.y-sT.y);if(bH<4)bH=4;float bW=bH*1.6f;float cx=(sB.x+sT.x)*0.5f;
        if(esp_v_box){if(esp_v_box_fill)bg->AddRectFilled(ImVec2(cx-bW*.5f,sT.y),ImVec2(cx+bW*.5f,sB.y),boxFill);bg->AddRect(ImVec2(cx-bW*.5f,sT.y),ImVec2(cx+bW*.5f,sB.y),boxCol,0,0,1.0f);}
        float yOff=0;Vec3 feet={nv.pos.x,nv.pos.y,nv.pos.z-1.2f};ImVec2 sFeet;
        if(W2S(feet,sFeet)){
            if(esp_v_name){const char*vn=GetVehicleName(nv.modelId);char lb[64];if(esp_v_dist)snprintf(lb,sizeof(lb),"%s [%.0fm]",vn,nv.dist);else snprintf(lb,sizeof(lb),"%s",vn);ImVec2 ts=ImGui::CalcTextSize(lb);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),nameCol,lb);yOff+=ts.y+2;}
            if(esp_v_dist&&!esp_v_name){char db[16];snprintf(db,sizeof(db),"%.0fm",nv.dist);ImVec2 ts=ImGui::CalcTextSize(db);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),distCol,db);yOff+=ts.y+2;}
            if(esp_v_driver&&nv.hasDriver){char db[64];snprintf(db,sizeof(db),"Driver: %s",nv.driverName);ImVec2 ts=ImGui::CalcTextSize(db);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),driverCol,db);}
        }
    }
}
static void DrawESP(ImDrawList*bg){
    g_drawn=0;if(!g_camOk)return;
    float screenCX=g_screenW*0.5f;float screenCY=(float)g_screenH;
    static float adminRainbow=0.0f;adminRainbow+=0.02f;if(adminRainbow>6.28f)adminRainbow-=6.28f;
    for(const auto&pl:g_players){
        if(!pl.valid)continue;
        bool loc=pl.isLocal;bool isAdm=!loc&&IsAdmin(pl.id);bool useAdminEsp=isAdm&&esp_admin;bool useNormalEsp=loc?esp_l:esp_p;
        if(!useAdminEsp&&!useNormalEsp)continue;
        ImU32 adminCol=0;
        if(isAdm){float r=sinf(adminRainbow)*0.5f+0.5f;float g=sinf(adminRainbow+2.09f)*0.5f+0.5f;float b=sinf(adminRainbow+4.18f)*0.5f+0.5f;adminCol=IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),255);}
        ImU32 boxCol,nameCol,distCol,skelCol;bool doBox,doBoxFill,doName,doHp,doHpDyn,doDist,doWeapon,doLines,doSkel;
        if(useAdminEsp){boxCol=adminCol;nameCol=adminCol;distCol=adminCol;skelCol=adminCol;doBox=esp_admin_box;doBoxFill=esp_admin_box_fill;doName=esp_admin_name;doHp=esp_admin_hp;doHpDyn=true;doDist=esp_admin_dist;doWeapon=false;doLines=esp_admin_lines;doSkel=esp_admin_skeleton;}
        else if(IsFriend(pl.name)){boxCol=IM_COL32(25,55,150,255);nameCol=boxCol;distCol=boxCol;skelCol=boxCol;doBox=loc?esp_l_box:esp_p_box;doBoxFill=loc?esp_l_box_fill:esp_p_box_fill;doName=loc?esp_l_name:esp_p_name;doHp=loc?esp_l_hp:esp_p_hp;doHpDyn=loc?esp_l_hp_dynamic:esp_p_hp_dynamic;doDist=loc?esp_l_dist:esp_p_dist;doWeapon=!loc&&esp_p_weapon;doLines=!loc&&esp_p_lines;doSkel=loc?esp_l_skeleton:esp_p_skeleton;}
        else{boxCol=Col(loc?esp_l_box_col:esp_p_box_col);nameCol=Col(loc?esp_l_name_col:esp_p_name_col);distCol=Col(loc?esp_l_dist_col:esp_p_dist_col);skelCol=Col(loc?esp_l_skel_col:esp_p_skel_col);doBox=loc?esp_l_box:esp_p_box;doBoxFill=loc?esp_l_box_fill:esp_p_box_fill;doName=loc?esp_l_name:esp_p_name;doHp=loc?esp_l_hp:esp_p_hp;doHpDyn=loc?esp_l_hp_dynamic:esp_p_hp_dynamic;doDist=loc?esp_l_dist:esp_p_dist;doWeapon=!loc&&esp_p_weapon;doLines=!loc&&esp_p_lines;doSkel=loc?esp_l_skeleton:esp_p_skeleton;}
        ImU32 boxFill=(boxCol&0x00FFFFFF)|0x40000000;
        ImU32 hpCol=doHpDyn?GetDynamicHpColor(pl.hp):(IsFriend(pl.name)?IM_COL32(25,55,150,255):Col(loc?esp_l_hp_col:esp_p_hp_col));
        ImU32 weapCol=Col(esp_p_weapon_col);
        Vec3 headBone;bool hasHead=GetPedBonePos(pl.ped,7,headBone);
        Vec3 topPos={pl.pos.x,pl.pos.y,hasHead?headBone.z+0.20f:pl.pos.z+1.0f};
        Vec3 botPos={pl.pos.x,pl.pos.y,pl.pos.z-1.0f};
        ImVec2 sB,sT;bool boxVisible=W2S(botPos,sB)&&W2S(topPos,sT);
        float bH=0,bW=0,boxCx=0;
        if(boxVisible){
            bH=fabsf(sB.y-sT.y);if(bH<4)bH=4;bW=bH*0.45f;boxCx=(sB.x+sT.x)*0.5f;
            if(doBox){if(doBoxFill)bg->AddRectFilled(ImVec2(boxCx-bW*.5f,sT.y),ImVec2(boxCx+bW*.5f,sB.y),boxFill);bg->AddRect(ImVec2(boxCx-bW*.5f,sT.y),ImVec2(boxCx+bW*.5f,sB.y),boxCol,0,0,1.0f);}
            if(doHp&&pl.hp>0){float hpBarW=3.0f,hpBarX=boxCx-bW*0.5f-hpBarW-3.0f;float hpFrac=pl.hp/100.0f;if(hpFrac>1)hpFrac=1;bg->AddRectFilled(ImVec2(hpBarX,sT.y),ImVec2(hpBarX+hpBarW,sB.y),IM_COL32(0,0,0,140));bg->AddRectFilled(ImVec2(hpBarX,sB.y-bH*hpFrac),ImVec2(hpBarX+hpBarW,sB.y),hpCol);bg->AddRect(ImVec2(hpBarX,sT.y),ImVec2(hpBarX+hpBarW,sB.y),IM_COL32(0,0,0,200),0,0,1.0f);}
            if(doLines){ImU32 lineCol=isAdm?adminCol:Col(esp_p_lines_col);bg->AddLine(ImVec2(screenCX,screenCY),ImVec2(boxCx,sB.y),lineCol,1.0f);}
        }
        if(doSkel&&Valid(pl.ped))DrawSkeleton(bg,pl.ped,skelCol);
        Vec3 feet={pl.pos.x,pl.pos.y,pl.pos.z-1.0f};ImVec2 sFeet;
        if(W2S(feet,sFeet)){
            float yOff=0;
            if(useAdminEsp&&doName){char ab[64];snprintf(ab,sizeof(ab),"[ADMIN] %s",pl.name);ImVec2 ts=ImGui::CalcTextSize(ab);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),nameCol,ab);yOff+=ts.y+2;}
            else if(doName){char lb[64];if(!loc&&pl.dist>=0&&doDist)snprintf(lb,sizeof(lb),"%s [%.0fm]",pl.name,pl.dist);else snprintf(lb,sizeof(lb),"%s",pl.name);ImVec2 ts=ImGui::CalcTextSize(lb);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),nameCol,lb);yOff+=ts.y+2;}
            if(!loc&&doDist&&!doName&&pl.dist>=0){char db[16];snprintf(db,sizeof(db),"%.0fm",pl.dist);ImVec2 ts=ImGui::CalcTextSize(db);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),distCol,db);yOff+=ts.y+2;}
            if(useAdminEsp&&doDist&&doName){char db[16];snprintf(db,sizeof(db),"%.0fm",pl.dist);ImVec2 ts=ImGui::CalcTextSize(db);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),distCol,db);yOff+=ts.y+2;}
            if(doWeapon&&pl.weaponId>0){const char*wn=GetWeaponName(pl.weaponId);ImVec2 ts=ImGui::CalcTextSize(wn);DrawTextOutlined(bg,ImVec2(sFeet.x-ts.x*.5f,sFeet.y+8+yOff),weapCol,wn);yOff+=ts.y+2;}
            if(!loc)g_drawn++;
        }
    }
}
static void DrawAdminPopup(ImDrawList*fg){
    if(!esp_admin||!esp_admin_popup)return;
    std::vector<const Player*>admins;
    for(const auto&pl:g_players){if(pl.isLocal||!pl.valid)continue;if(IsAdmin(pl.id))admins.push_back(&pl);}
    if(admins.empty())return;
    for(size_t i=0;i<admins.size();i++)for(size_t j=i+1;j<admins.size();j++)if(admins[j]->dist<admins[i]->dist){const Player*t=admins[i];admins[i]=admins[j];admins[j]=t;}
    float popW=220,popH=(float)(admins.size()*32+36),popX=20,popY=100;
    fg->AddRectFilled(ImVec2(popX,popY),ImVec2(popX+popW,popY+popH),IM_COL32(20,20,30,220),8);
    fg->AddRect(ImVec2(popX,popY),ImVec2(popX+popW,popY+popH),IM_COL32(139,92,246,180),8,0,1.0f);
    const char*title="ADMINS NEARBY";ImVec2 ts=ImGui::CalcTextSize(title);
    fg->AddText(ImVec2(popX+(popW-ts.x)/2,popY+10),IM_COL32(139,92,246,255),title);
    fg->AddLine(ImVec2(popX+10,popY+28),ImVec2(popX+popW-10,popY+28),IM_COL32(139,92,246,100),1.0f);
    float y=popY+34;
    for(const auto*ad:admins){
        char info[64];snprintf(info,sizeof(info),"[%d] %s",ad->id,ad->name);
        fg->AddText(ImVec2(popX+14,y),IM_COL32(220,220,240,255),info);
        char dist[16];snprintf(dist,sizeof(dist),"%.0fm",ad->dist);
        ImVec2 ds=ImGui::CalcTextSize(dist);
        fg->AddText(ImVec2(popX+popW-14-ds.x,y),IM_COL32(139,92,246,200),dist);
        y+=32;
    }
}
static const char* KeyName(int k){if(k==0)return"NONE";if(k==VK_LBUTTON)return"LMB";if(k==VK_RBUTTON)return"RMB";if(k==VK_MBUTTON)return"MMB";if(k==VK_XBUTTON1)return"MOUSE 4";if(k==VK_XBUTTON2)return"MOUSE 5";if(k==VK_INSERT)return"INSERT";if(k==VK_DELETE)return"DELETE";if(k==VK_HOME)return"HOME";if(k==VK_END)return"END";if(k==VK_PRIOR)return"PGUP";if(k==VK_NEXT)return"PGDN";if(k==VK_SPACE)return"SPACE";if(k==VK_SHIFT)return"SHIFT";if(k==VK_LSHIFT)return"LSHIFT";if(k==VK_RSHIFT)return"RSHIFT";if(k==VK_CONTROL)return"CTRL";if(k==VK_LCONTROL)return"LCTRL";if(k==VK_RCONTROL)return"RCTRL";if(k==VK_MENU)return"ALT";if(k==VK_LMENU)return"LALT";if(k==VK_RMENU)return"RALT";if(k==VK_TAB)return"TAB";if(k==VK_RETURN)return"ENTER";if(k==VK_BACK)return"BACKSPACE";if(k==VK_CAPITAL)return"CAPS";if(k==VK_ESCAPE)return"ESC";if(k>='A'&&k<='Z'){static char b[2];b[0]=(char)k;b[1]=0;return b;}if(k>='0'&&k<='9'){static char b2[2];b2[0]=(char)k;b2[1]=0;return b2;}if(k>=VK_F1&&k<=VK_F12){static char b3[4];snprintf(b3,4,"F%d",k-VK_F1+1);return b3;}if(k>=VK_NUMPAD0&&k<=VK_NUMPAD9){static char b4[8];snprintf(b4,8,"NUM%d",k-VK_NUMPAD0);return b4;}return"KEY";}
static void TooltipHover(const char*tip){if(!tip||!*tip)return;if(ImGui::IsItemHovered()){ImGuiContext*ctx=ImGui::GetCurrentContext();if(ctx->HoveredIdTimer>2.0f){ImGui::BeginTooltip();ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));ImGui::Text("%s",tip);ImGui::PopStyleColor();ImGui::EndTooltip();}}}
static bool KeybindCard(const char*id,const char*label,int key,bool waiting,const char*tip=nullptr){
    ImDrawList*d=ImGui::GetWindowDrawList();ImVec2 pos=ImGui::GetCursorScreenPos();ImVec2 avail=ImGui::GetContentRegionAvail();float height=38.0f;d->AddRectFilled(pos,ImVec2(pos.x+avail.x,pos.y+height),COL_CARD,6);float iconSize=20.0f;float iconX=pos.x+avail.x-iconSize-14;float iconY=pos.y+(height-iconSize)/2;if(tex_keybind)d->AddImage((ImTextureID)tex_keybind,ImVec2(iconX,iconY),ImVec2(iconX+iconSize,iconY+iconSize),ImVec2(0,0),ImVec2(1,1),COL_ACCENT);d->AddText(ImVec2(pos.x+12,pos.y+4),COL_TEXT_DIM,label);char kt[32];if(waiting)strcpy(kt,"[ pressione ]");else if(key==0)strcpy(kt,"[ nenhuma ]");else snprintf(kt,32,"[ %s ]",KeyName(key));d->AddText(ImVec2(pos.x+12,pos.y+18),COL_ACCENT,kt);ImGui::PushID(id);ImGui::SetCursorScreenPos(pos);bool clicked=ImGui::InvisibleButton("##kc",ImVec2(avail.x,height));TooltipHover(tip);ImGui::PopID();ImGui::Dummy(ImVec2(avail.x,4));return clicked;
}
static bool ToggleColorCard(const char*label,bool*v,float col[4],const char*tip=nullptr){
    ImDrawList*dl=ImGui::GetWindowDrawList();ImVec2 pos=ImGui::GetCursorScreenPos();ImVec2 avail=ImGui::GetContentRegionAvail();
    float height=26.0f;float bxSz=18.0f;float bxX=pos.x+10;float bxY=pos.y+(height-bxSz)/2;
    ImU32 c=ImGui::ColorConvertFloat4ToU32(ImVec4(col[0],col[1],col[2],col[3]));
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(ImVec2(bxX,bxY));ImGui::InvisibleButton("##cb",ImVec2(bxSz,bxSz));
    bool colClicked=ImGui::IsItemClicked(0);if(colClicked)ImGui::OpenPopup("##cpk");
    ImGui::SetCursorScreenPos(pos);ImGui::InvisibleButton("##c",ImVec2(avail.x,height));
    bool clicked=ImGui::IsItemClicked(0)&&!colClicked;TooltipHover(tip);
    ImVec2 mp=ImGui::GetMousePos();bool onColor=(mp.x>=bxX&&mp.x<=bxX+bxSz&&mp.y>=bxY&&mp.y<=bxY+bxSz);
    if(clicked&&!onColor)*v=!*v;bool hovered=ImGui::IsItemHovered();
    dl->AddRectFilled(pos,ImVec2(pos.x+avail.x,pos.y+height),hovered?COL_CARD_HOVER:COL_CARD,6.0f);
    dl->AddRectFilled(ImVec2(bxX,bxY),ImVec2(bxX+bxSz,bxY+bxSz),c,4);
    dl->AddRect(ImVec2(bxX,bxY),ImVec2(bxX+bxSz,bxY+bxSz),IM_COL32(255,255,255,80),4);
    const char*sep=strstr(label,"##");const char*de=sep?sep:(label+strlen(label));
    ImVec2 ts=ImGui::CalcTextSize(label,de);
    dl->AddText(ImVec2(bxX+bxSz+10,pos.y+(height-ts.y)/2),*v?COL_TEXT:COL_TEXT_DIM,label,de);
    float tW=30,tH=16,tR=tH*0.5f;float tx=pos.x+avail.x-tW-12;float ty=pos.y+(height-tH)/2;
    ImGuiID id=ImGui::GetID(label);ImGuiStorage*st=ImGui::GetStateStorage();
    float aT=st->GetFloat(id,*v?1.0f:0.0f);float target=*v?1.0f:0.0f;
    aT+=(target-aT)*0.25f;if(fabsf(target-aT)<0.01f)aT=target;st->SetFloat(id,aT);
    int br=50+(int)((139-50)*aT),bg=52+(int)((92-52)*aT),bb=65+(int)((246-65)*aT);
    dl->AddRectFilled(ImVec2(tx,ty),ImVec2(tx+tW,ty+tH),IM_COL32(br,bg,bb,255),tR);
    float cX=tx+tR+(tW-tH)*aT;dl->AddCircleFilled(ImVec2(cX,ty+tR),tR-2.5f,IM_COL32(255,255,255,255));
    if(ImGui::BeginPopup("##cpk")){ImGui::ColorPicker4("##pk",col,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);ImGui::EndPopup();}
    ImGui::PopID();ImGui::Dummy(ImVec2(avail.x,2));return clicked;
}
static bool ToggleCard(const char*label,bool*v,const char*tip=nullptr){
    ImDrawList*dl=ImGui::GetWindowDrawList();ImVec2 pos=ImGui::GetCursorScreenPos();ImVec2 avail=ImGui::GetContentRegionAvail();float height=26.0f;
    ImGui::PushID(label);ImGui::InvisibleButton("##c",ImVec2(avail.x,height));bool clicked=ImGui::IsItemClicked(0);bool hovered=ImGui::IsItemHovered();TooltipHover(tip);if(clicked)*v=!*v;ImGui::PopID();
    dl->AddRectFilled(pos,ImVec2(pos.x+avail.x,pos.y+height),hovered?COL_CARD_HOVER:COL_CARD,6.0f);
    const char*sep=strstr(label,"##");const char*de=sep?sep:(label+strlen(label));ImVec2 ts=ImGui::CalcTextSize(label,de);
    dl->AddText(ImVec2(pos.x+12,pos.y+(height-ts.y)/2),*v?COL_TEXT:COL_TEXT_DIM,label,de);
    float tW=30,tH=16,tR=tH*0.5f;float tx=pos.x+avail.x-tW-12;float ty=pos.y+(height-tH)/2;
    ImGuiID id=ImGui::GetID(label);ImGuiStorage*st=ImGui::GetStateStorage();
    float aT=st->GetFloat(id,*v?1.0f:0.0f);float target=*v?1.0f:0.0f;aT+=(target-aT)*0.25f;if(fabsf(target-aT)<0.01f)aT=target;st->SetFloat(id,aT);
    int br=50+(int)((139-50)*aT),bg=52+(int)((92-52)*aT),bb=65+(int)((246-65)*aT);
    dl->AddRectFilled(ImVec2(tx,ty),ImVec2(tx+tW,ty+tH),IM_COL32(br,bg,bb,255),tR);
    float cX=tx+tR+(tW-tH)*aT;dl->AddCircleFilled(ImVec2(cX,ty+tR),tR-2.5f,IM_COL32(255,255,255,255));
    ImGui::Dummy(ImVec2(avail.x,2));return clicked;
}
static void SliderCard(const char*label,float*v,float mn,float mx,const char*fmt="%.0f",const char*tip=nullptr){
    ImDrawList*dl=ImGui::GetWindowDrawList();ImVec2 pos=ImGui::GetCursorScreenPos();ImVec2 avail=ImGui::GetContentRegionAvail();float height=36.0f;
    dl->AddRectFilled(pos,ImVec2(pos.x+avail.x,pos.y+height),COL_CARD,6.0f);
    const char*sep=strstr(label,"##");const char*de=sep?sep:(label+strlen(label));
    dl->AddText(ImVec2(pos.x+12,pos.y+4),COL_TEXT,label,de);
    char vb[32];snprintf(vb,sizeof(vb),fmt,*v);ImVec2 vs=ImGui::CalcTextSize(vb);
    dl->AddText(ImVec2(pos.x+avail.x-vs.x-12,pos.y+4),COL_TEXT,vb);
    float barY=pos.y+22,barW=avail.x-24,barX=pos.x+12,barH=5;
    dl->AddRectFilled(ImVec2(barX,barY),ImVec2(barX+barW,barY+barH),COL_TOGGLE_OFF,barH*0.5f);
    float frac=(*v-mn)/(mx-mn);if(frac<0)frac=0;if(frac>1)frac=1;
    dl->AddRectFilled(ImVec2(barX,barY),ImVec2(barX+barW*frac,barY+barH),COL_ACCENT,barH*0.5f);
    dl->AddCircleFilled(ImVec2(barX+barW*frac,barY+barH*0.5f),5,IM_COL32(255,255,255,255));
    ImGui::PushID(label);ImGui::SetCursorScreenPos(ImVec2(barX-6,barY-8));ImGui::InvisibleButton("##s",ImVec2(barW+12,20));TooltipHover(tip);
    if(ImGui::IsItemActive()){float mpx=ImGui::GetIO().MousePos.x-barX;float f=mpx/barW;if(f<0)f=0;if(f>1)f=1;*v=mn+f*(mx-mn);}
    ImGui::PopID();ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+height));ImGui::Dummy(ImVec2(avail.x,2));
}
static void SliderIntCard(const char*label,int*v,int mn,int mx,const char*tip=nullptr){float fv=(float)*v;SliderCard(label,&fv,(float)mn,(float)mx,"%.0f",tip);*v=(int)fv;}
static void ComboCard(const char*label,int*current,const char**items,int cnt,const char*tip=nullptr){
    ImDrawList*dl=ImGui::GetWindowDrawList();ImVec2 pos=ImGui::GetCursorScreenPos();ImVec2 avail=ImGui::GetContentRegionAvail();float height=48.0f;
    dl->AddRectFilled(pos,ImVec2(pos.x+avail.x,pos.y+height),COL_CARD,6.0f);
    const char*sep=strstr(label,"##");const char*de=sep?sep:(label+strlen(label));
    dl->AddText(ImVec2(pos.x+12,pos.y+5),COL_TEXT_DIM,label,de);dl->AddText(ImVec2(pos.x+12,pos.y+24),COL_TEXT,items[*current]);
    float aX=pos.x+avail.x-20,aY=pos.y+height/2;dl->AddTriangleFilled(ImVec2(aX-5,aY-3),ImVec2(aX+5,aY-3),ImVec2(aX,aY+4),COL_ACCENT);
    ImGui::PushID(label);ImGui::SetCursorScreenPos(pos);ImGui::InvisibleButton("##cc",ImVec2(avail.x,height));TooltipHover(tip);
    bool clicked=ImGui::IsItemClicked(0);ImGuiID id=ImGui::GetID("pp");ImGuiStorage*st=ImGui::GetStateStorage();bool isOpen=st->GetBool(id,false);
    if(clicked){isOpen=!isOpen;st->SetBool(id,isOpen);}
    if(isOpen){
        float itemH=30,popH=cnt*itemH+8;ImVec2 pp(pos.x,pos.y+height+4);
        if(ImGui::IsMouseClicked(0)){ImVec2 mp=ImGui::GetMousePos();bool ip=(mp.x>=pp.x&&mp.x<=pp.x+avail.x&&mp.y>=pp.y&&mp.y<=pp.y+popH);bool ic=(mp.x>=pos.x&&mp.x<=pos.x+avail.x&&mp.y>=pos.y&&mp.y<=pos.y+height);if(!ip&&!ic){isOpen=false;st->SetBool(id,false);}}
        ImDrawList*fg=ImGui::GetForegroundDrawList();fg->AddRectFilled(pp,ImVec2(pp.x+avail.x,pp.y+popH),IM_COL32(30,32,42,255),6.0f);fg->AddRect(pp,ImVec2(pp.x+avail.x,pp.y+popH),COL_ACCENT_DIM,6.0f,0,1.0f);
        for(int i=0;i<cnt;i++){ImVec2 iMin(pp.x+4,pp.y+4+i*itemH);ImVec2 iMax(pp.x+avail.x-4,iMin.y+itemH);ImVec2 mp=ImGui::GetMousePos();bool hov=(mp.x>=iMin.x&&mp.x<=iMax.x&&mp.y>=iMin.y&&mp.y<=iMax.y);if(hov)fg->AddRectFilled(iMin,iMax,COL_ACCENT_DIM,4.0f);if(*current==i)fg->AddRectFilled(iMin,iMax,COL_ACCENT_DIM,4.0f);fg->AddText(ImVec2(iMin.x+10,iMin.y+(itemH-ImGui::GetFontSize())/2),(*current==i)?COL_ACCENT:COL_TEXT,items[i]);if(hov&&ImGui::IsMouseClicked(0)){*current=i;isOpen=false;st->SetBool(id,false);}}
    }
    ImGui::PopID();ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+height));ImGui::Dummy(ImVec2(avail.x,4));
}
IDirect3DTexture9* LoadTextureFromMemory(LPDIRECT3DDEVICE9 pDevice,const unsigned char*data,int size){int w,h,ch;unsigned char*img=stbi_load_from_memory(data,size,&w,&h,&ch,4);if(!img)return nullptr;for(int i=0;i<w*h;i++){unsigned char r=img[i*4+0];unsigned char b=img[i*4+2];img[i*4+0]=b;img[i*4+2]=r;}IDirect3DTexture9*tex=nullptr;if(pDevice->CreateTexture(w,h,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,nullptr)!=D3D_OK){stbi_image_free(img);return nullptr;}D3DLOCKED_RECT lr;if(tex->LockRect(0,&lr,nullptr,D3DLOCK_DISCARD)==D3D_OK){for(int y=0;y<h;y++)memcpy((unsigned char*)lr.pBits+y*lr.Pitch,img+y*w*4,w*4);tex->UnlockRect(0);}stbi_image_free(img);return tex;}
void SetLinearFilter(LPDIRECT3DDEVICE9 pDevice){if(!pDevice)return;pDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);pDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);pDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);}
void ReleaseTextures(){if(tex_crosshair){tex_crosshair->Release();tex_crosshair=nullptr;}if(tex_eye){tex_eye->Release();tex_eye=nullptr;}if(tex_localplayer){tex_localplayer->Release();tex_localplayer=nullptr;}if(tex_players){tex_players->Release();tex_players=nullptr;}if(tex_vehicles){tex_vehicles->Release();tex_vehicles=nullptr;}if(tex_keybind){tex_keybind->Release();tex_keybind=nullptr;}if(tex_server){tex_server->Release();tex_server=nullptr;}}
BOOL WINAPI hkSetCursorPos(int X,int Y){if(gMenuOpen)return TRUE;return oSetCursorPos(X,Y);}
BOOL WINAPI hkClipCursor(const RECT*r){if(gMenuOpen)return oClipCursor(nullptr);return oClipCursor(r);}
UINT WINAPI hkGetRawInputData(HRAWINPUT h,UINT c,LPVOID d,PUINT s,UINT hs){UINT r=oGetRawInputData(h,c,d,s,hs);if(gMenuOpen&&d&&c==RID_INPUT){RAWINPUT*ri=(RAWINPUT*)d;if(ri->header.dwType==RIM_TYPEMOUSE){ri->data.mouse.lLastX=0;ri->data.mouse.lLastY=0;}}return r;}
void FreeMouse(){if(oClipCursor)oClipCursor(nullptr);else ClipCursor(nullptr);while(::ShowCursor(TRUE)<0);}
void LockMouse(){while(::ShowCursor(FALSE)>=0);if(gWindow){RECT r;GetClientRect(gWindow,&r);POINT p={0,0};ClientToScreen(gWindow,&p);r.left+=p.x;r.top+=p.y;r.right+=p.x;r.bottom+=p.y;if(oClipCursor)oClipCursor(&r);else ClipCursor(&r);}}
void* GetD3D9DeviceVTable(){IDirect3D9*p=Direct3DCreate9(D3D_SDK_VERSION);if(!p)return nullptr;D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=GetDesktopWindow();pp.BackBufferFormat=D3DFMT_UNKNOWN;IDirect3DDevice9*d=nullptr;if(FAILED(p->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&d))){p->Release();return nullptr;}void**vt=*(void***)d;static void*cp[119];memcpy(cp,vt,sizeof(cp));d->Release();p->Release();return cp;}
LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(gShutdownComplete) return CallWindowProc(oWndProc, h, m, w, l);
    if(m == WM_KEYDOWN && w == VK_INSERT) {
        gMenuOpen = !gMenuOpen;
        if(gMenuOpen) { LockMouse(); }
        else { FreeMouse(); }
        return true;
    }
    if(gMenuOpen && m == WM_SETCURSOR) {
        SetCursor(NULL);
        return TRUE;
    }
    if(gMenuOpen) {
        ImGui_ImplWin32_WndProcHandler(h, m, w, l);
        switch(m) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEMOVE:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return true;
        }
    }
    return CallWindowProc(oWndProc, h, m, w, l);
}
static void SendSpectate(int pid){for(const auto&pl:g_players){if(pl.id!=pid||!pl.valid||!Valid(pl.ped))continue;DWORD mp=0;if(!RP(0xB6F5F0,mp)||!Valid(mp))return;DWORD m=0;if(RP(mp+0x14,m)&&Valid(m)){Vec3 cp;if(Rd(m+0x30,&cp,12)&&IsValidPos(cp))g_specLastPos=cp;}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}if(!IsBadReadPtr((void*)(mp+0x40),1)){BYTE f=*(BYTE*)(mp+0x40);*(BYTE*)(mp+0x40)=f|0x20;}g_isSpectating=true;g_spectatingId=pid;return;}}
static void StopSpectate(){DWORD mp=0;if(RP(0xB6F5F0,mp)&&Valid(mp)){if(IsValidPos(g_specLastPos)){DWORD m=0;if(RP(mp+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12))memcpy((void*)(m+0x30),&g_specLastPos,12);}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}if(!IsBadReadPtr((void*)(mp+0x40),1)){BYTE f=*(BYTE*)(mp+0x40);*(BYTE*)(mp+0x40)=f&~0x20;}}g_isSpectating=false;g_spectatingId=-1;}
static void ProcessSpectate(){if(!g_isSpectating||g_spectatingId<0)return;bool found=false;for(const auto&pl:g_players){if(pl.id!=g_spectatingId)continue;if(!pl.valid||!Valid(pl.ped))break;found=true;DWORD mp=0;if(RP(0xB6F5F0,mp)&&Valid(mp)){DWORD m=0;if(RP(mp+0x14,m)&&Valid(m))if(!IsBadWritePtr((void*)(m+0x30),12)){Vec3 off={pl.pos.x,pl.pos.y-4.0f,pl.pos.z+3.5f};memcpy((void*)(m+0x30),&off,12);}if(!IsBadWritePtr((void*)(mp+0x4C0),12)){*(float*)(mp+0x4C0)=0;*(float*)(mp+0x4C4)=0;*(float*)(mp+0x4C8)=0;}}break;}if(!found)StopSpectate();}
static void LoadConfig() {
    std::ifstream f("menu.cfg");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        #define LOAD_BOOL(x) if (key == #x) x = (val == "1")
        #define LOAD_INT(x) if (key == #x) x = std::stoi(val)
        #define LOAD_FLOAT(x) if (key == #x) x = std::stof(val)
        #define LOAD_COLOR(c) if (key == #c) { std::stringstream ss(val); std::string tok; int i=0; while (std::getline(ss, tok, ',') && i<4) c[i++] = std::stof(tok); }
        LOAD_BOOL(esp_p); LOAD_BOOL(esp_p_box); LOAD_BOOL(esp_p_box_fill); LOAD_BOOL(esp_p_name);
        LOAD_BOOL(esp_p_hp); LOAD_BOOL(esp_p_hp_dynamic); LOAD_BOOL(esp_p_dist); LOAD_BOOL(esp_p_weapon);
        LOAD_BOOL(esp_p_lines); LOAD_BOOL(esp_p_skeleton);
        LOAD_COLOR(esp_p_box_col); LOAD_COLOR(esp_p_name_col); LOAD_COLOR(esp_p_hp_col);
        LOAD_COLOR(esp_p_dist_col); LOAD_COLOR(esp_p_weapon_col); LOAD_COLOR(esp_p_lines_col); LOAD_COLOR(esp_p_skel_col);
        LOAD_BOOL(esp_l); LOAD_BOOL(esp_l_box); LOAD_BOOL(esp_l_box_fill); LOAD_BOOL(esp_l_name);
        LOAD_BOOL(esp_l_hp); LOAD_BOOL(esp_l_hp_dynamic); LOAD_BOOL(esp_l_dist); LOAD_BOOL(esp_l_skeleton);
        LOAD_COLOR(esp_l_box_col); LOAD_COLOR(esp_l_name_col); LOAD_COLOR(esp_l_hp_col);
        LOAD_COLOR(esp_l_dist_col); LOAD_COLOR(esp_l_skel_col);
        LOAD_BOOL(esp_v); LOAD_BOOL(esp_v_box); LOAD_BOOL(esp_v_box_fill); LOAD_BOOL(esp_v_name);
        LOAD_BOOL(esp_v_dist); LOAD_BOOL(esp_v_driver); LOAD_FLOAT(esp_v_max_dist);
        LOAD_COLOR(esp_v_box_col); LOAD_COLOR(esp_v_name_col); LOAD_COLOR(esp_v_dist_col); LOAD_COLOR(esp_v_driver_col);
        LOAD_BOOL(esp_admin); LOAD_BOOL(esp_admin_box); LOAD_BOOL(esp_admin_box_fill);
        LOAD_BOOL(esp_admin_name); LOAD_BOOL(esp_admin_dist); LOAD_BOOL(esp_admin_hp);
        LOAD_BOOL(esp_admin_lines); LOAD_BOOL(esp_admin_skeleton); LOAD_BOOL(esp_admin_popup);
        LOAD_BOOL(aim_on); LOAD_BOOL(aim_show_fov); LOAD_BOOL(aim_visicheck);
        LOAD_INT(aim_smooth); LOAD_INT(aim_fov_size); LOAD_INT(aim_key); LOAD_INT(aim_target); LOAD_INT(aim_priority);
        LOAD_COLOR(aim_fov_col);
        LOAD_BOOL(boost_on); LOAD_INT(boost_key); LOAD_FLOAT(boost_amount); LOAD_FLOAT(boost_max);
        LOAD_BOOL(magnet_on); LOAD_BOOL(magnet_show_fov); LOAD_INT(magnet_key); LOAD_INT(magnet_target); LOAD_INT(magnet_priority);
        LOAD_FLOAT(magnet_fov); LOAD_FLOAT(magnet_distance); LOAD_FLOAT(magnet_offsetX);
        LOAD_COLOR(magnet_fov_col);
        LOAD_BOOL(magnet_pull_all);
        LOAD_BOOL(proaim_on); LOAD_BOOL(proaim_show_fov); LOAD_INT(proaim_key);
        LOAD_INT(proaim_target); LOAD_INT(proaim_priority); LOAD_INT(proaim_fov); LOAD_FLOAT(proaim_stickiness);
        LOAD_COLOR(proaim_fov_col);
        LOAD_BOOL(flycar_on); LOAD_INT(flycar_key); LOAD_FLOAT(flycar_speed);
        LOAD_BOOL(fly_on); LOAD_INT(fly_key); LOAD_FLOAT(fly_speed);
        LOAD_BOOL(godmode_on); LOAD_BOOL(tp_way_on);
        LOAD_BOOL(noclip_on); LOAD_FLOAT(noclip_speed);
        LOAD_BOOL(slingshot_on); LOAD_INT(slingshot_key); LOAD_FLOAT(slingshot_speed);
        LOAD_BOOL(insta_brake_on); LOAD_INT(insta_brake_key);
        if (key == "friends") {
            g_friends.clear();
            std::stringstream ss(val);
            std::string name;
            while (std::getline(ss, name, ',')) if (!name.empty()) g_friends.push_back(name);
        }
        #undef LOAD_BOOL
        #undef LOAD_INT
        #undef LOAD_FLOAT
        #undef LOAD_COLOR
    }
    f.close();
}
static void SaveConfig() {
    std::ofstream f("menu.cfg");
    if (!f.is_open()) return;
    #define SAVE_BOOL(x) f << #x "=" << (x ? "1" : "0") << "\n"
    #define SAVE_INT(x) f << #x "=" << x << "\n"
    #define SAVE_FLOAT(x) f << #x "=" << x << "\n"
    #define SAVE_COLOR(c) f << #c "=" << c[0] << "," << c[1] << "," << c[2] << "," << c[3] << "\n"
    SAVE_BOOL(esp_p); SAVE_BOOL(esp_p_box); SAVE_BOOL(esp_p_box_fill); SAVE_BOOL(esp_p_name);
    SAVE_BOOL(esp_p_hp); SAVE_BOOL(esp_p_hp_dynamic); SAVE_BOOL(esp_p_dist); SAVE_BOOL(esp_p_weapon);
    SAVE_BOOL(esp_p_lines); SAVE_BOOL(esp_p_skeleton);
    SAVE_COLOR(esp_p_box_col); SAVE_COLOR(esp_p_name_col); SAVE_COLOR(esp_p_hp_col);
    SAVE_COLOR(esp_p_dist_col); SAVE_COLOR(esp_p_weapon_col); SAVE_COLOR(esp_p_lines_col); SAVE_COLOR(esp_p_skel_col);
    SAVE_BOOL(esp_l); SAVE_BOOL(esp_l_box); SAVE_BOOL(esp_l_box_fill); SAVE_BOOL(esp_l_name);
    SAVE_BOOL(esp_l_hp); SAVE_BOOL(esp_l_hp_dynamic); SAVE_BOOL(esp_l_dist); SAVE_BOOL(esp_l_skeleton);
    SAVE_COLOR(esp_l_box_col); SAVE_COLOR(esp_l_name_col); SAVE_COLOR(esp_l_hp_col);
    SAVE_COLOR(esp_l_dist_col); SAVE_COLOR(esp_l_skel_col);
    SAVE_BOOL(esp_v); SAVE_BOOL(esp_v_box); SAVE_BOOL(esp_v_box_fill); SAVE_BOOL(esp_v_name);
    SAVE_BOOL(esp_v_dist); SAVE_BOOL(esp_v_driver); SAVE_FLOAT(esp_v_max_dist);
    SAVE_COLOR(esp_v_box_col); SAVE_COLOR(esp_v_name_col); SAVE_COLOR(esp_v_dist_col); SAVE_COLOR(esp_v_driver_col);
    SAVE_BOOL(esp_admin); SAVE_BOOL(esp_admin_box); SAVE_BOOL(esp_admin_box_fill);
    SAVE_BOOL(esp_admin_name); SAVE_BOOL(esp_admin_dist); SAVE_BOOL(esp_admin_hp);
    SAVE_BOOL(esp_admin_lines); SAVE_BOOL(esp_admin_skeleton); SAVE_BOOL(esp_admin_popup);
    SAVE_BOOL(aim_on); SAVE_BOOL(aim_show_fov); SAVE_BOOL(aim_visicheck);
    SAVE_INT(aim_smooth); SAVE_INT(aim_fov_size); SAVE_INT(aim_key); SAVE_INT(aim_target); SAVE_INT(aim_priority);
    SAVE_COLOR(aim_fov_col);
    SAVE_BOOL(boost_on); SAVE_INT(boost_key); SAVE_FLOAT(boost_amount); SAVE_FLOAT(boost_max);
    SAVE_BOOL(magnet_on); SAVE_BOOL(magnet_show_fov); SAVE_INT(magnet_key); SAVE_INT(magnet_target); SAVE_INT(magnet_priority);
    SAVE_FLOAT(magnet_fov); SAVE_FLOAT(magnet_distance); SAVE_FLOAT(magnet_offsetX);
    SAVE_COLOR(magnet_fov_col);
    SAVE_BOOL(magnet_pull_all);
    SAVE_BOOL(proaim_on); SAVE_BOOL(proaim_show_fov); SAVE_INT(proaim_key);
    SAVE_INT(proaim_target); SAVE_INT(proaim_priority); SAVE_INT(proaim_fov); SAVE_FLOAT(proaim_stickiness);
    SAVE_COLOR(proaim_fov_col);
    SAVE_BOOL(flycar_on); SAVE_INT(flycar_key); SAVE_FLOAT(flycar_speed);
    SAVE_BOOL(fly_on); SAVE_INT(fly_key); SAVE_FLOAT(fly_speed);
    SAVE_BOOL(godmode_on); SAVE_BOOL(tp_way_on);
    SAVE_BOOL(noclip_on); SAVE_FLOAT(noclip_speed);
    SAVE_BOOL(slingshot_on); SAVE_INT(slingshot_key); SAVE_FLOAT(slingshot_speed);
    SAVE_BOOL(insta_brake_on); SAVE_INT(insta_brake_key);
    f << "friends=";
    for (size_t i = 0; i < g_friends.size(); i++) {
        if (i > 0) f << ",";
        f << g_friends[i];
    }
    f << "\n";
    #undef SAVE_BOOL
    #undef SAVE_INT
    #undef SAVE_FLOAT
    #undef SAVE_COLOR
    f.close();
}
static void ProcessAimbot() {
    if(!aim_on || aim_key == 0 || !(GetAsyncKeyState(aim_key) & 0x8000) || !g_camOk) return;
    float cxS = g_crossX * (float)g_screenW;
    float cyS = g_crossY * (float)g_screenH;
    float fovR = (float)aim_fov_size;
    int bestI = -1;
    float bestDist = 999999.f;
    float bestHp = 999999.f;
    for(int i = 0; i < (int)g_players.size(); i++) {
        const auto& pl = g_players[i];
        if(pl.isLocal || !pl.valid || IsFriend(pl.name) || pl.hp <= 0) continue;
        Vec3 ap;
        Vec3 headB;
        if(aim_target == 0 && GetPedBonePos(pl.ped, 7, headB)) ap = headB;
        else {
            float aimZ = (aim_target == 0) ? 0.65f : (aim_target == 1) ? 0.55f : 0.35f;
            ap.x = pl.pos.x; ap.y = pl.pos.y; ap.z = pl.pos.z + aimZ;
        }
        ImVec2 sc;
        if(!W2S(ap, sc)) continue;
        float dx2 = sc.x - cxS, dy2 = sc.y - cyS;
        if(sqrtf(dx2*dx2 + dy2*dy2) > fovR) continue;
        if(aim_visicheck && !HasLineOfSight(g_camPos, ap)) continue;
        bool better = false;
        if(aim_priority == 0) {
            if(pl.dist < bestDist) better = true;
        } else {
            if(pl.hp < bestHp) better = true;
            else if(pl.hp == bestHp && pl.dist < bestDist) better = true;
        }
        if(better) { bestDist = pl.dist; bestHp = pl.hp; bestI = i; }
    }
    if(bestI < 0) return;
    const auto& t = g_players[bestI];
    Vec3 ap;
    Vec3 headB;
    if(aim_target == 0 && GetPedBonePos(t.ped, 7, headB)) ap = headB;
    else {
        float aimZ = (aim_target == 0) ? 0.65f : (aim_target == 1) ? 0.55f : 0.35f;
        ap.x = t.pos.x; ap.y = t.pos.y; ap.z = t.pos.z + aimZ;
    }
    ImVec2 sc;
    if(!W2S(ap, sc)) return;
    float pixDX = sc.x - cxS, pixDY = sc.y - cyS;
    float focal = ((float)g_screenH * 0.5f) / tanf(g_fov * 0.5f);
    if(focal < 1) return;
    float dyaw = pixDX / focal, dpitch = pixDY / focal;
    if(!IsFiniteF(dyaw) || !IsFiniteF(dpitch)) return;
    float maxStep = 0.5f;
    if(dyaw > maxStep) dyaw = maxStep;
    if(dyaw < -maxStep) dyaw = -maxStep;
    if(dpitch > maxStep) dpitch = maxStep;
    if(dpitch < -maxStep) dpitch = -maxStep;
    float sm = (float)aim_smooth;
    if(sm < 1) sm = 1;
    float cY = 0, cP = 0;
    RV(0xB6F258, cY); RV(0xB6F248, cP);
    if(!IsFiniteF(cY) || !IsFiniteF(cP)) return;
    float newYaw = cY - dyaw / sm, newPitch = cP - dpitch / sm;
    if(!IsFiniteF(newYaw) || !IsFiniteF(newPitch)) return;
    if(!IsBadWritePtr((void*)0xB6F258, 4)) *(float*)0xB6F258 = newYaw;
    if(!IsBadWritePtr((void*)0xB6F248, 4)) *(float*)0xB6F248 = newPitch;
}
static void ProcessProAim() {
    if(!proaim_on || proaim_key == 0 || !g_camOk) return;
    bool keyHeld = (GetAsyncKeyState(proaim_key) & 0x8000) != 0;
    if(g_crossX < 0.1f || g_crossX > 0.9f) g_crossX = 0.5f;
    if(g_crossY < 0.1f || g_crossY > 0.9f) g_crossY = 0.5f;
    float cxS = g_crossX * (float)g_screenW;
    float cyS = g_crossY * (float)g_screenH;
    float fovR = (float)proaim_fov;
    float aimZ = (proaim_target == 0) ? 0.75f : (proaim_target == 1) ? 0.55f : 0.35f;
    if(!keyHeld) {
        if(proaim_savedPos) {
            if(proaim_origCX < 0.1f || proaim_origCX > 0.9f) proaim_origCX = 0.5f;
            if(proaim_origCY < 0.1f || proaim_origCY > 0.9f) proaim_origCY = 0.5f;
            g_crossX = proaim_origCX; g_crossY = proaim_origCY;
            if(!IsBadWritePtr((void*)0xB6EC14, 4)) *(float*)0xB6EC14 = proaim_origCX;
            if(!IsBadWritePtr((void*)0xB6EC10, 4)) *(float*)0xB6EC10 = proaim_origCY;
            proaim_savedPos = false;
        }
        proaim_hasTarget = false;
        return;
    }
    int bestI = -1;
    float bestScreenDist = 999999.f;
    float bestHp = 999999.f;
    for(int i = 0; i < (int)g_players.size(); i++) {
        const auto& pl = g_players[i];
        if(pl.isLocal || !pl.valid || IsFriend(pl.name) || pl.hp <= 0) continue;
        Vec3 headB;
        if(proaim_target == 0) { if(!GetPedBonePos(pl.ped, 7, headB)) continue; }
        else { headB = {pl.pos.x, pl.pos.y, pl.pos.z + aimZ}; }
        ImVec2 sc;
        if(!W2S(headB, sc)) continue;
        float dx = sc.x - cxS, dy = sc.y - cyS;
        float screenDist = sqrtf(dx*dx + dy*dy);
        if(screenDist > fovR) continue;
        bool better = false;
        if(proaim_priority == 0) {
            if(screenDist < bestScreenDist) better = true;
        } else {
            if(pl.hp < bestHp) better = true;
            else if(pl.hp == bestHp && screenDist < bestScreenDist) better = true;
        }
        if(better) { bestScreenDist = screenDist; bestHp = pl.hp; bestI = i; }
    }
    if(bestI < 0) { proaim_hasTarget = false; return; }
    if(!proaim_savedPos) {
        proaim_origCX = (g_crossX >= 0.1f && g_crossX <= 0.9f) ? g_crossX : 0.5f;
        proaim_origCY = (g_crossY >= 0.1f && g_crossY <= 0.9f) ? g_crossY : 0.5f;
        proaim_savedPos = true;
    }
    const auto& t = g_players[bestI];
    Vec3 headB;
    if(proaim_target == 0) { if(!GetPedBonePos(t.ped, 7, headB)) { proaim_hasTarget = false; return; } }
    else { headB = {t.pos.x, t.pos.y, t.pos.z + aimZ}; }
    ImVec2 targetScreen;
    if(!W2S(headB, targetScreen)) { proaim_hasTarget = false; return; }
    float lerp = proaim_stickiness;
    float newCX = g_crossX + (targetScreen.x / (float)g_screenW - g_crossX) * lerp;
    float newCY = g_crossY + (targetScreen.y / (float)g_screenH - g_crossY) * lerp;
    if(newCX < 0.05f) newCX = 0.05f;
    if(newCX > 0.95f) newCX = 0.95f;
    if(newCY < 0.05f) newCY = 0.05f;
    if(newCY > 0.95f) newCY = 0.95f;
    if(IsFiniteF(newCX) && IsFiniteF(newCY)) {
        g_crossX = newCX; g_crossY = newCY;
        if(!IsBadWritePtr((void*)0xB6EC14, 4)) *(float*)0xB6EC14 = newCX;
        if(!IsBadWritePtr((void*)0xB6EC10, 4)) *(float*)0xB6EC10 = newCY;
    }
    proaim_hasTarget = true;
}
static void ProcessMagnet() {
    if(!magnet_on || magnet_key == 0 || !(GetAsyncKeyState(magnet_key) & 0x8000) || !g_haveMe || !g_camOk) {
        magnet_lockedId = -1;
        return;
    }
    DWORD myPed = 0;
    if(!RP(0xB6F5F0, myPed) || !Valid(myPed)) return;

    float cxS = g_crossX * (float)g_screenW;
    float cyS = g_crossY * (float)g_screenH;
    float fovR = magnet_fov;

    float yaw = 0, pitch = 0;
    RV(0xB6F258, yaw); RV(0xB6F248, pitch);

    float fx = -cosf(yaw) * cosf(pitch);
    float fy = -sinf(yaw) * cosf(pitch);
    float fz = sinf(pitch);

    float rightX = -sinf(yaw);
    float rightY = cosf(yaw);

    // Stable height relative to local player (prevents flickering when target is above/below)
    float stableZ = g_myPos.z + 0.75f;

    if (magnet_pull_all) {
        // PULL ALL - no priority, apply to every player inside FOV
        for (const auto& pl : g_players) {
            if (pl.isLocal || !pl.valid || !Valid(pl.ped) || pl.ped == myPed || IsFriend(pl.name) || pl.hp <= 0) continue;

            Vec3 ap = {pl.pos.x, pl.pos.y, pl.pos.z + 0.6f};
            ImVec2 sc;
            if (!W2S(ap, sc)) continue;

            float dx = sc.x - cxS;
            float dy = sc.y - cyS;
            float screenDist = sqrtf(dx*dx + dy*dy);
            if (screenDist > fovR) continue;

            Vec3 targetPos = {
                g_myPos.x + fx * magnet_distance + rightX * magnet_offsetX,
                g_myPos.y + fy * magnet_distance + rightY * magnet_offsetX,
                stableZ
            };

            if (!IsValidPos(targetPos)) continue;

            DWORD ped = pl.ped;
            for (int rep = 0; rep < 4; rep++) {
                DWORD m = 0;
                if (!RP(ped + 0x14, m) || !Valid(m)) break;
                if (IsBadWritePtr((void*)(m + 0x30), 12)) break;

                memcpy((void*)(m + 0x30), &targetPos, 12);

                if (!IsBadWritePtr((void*)(ped + 0x44), 12)) {
                    float* vel = (float*)(ped + 0x44);
                    vel[0] = 0; vel[1] = 0; vel[2] = 0;
                }
                if (!IsBadWritePtr((void*)(ped + 0x50), 12)) {
                    float* vel = (float*)(ped + 0x50);
                    vel[0] = 0; vel[1] = 0; vel[2] = 0;
                }
            }

            // Also try to update SAMP data for better sync
            if (g_sampBase && pl.id >= 0) {
                DWORD info = 0, pools = 0, pp = 0;
                if (RP(g_sampBase + 0x21A0F8, info) &&
                    RP(info + 0x3CD, pools) &&
                    RP(pools + 0x18, pp)) {
                    DWORD rp = 0;
                    if (RP(pp + 0x2E + (DWORD)pl.id * 4, rp) && Valid(rp)) {
                        DWORD pd = 0;
                        if (RP(rp, pd) && Valid(pd)) {
                            DWORD offsets[] = {0x36, 0x40, 0x54, 0x68, 0xF6, 0x110};
                            for (int k = 0; k < 6; k++) {
                                if (!IsBadWritePtr((void*)(pd + offsets[k]), 12))
                                    memcpy((void*)(pd + offsets[k]), &targetPos, 12);
                            }
                        }
                    }
                }
            }
        }
        magnet_lockedId = -1; // no single lock when pulling all
        return;
    }

    // === NORMAL MODE (single target with priority) ===
    float aimZ = (magnet_target == 0) ? 0.75f : (magnet_target == 1) ? 0.55f : 0.35f;

    if (magnet_lockedId >= 0) {
        const Player* locked = nullptr;
        for (const auto& pl : g_players) {
            if (pl.id == magnet_lockedId && !pl.isLocal && pl.valid) {
                locked = &pl; break;
            }
        }
        if (!locked || locked->hp <= 0) {
            magnet_lockedId = -1;
        } else {
            Vec3 targetPos = {
                g_myPos.x + fx * magnet_distance + rightX * magnet_offsetX,
                g_myPos.y + fy * magnet_distance + rightY * magnet_offsetX,
                stableZ
            };
            if (!IsValidPos(targetPos)) return;

            DWORD ped = locked->ped;
            for (int rep = 0; rep < 5; rep++) {
                DWORD m = 0;
                if (!RP(ped + 0x14, m) || !Valid(m)) break;
                if (IsBadWritePtr((void*)(m + 0x30), 12)) break;
                memcpy((void*)(m + 0x30), &targetPos, 12);

                if (!IsBadWritePtr((void*)(ped + 0x44), 12)) {
                    float* vel = (float*)(ped + 0x44);
                    vel[0] = 0; vel[1] = 0; vel[2] = 0;
                }
                if (!IsBadWritePtr((void*)(ped + 0x50), 12)) {
                    float* vel = (float*)(ped + 0x50);
                    vel[0] = 0; vel[1] = 0; vel[2] = 0;
                }
            }

            if (g_sampBase && locked->id >= 0) {
                DWORD info = 0, pools = 0, pp = 0;
                DWORD sOff1 = 0x21A0F8, sOff2 = 0x3CD, sOff3 = 0x18, sOff4 = 0x2E;
                if (RP(g_sampBase + sOff1, info) && RP(info + sOff2, pools) && RP(pools + sOff3, pp)) {
                    DWORD rp = 0;
                    if (RP(pp + sOff4 + (DWORD)locked->id * 4, rp) && Valid(rp)) {
                        DWORD pd = 0;
                        if (RP(rp, pd) && Valid(pd)) {
                            DWORD offsets[] = {0x36, 0x40, 0x54, 0x68, 0xF6, 0x110};
                            for (int k = 0; k < 6; k++) {
                                DWORD addr = pd + offsets[k];
                                if (!IsBadWritePtr((void*)addr, 12)) memcpy((void*)addr, &targetPos, 12);
                            }
                        }
                    }
                }
            }
            return;
        }
    }

    // Find best target (normal priority mode)
    int bestI = -1;
    float bestScreenDist = 999999.f;
    float bestHp = 999999.f;

    for (int i = 0; i < (int)g_players.size(); i++) {
        const auto& pl = g_players[i];
        if (pl.isLocal || !pl.valid || !Valid(pl.ped) || pl.ped == myPed || IsFriend(pl.name) || pl.hp <= 0) continue;

        Vec3 ap = {pl.pos.x, pl.pos.y, pl.pos.z + aimZ};
        ImVec2 sc;
        if (!W2S(ap, sc)) continue;

        float dx = sc.x - cxS, dy = sc.y - cyS;
        float screenDist = sqrtf(dx*dx + dy*dy);
        if (screenDist > fovR) continue;

        bool better = false;
        if (magnet_priority == 0) {
            if (screenDist < bestScreenDist) better = true;
        } else {
            if (pl.hp < bestHp) better = true;
            else if (pl.hp == bestHp && screenDist < bestScreenDist) better = true;
        }
        if (better) {
            bestScreenDist = screenDist;
            bestHp = pl.hp;
            bestI = i;
        }
    }

    if (bestI < 0) return;

    magnet_lockedId = g_players[bestI].id;
    const auto& t = g_players[bestI];

    Vec3 targetPos = {
        g_myPos.x + fx * magnet_distance + rightX * magnet_offsetX,
        g_myPos.y + fy * magnet_distance + rightY * magnet_offsetX,
        stableZ
    };

    if (!IsValidPos(targetPos)) return;

    DWORD ped = t.ped;
    for (int rep = 0; rep < 5; rep++) {
        DWORD m = 0;
        if (!RP(ped + 0x14, m) || !Valid(m)) break;
        if (IsBadWritePtr((void*)(m + 0x30), 12)) break;
        memcpy((void*)(m + 0x30), &targetPos, 12);

        if (!IsBadWritePtr((void*)(ped + 0x44), 12)) {
            float* vel = (float*)(ped + 0x44);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
        if (!IsBadWritePtr((void*)(ped + 0x50), 12)) {
            float* vel = (float*)(ped + 0x50);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
    }

    if (g_sampBase && t.id >= 0) {
        DWORD info = 0, pools = 0, pp = 0;
        DWORD sOff1 = 0x21A0F8, sOff2 = 0x3CD, sOff3 = 0x18, sOff4 = 0x2E;
        if (RP(g_sampBase + sOff1, info) && RP(info + sOff2, pools) && RP(pools + sOff3, pp)) {
            DWORD rp = 0;
            if (RP(pp + sOff4 + (DWORD)t.id * 4, rp) && Valid(rp)) {
                DWORD pd = 0;
                if (RP(rp, pd) && Valid(pd)) {
                    DWORD offsets[] = {0x36, 0x40, 0x54, 0x68, 0xF6, 0x110};
                    for (int k = 0; k < 6; k++) {
                        DWORD addr = pd + offsets[k];
                        if (!IsBadWritePtr((void*)addr, 12)) memcpy((void*)addr, &targetPos, 12);
                    }
                }
            }
        }
    }
}
static bool GetAimDirection(Vec3& dir) {
    float yaw = 0, pitch = 0;
    if(!RV(0xB6F258, yaw) || !RV(0xB6F248, pitch) || !IsFiniteF(yaw) || !IsFiniteF(pitch)) return false;
    dir.x = -cosf(yaw) * cosf(pitch);
    dir.y = -sinf(yaw) * cosf(pitch);
    dir.z = sinf(pitch);
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if(len < 0.001f) return false;
    dir.x /= len; dir.y /= len; dir.z /= len;
    return true;
}
static void ProcessFlyCar() {
    if(!flycar_on || flycar_key == 0 || !(GetAsyncKeyState(flycar_key) & 0x8000) || !g_camOk) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    DWORD veh = 0;
    if(!RP(mp + 0x58C, veh) || !Valid(veh)) return;
    DWORD vm = 0;
    if(!RP(veh + 0x14, vm) || !Valid(vm)) return;
    if(IsBadReadPtr((void*)veh, 0x600)) return;
    Vec3 dir;
    if(!GetAimDirection(dir)) return;
    Vec3 vpos;
    if(!Rd(vm + 0x30, &vpos, 12) || !IsValidPos(vpos)) return;
    float spd = flycar_speed * 0.05f;
    Vec3 targetPos = {vpos.x + dir.x*spd, vpos.y + dir.y*spd, vpos.z + dir.z*spd};
    if(!IsValidPos(targetPos)) return;
    for(int rep = 0; rep < 5; rep++) {
        if(!IsBadWritePtr((void*)(vm + 0x30), 12)) memcpy((void*)(vm + 0x30), &targetPos, 12);
        if(!IsBadWritePtr((void*)(veh + 0x44), 12)) {
            float* vel = (float*)(veh + 0x44);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
        if(!IsBadWritePtr((void*)(veh + 0x50), 12)) {
            float* vel = (float*)(veh + 0x50);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
    }
}
static void ProcessFly() {
    if(!fly_on || fly_key == 0 || !(GetAsyncKeyState(fly_key) & 0x8000) || !g_haveMe) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    Vec3 dir;
    if(!GetAimDirection(dir)) return;
    float spd = fly_speed * 0.1f;
    Vec3 nextPos = {g_myPos.x + dir.x*spd, g_myPos.y + dir.y*spd, g_myPos.z + dir.z*spd};
    if(!IsValidPos(nextPos)) return;
    DWORD m = 0;
    if(RP(mp + 0x14, m) && Valid(m)) {
        if(!IsBadWritePtr((void*)(m + 0x30), 12)) memcpy((void*)(m + 0x30), &nextPos, 12);
    }
    if(!IsBadWritePtr((void*)(mp + 0x44), 12)) {
        float* vel = (float*)(mp + 0x44);
        vel[0] = 0; vel[1] = 0; vel[2] = 0;
    }
}

// Noclip REAL (confirmado GTA SA 1.0 - GTAMods Wiki):
// +0x42 (3º byte das physical flags em +0x40) bit 0x01 = "Soft (noclip)"
// → Ped atravessa paredes, chão e objetos do cenário mantendo controle 100% normal.
// Aplica TODO FRAME (jogo reseta em transições como entrar/sair de veículo).
// Nenhuma escrita de posição, velocidade ou matriz. Movimento GTA SA puro.
static void ProcessNoclip() {
    DWORD mp = 0;
    if (!RP(0xB6F5F0, mp) || !Valid(mp)) return;

    DWORD veh = 0;
    bool inVeh = RP(mp + 0x58C, veh) && Valid(veh);

    const BYTE SOFT_NOCLIP = 0x01;   // +0x42 bit 0 = Soft / noclip (atravessa parede)

    if (noclip_on) {
        // === Ped ===
        if (!IsBadWritePtr((void*)(mp + 0x42), 1)) {
            BYTE* flags = (BYTE*)(mp + 0x42);
            *flags |= SOFT_NOCLIP;
        }

        // === Veículo (se estiver dentro) ===
        if (inVeh && !IsBadWritePtr((void*)(veh + 0x42), 1)) {
            BYTE* vflags = (BYTE*)(veh + 0x42);
            *vflags |= SOFT_NOCLIP;
        }
    } else {
        // Restaura colisão normal
        if (!IsBadWritePtr((void*)(mp + 0x42), 1)) {
            BYTE* flags = (BYTE*)(mp + 0x42);
            *flags &= ~SOFT_NOCLIP;
        }

        if (inVeh && !IsBadWritePtr((void*)(veh + 0x42), 1)) {
            BYTE* vflags = (BYTE*)(veh + 0x42);
            *vflags &= ~SOFT_NOCLIP;
        }
    }

    // 100% normal: WASD, pulo, sprint, agachar, animações GTA SA originais.
    // Apenas paredes / mundo perdem colisão.
}
static void ProcessGodMode() {
    if(!godmode_on || !g_haveMe) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    if(!IsBadWritePtr((void*)(mp + 0x540), 4)) *(float*)(mp + 0x540) = 100.0f;
    if(!IsBadWritePtr((void*)(mp + 0x548), 4)) *(float*)(mp + 0x548) = 100.0f;
}
static void ProcessBoost() {
    if(!boost_on || boost_key == 0 || !(GetAsyncKeyState(boost_key) & 0x8000)) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    DWORD veh = 0;
    if(!RP(mp + 0x58C, veh) || !Valid(veh)) return;
    DWORD vm = 0;
    if(!RP(veh + 0x14, vm) || !Valid(vm)) return;
    Vec3 forward, curVel;
    if(!Rd(vm + 0x10, &forward, 12) || !IsValidPos(forward)) return;
    if(!Rd(veh + 0x44, &curVel, 12) || !IsValidPos(curVel)) return;
    float curSpeed = sqrtf(curVel.x*curVel.x + curVel.y*curVel.y + curVel.z*curVel.z);
    float maxSpeed = boost_max / 180.0f;
    if(curSpeed*180.0f >= maxSpeed*180.0f) return;
    float boostPerFrame = (boost_amount / 60.0f) / 180.0f;
    float targetSpeed = curSpeed + boostPerFrame;
    if(targetSpeed > maxSpeed) targetSpeed = maxSpeed;
    float speedDiff = targetSpeed - curSpeed;
    Vec3 nv = {
        curVel.x + (forward.x * speedDiff),
        curVel.y + (forward.y * speedDiff),
        curVel.z + (forward.z * speedDiff * 0.3f)
    };
    if(!IsBadWritePtr((void*)(veh + 0x44), 12)) memcpy((void*)(veh + 0x44), &nv, 12);
}
static void ProcessSlingshot() {
    if(!slingshot_on || slingshot_key == 0 || slingshot_active) return;
    if(!(GetAsyncKeyState(slingshot_key) & 0x8000)) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    DWORD veh = 0;
    if(!RP(mp + 0x58C, veh) || !Valid(veh)) return;
    DWORD vm = 0;
    if(!RP(veh + 0x14, vm) || !Valid(vm)) return;
    WORD modelId = 0;
    if(!RV(veh + 0x22, modelId)) return;
    slingshot_isBike = (modelId >= 462 && modelId <= 586);
    if(!Rd(vm + 0x30, &slingshot_startPos, 12)) return;
    if(!IsValidPos(slingshot_startPos)) return;
    RV(0xB6F258, slingshot_startYaw);
    Vec3 forward;
    if(!Rd(vm + 0x10, &forward, 12) || !IsValidPos(forward)) {
        float yaw = 0, pitch = 0;
        if(!RV(0xB6F258, yaw) || !RV(0xB6F248, pitch)) return;
        forward.x = -cosf(yaw) * cosf(pitch);
        forward.y = -sinf(yaw) * cosf(pitch);
        forward.z = sinf(pitch);
    }
    float len = sqrtf(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if(len < 0.001f) return;
    forward.x /= len; forward.y /= len; forward.z /= len;
    slingshot_forward = forward;
    float maxSpeed = 125.0f / 180.0f;
    Vec3 nv = {
        forward.x * maxSpeed,
        forward.y * maxSpeed,
        forward.z * maxSpeed * 0.05f
    };
    if(!IsBadWritePtr((void*)(veh + 0x44), 12)) memcpy((void*)(veh + 0x44), &nv, 12);
    if(!IsBadWritePtr((void*)(veh + 0x4C), 12)) {
        float* angVel = (float*)(veh + 0x4C);
        angVel[0] = 0; angVel[1] = 0; angVel[2] = 0;
    }
    if(slingshot_isBike) {
        if(!IsBadWritePtr((void*)(veh + 0x5C), 12)) {
            float* rot = (float*)(veh + 0x5C);
            rot[0] = 0; rot[1] = 0; rot[2] = 0;
        }
        if(!IsBadWritePtr((void*)(mp + 0x4C0), 12)) {
            float* vel = (float*)(mp + 0x4C0);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
        if(!IsBadWritePtr((void*)(mp + 0x46), 1)) {
            BYTE flags = *(BYTE*)(mp + 0x46);
            flags |= 0x02;
            *(BYTE*)(mp + 0x46) = flags;
        }
        DWORD pedMatrix = 0;
        if(RP(mp + 0x14, pedMatrix) && Valid(pedMatrix)) {
            Vec3 pedPos = {slingshot_startPos.x, slingshot_startPos.y, slingshot_startPos.z + 0.5f};
            if(!IsBadWritePtr((void*)(pedMatrix + 0x30), 12)) memcpy((void*)(pedMatrix + 0x30), &pedPos, 12);
        }
    }
    slingshot_active = true;
    slingshot_phase = 0;
    slingshot_timer = GetTickCount();
}
static void ProcessSlingshotReturn() {
    if(!slingshot_active) return;
    DWORD now = GetTickCount();
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) { slingshot_active = false; return; }
    DWORD veh = 0;
    if(!RP(mp + 0x58C, veh) || !Valid(veh)) { slingshot_active = false; return; }
    DWORD vm = 0;
    if(!RP(veh + 0x14, vm) || !Valid(vm)) { slingshot_active = false; return; }
    switch(slingshot_phase) {
        case 0: {
            if(now - slingshot_timer < 500) {
                if(slingshot_isBike) {
                    if(!IsBadWritePtr((void*)(mp + 0x4C0), 12)) {
                        float* vel = (float*)(mp + 0x4C0);
                        vel[0] = 0; vel[1] = 0; vel[2] = 0;
                    }
                    if(!IsBadWritePtr((void*)(mp + 0x46), 1)) {
                        BYTE flags = *(BYTE*)(mp + 0x46);
                        flags |= 0x02;
                        *(BYTE*)(mp + 0x46) = flags;
                    }
                    if(!IsBadWritePtr((void*)(veh + 0x5C), 12)) {
                        float* rot = (float*)(veh + 0x5C);
                        rot[0] = 0; rot[1] = 0; rot[2] = 0;
                    }
                }
                if(!IsBadWritePtr((void*)(veh + 0x4C), 12)) {
                    float* angVel = (float*)(veh + 0x4C);
                    angVel[0] = 0; angVel[1] = 0; angVel[2] = 0;
                }
                Vec3 curVel;
                if(Rd(veh + 0x44, &curVel, 12) && IsValidPos(curVel)) {
                    float speed = sqrtf(curVel.x*curVel.x + curVel.y*curVel.y + curVel.z*curVel.z);
                    if(speed < 100.0f / 180.0f) {
                        float maxSpeed = 125.0f / 180.0f;
                        Vec3 nv = {
                            slingshot_forward.x * maxSpeed,
                            slingshot_forward.y * maxSpeed,
                            slingshot_forward.z * maxSpeed * 0.05f
                        };
                        if(!IsBadWritePtr((void*)(veh + 0x44), 12)) memcpy((void*)(veh + 0x44), &nv, 12);
                    }
                }
                return;
            }
            Vec3 curVel;
            if(Rd(veh + 0x44, &curVel, 12) && IsValidPos(curVel)) {
                Vec3 brakeVel = {
                    curVel.x * 0.3f,
                    curVel.y * 0.3f,
                    curVel.z * 0.3f
                };
                if(!IsBadWritePtr((void*)(veh + 0x44), 12)) memcpy((void*)(veh + 0x44), &brakeVel, 12);
            }
            slingshot_phase = 1;
            slingshot_timer = now;
            break;
        }
        case 1: {
            if(now - slingshot_timer < 100) return;
            if(!IsBadWritePtr((void*)(vm + 0x30), 12)) memcpy((void*)(vm + 0x30), &slingshot_startPos, 12);
            Vec3 zeroVel = {0, 0, 0};
            if(!IsBadWritePtr((void*)(veh + 0x44), 12)) memcpy((void*)(veh + 0x44), &zeroVel, 12);
            if(slingshot_isBike) {
                if(!IsBadWritePtr((void*)(mp + 0x4C0), 12)) {
                    float* vel = (float*)(mp + 0x4C0);
                    vel[0] = 0; vel[1] = 0; vel[2] = 0;
                }
                if(!IsBadWritePtr((void*)(mp + 0x46), 1)) {
                    BYTE flags = *(BYTE*)(mp + 0x46);
                    flags |= 0x02;
                    *(BYTE*)(mp + 0x46) = flags;
                }
            }
            if(!IsBadWritePtr((void*)0xB6F258, 4)) *(float*)0xB6F258 = slingshot_startYaw;
            slingshot_active = false;
            slingshot_phase = 0;
            break;
        }
    }
}

static void ProcessInstaBrake() {
    if(!insta_brake_on || insta_brake_key == 0 || !(GetAsyncKeyState(insta_brake_key) & 0x8000)) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    DWORD veh = 0;
    if(!RP(mp + 0x58C, veh) || !Valid(veh)) return;
    DWORD vm = 0;
    if(!RP(veh + 0x14, vm) || !Valid(vm)) return;
    if(!IsBadWritePtr((void*)(veh + 0x44), 12)) {
        float* vel = (float*)(veh + 0x44);
        vel[0] = 0; vel[1] = 0; vel[2] = 0;
    }
    if(!IsBadWritePtr((void*)(veh + 0x4C), 12)) {
        float* angVel = (float*)(veh + 0x4C);
        angVel[0] = 0; angVel[1] = 0; angVel[2] = 0;
    }
    // Also zero player velocity to prevent push
    if(!IsBadWritePtr((void*)(mp + 0x4C0), 12)) {
        float* pvel = (float*)(mp + 0x4C0);
        pvel[0] = 0; pvel[1] = 0; pvel[2] = 0;
    }
}
static void TakeVehicle(DWORD vehPtr) {
    if(!Valid(vehPtr)) return;
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    DWORD vm = 0;
    if(!RP(vehPtr + 0x14, vm) || !Valid(vm)) return;
    Vec3 vp;
    if(!Rd(vm + 0x30, &vp, 12) || !IsValidPos(vp)) return;
    if(!IsBadWritePtr((void*)(vehPtr + 0x4A8), 1)) *(BYTE*)(vehPtr + 0x4A8) = 0;
    if(!IsBadWritePtr((void*)(vehPtr + 0x4A9), 1)) *(BYTE*)(vehPtr + 0x4A9) = 0;
    if(!IsBadWritePtr((void*)(vehPtr + 0x4C8), 4)) *(DWORD*)(vehPtr + 0x4C8) = 0;
    Vec3 tp = {vp.x, vp.y, vp.z + 1.5f};
    DWORD mm = 0;
    if(RP(mp + 0x14, mm) && Valid(mm)) {
        if(!IsBadWritePtr((void*)(mm + 0x30), 12)) memcpy((void*)(mm + 0x30), &tp, 12);
    }
    if(!IsBadWritePtr((void*)(mp + 0x4C0), 12)) {
        float* vel = (float*)(mp + 0x4C0);
        vel[0] = 0; vel[1] = 0; vel[2] = 0;
    }
    if(!IsBadWritePtr((void*)(mp + 0x58C), 4)) *(DWORD*)(mp + 0x58C) = vehPtr;
    if(!IsBadWritePtr((void*)(mp + 0x46), 1)) {
        BYTE flags = *(BYTE*)(mp + 0x46);
        flags |= 0x02;
        flags |= 0x04;
        *(BYTE*)(mp + 0x46) = flags;
    }
    if(!IsBadWritePtr((void*)(mp + 0x4D8), 4)) *(DWORD*)(mp + 0x4D8) = 0;
    if(!IsBadWritePtr((void*)(vehPtr + 0x460), 4)) *(DWORD*)(vehPtr + 0x460) = mp;
    if(!IsBadWritePtr((void*)(vehPtr + 0x4FC), 4)) *(DWORD*)(vehPtr + 0x4FC) = mp;
    WORD modelId = 0;
    if(RV(vehPtr + 0x22, modelId) && modelId >= 462 && modelId <= 586) {
        Vec3 bikeTp = {vp.x, vp.y, vp.z + 0.5f};
        if(RP(mp + 0x14, mm) && Valid(mm)) {
            if(!IsBadWritePtr((void*)(mm + 0x30), 12)) memcpy((void*)(mm + 0x30), &bikeTp, 12);
        }
        if(!IsBadWritePtr((void*)(vehPtr + 0x5C), 12)) {
            float* rot = (float*)(vehPtr + 0x5C);
            rot[0] = 0; rot[1] = 0; rot[2] = 0;
        }
    }
}
static void ProcessTeleportWaypoint() {
    if(!tp_way_on) return;
    DWORD targetBlipHandle = 0;
    if(!RV(0xBA6774, targetBlipHandle)) {
        if(tp_way_active) { tp_way_on = false; tp_way_active = false; }
        return;
    }
    if(targetBlipHandle == 0) {
        if(tp_way_active) { tp_way_on = false; tp_way_active = false; }
        return;
    }
    WORD blipIndex = (WORD)(targetBlipHandle & 0xFFFF);
    if(blipIndex == 0 || blipIndex >= 175) {
        if(tp_way_active) { tp_way_on = false; tp_way_active = false; }
        return;
    }
    DWORD blipAddr = 0xBA86F0 + (DWORD)blipIndex * 0x28;
    float x = 0, y = 0, z = 0;
    if(!RV(blipAddr + 0x8, x) || !RV(blipAddr + 0xC, y) || !RV(blipAddr + 0x10, z)) {
        if(tp_way_active) { tp_way_on = false; tp_way_active = false; }
        return;
    }
    if(!IsFiniteF(x) || !IsFiniteF(y)) {
        if(tp_way_active) { tp_way_on = false; tp_way_active = false; }
        return;
    }
    Vec3 wp = {x, y, 0};
    if(!tp_way_active) { tp_way_target = wp; }
    DWORD mp = 0;
    if(!RP(0xB6F5F0, mp) || !Valid(mp)) return;
    Vec3 myPos;
    if(!PedPos(mp, myPos)) return;
    float dx = tp_way_target.x - myPos.x, dy = tp_way_target.y - myPos.y;
    float dist = sqrtf(dx*dx + dy*dy);
    if(dist < 5.0f) {
        Vec3 finalPos = {myPos.x, myPos.y, myPos.z + 30.0f};
        DWORD m = 0;
        if(RP(mp + 0x14, m) && Valid(m)) {
            if(!IsBadWritePtr((void*)(m + 0x30), 12)) memcpy((void*)(m + 0x30), &finalPos, 12);
        }
        if(!IsBadWritePtr((void*)(mp + 0x44), 12)) {
            float* vel = (float*)(mp + 0x44);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
        tp_way_on = false; tp_way_active = false;
        return;
    }
    DWORD now = GetTickCount();
    if(!tp_way_active || (now - tp_way_timer) > 200) {
        tp_way_active = true; tp_way_timer = now;
        float moveDist = dist > 50.0f ? 50.0f : dist;
        Vec3 nextPos = {myPos.x + (dx / dist)*moveDist, myPos.y + (dy / dist)*moveDist, myPos.z - 30.0f};
        DWORD m = 0;
        if(RP(mp + 0x14, m) && Valid(m)) {
            if(!IsBadWritePtr((void*)(m + 0x30), 12)) memcpy((void*)(m + 0x30), &nextPos, 12);
        }
        if(!IsBadWritePtr((void*)(mp + 0x44), 12)) {
            float* vel = (float*)(mp + 0x44);
            vel[0] = 0; vel[1] = 0; vel[2] = 0;
        }
        DWORD mv = 0;
        RP(mp + 0x58C, mv);
        if(Valid(mv)) {
            DWORD vm = 0;
            if(RP(mv + 0x14, vm) && Valid(vm)) {
                if(!IsBadWritePtr((void*)(vm + 0x30), 12)) memcpy((void*)(vm + 0x30), &nextPos, 12);
            }
            if(!IsBadWritePtr((void*)(mv + 0x44), 12)) {
                float* vel = (float*)(mv + 0x44);
                vel[0] = 0; vel[1] = 0; vel[2] = 0;
            }
        }
    }
}
static void ProcessKeyWaits() {
    if(wait_aim_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { aim_key = 0; wait_aim_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { aim_key = b; wait_aim_key = false; break; }
    }
    if(wait_boost_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { boost_key = 0; wait_boost_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { boost_key = b; wait_boost_key = false; break; }
    }
    if(wait_magnet_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { magnet_key = 0; wait_magnet_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { magnet_key = b; wait_magnet_key = false; break; }
    }
    if(wait_flycar_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { flycar_key = 0; wait_flycar_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { flycar_key = b; wait_flycar_key = false; break; }
    }
    if(wait_proaim_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { proaim_key = 0; wait_proaim_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { proaim_key = b; wait_proaim_key = false; break; }
    }
    if(wait_fly_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { fly_key = 0; wait_fly_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { fly_key = b; wait_fly_key = false; break; }
    }
    if(wait_slingshot_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { slingshot_key = 0; wait_slingshot_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { slingshot_key = b; wait_slingshot_key = false; break; }
    }
    if(wait_insta_brake_key) {
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000) { insta_brake_key = 0; wait_insta_brake_key = false; }
        else for(int b = 1; b < 256; b++) if(GetAsyncKeyState(b) & 0x8000) { insta_brake_key = b; wait_insta_brake_key = false; break; }
    }
}
void InitImGui(LPDIRECT3DDEVICE9 pDevice){
    LoadConfig();
    D3DDEVICE_CREATION_PARAMETERS params;pDevice->GetCreationParameters(&params);gWindow=params.hFocusWindow;
    oWndProc=(WNDPROC)SetWindowLongPtr(gWindow,GWLP_WNDPROC,(LONG_PTR)WndProc);
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGuiIO&io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",14.0f);
    ImGuiStyle&s=ImGui::GetStyle();s.WindowRounding=12;s.FrameRounding=6;s.GrabRounding=6;s.ScrollbarRounding=6;s.ChildRounding=8;s.PopupRounding=6;s.FramePadding=ImVec2(10,6);s.WindowPadding=ImVec2(0,0);s.ItemSpacing=ImVec2(8,4);s.WindowBorderSize=0;s.FrameBorderSize=0;s.ScrollbarSize=8;
    ImVec4 bg=ImVec4(15/255.f,17/255.f,23/255.f,1),card=ImVec4(26/255.f,28/255.f,38/255.f,1),cardH=ImVec4(34/255.f,36/255.f,48/255.f,1),acc=ImVec4(139/255.f,92/255.f,246/255.f,1),txt=ImVec4(230/255.f,230/255.f,240/255.f,1),txtD=ImVec4(140/255.f,140/255.f,160/255.f,1);
    s.Colors[ImGuiCol_Text]=txt;s.Colors[ImGuiCol_TextDisabled]=txtD;s.Colors[ImGuiCol_WindowBg]=bg;s.Colors[ImGuiCol_ChildBg]=bg;s.Colors[ImGuiCol_PopupBg]=card;s.Colors[ImGuiCol_Border]=ImVec4(0,0,0,0);s.Colors[ImGuiCol_FrameBg]=card;s.Colors[ImGuiCol_FrameBgHovered]=cardH;s.Colors[ImGuiCol_FrameBgActive]=cardH;s.Colors[ImGuiCol_TitleBg]=bg;s.Colors[ImGuiCol_TitleBgActive]=bg;s.Colors[ImGuiCol_ScrollbarBg]=bg;s.Colors[ImGuiCol_ScrollbarGrab]=card;s.Colors[ImGuiCol_ScrollbarGrabHovered]=cardH;s.Colors[ImGuiCol_ScrollbarGrabActive]=acc;s.Colors[ImGuiCol_CheckMark]=txt;s.Colors[ImGuiCol_SliderGrab]=acc;s.Colors[ImGuiCol_SliderGrabActive]=acc;s.Colors[ImGuiCol_Button]=card;s.Colors[ImGuiCol_ButtonHovered]=cardH;s.Colors[ImGuiCol_ButtonActive]=acc;s.Colors[ImGuiCol_Header]=card;s.Colors[ImGuiCol_HeaderHovered]=cardH;s.Colors[ImGuiCol_HeaderActive]=acc;s.Colors[ImGuiCol_Separator]=ImVec4(1,1,1,0.06f);
    ImGui_ImplWin32_Init(gWindow);ImGui_ImplDX9_Init(pDevice);
    tex_crosshair=LoadTextureFromMemory(pDevice,crosshair_png,sizeof(crosshair_png));tex_eye=LoadTextureFromMemory(pDevice,eye_png,sizeof(eye_png));tex_localplayer=LoadTextureFromMemory(pDevice,localplayer_png,sizeof(localplayer_png));tex_players=LoadTextureFromMemory(pDevice,players_png,sizeof(players_png));tex_vehicles=LoadTextureFromMemory(pDevice,vehicle_png,sizeof(vehicle_png));tex_keybind=LoadTextureFromMemory(pDevice,keybind_png,sizeof(keybind_png));tex_server=LoadTextureFromMemory(pDevice,server_png,sizeof(server_png));
    void*pSCP=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"),"SetCursorPos"));MH_CreateHook(pSCP,reinterpret_cast<void*>(hkSetCursorPos),(void**)&oSetCursorPos);MH_EnableHook(pSCP);
    void*pCC=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"),"ClipCursor"));MH_CreateHook(pCC,reinterpret_cast<void*>(hkClipCursor),(void**)&oClipCursor);MH_EnableHook(pCC);
    void*pGRI=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"),"GetRawInputData"));MH_CreateHook(pGRI,reinterpret_cast<void*>(hkGetRawInputData),(void**)&oGetRawInputData);MH_EnableHook(pGRI);
    g_sampBase=(DWORD)GetModuleHandleA("samp.dll");LockMouse();gInitialized=true;
}
void UpdateScreenSize(){if(!gWindow)return;RECT rc;if(GetClientRect(gWindow,&rc)){g_screenW=rc.right;g_screenH=rc.bottom;}}

void RenderMenu(LPDIRECT3DDEVICE9 pDevice){
    ProcessKeyWaits();
    ImGui_ImplDX9_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    ImGuiIO&io=ImGui::GetIO();io.MouseDrawCursor=false;
    if(gMenuOpen){LockMouse();while(::ShowCursor(FALSE)>=0);}
    UpdateScreenSize();UpdateCamera();LoadPlayers();UpdateAdminTracking();ScanNearbyVehicles();ProcessSpectate();ProcessAimbot();ProcessBoost();ProcessMagnet();ProcessProAim();ProcessFlyCar();ProcessSlingshot();ProcessSlingshotReturn();ProcessFly();ProcessNoclip();ProcessGodMode();ProcessTeleportSequence();ProcessTeleportWaypoint();ProcessInstaBrake();
    ImDrawList*bg_dl=ImGui::GetBackgroundDrawList();
    if(aim_show_fov&&aim_on)bg_dl->AddCircle(ImVec2(g_crossX*(float)g_screenW,g_crossY*(float)g_screenH),(float)aim_fov_size,Col(aim_fov_col),64,1.5f);
    if(magnet_show_fov&&magnet_on)bg_dl->AddCircle(ImVec2(g_crossX*(float)g_screenW,g_crossY*(float)g_screenH),magnet_fov,Col(magnet_fov_col),64,1.5f);
    if(proaim_show_fov&&proaim_on)bg_dl->AddCircle(ImVec2(g_crossX*(float)g_screenW,g_crossY*(float)g_screenH),(float)proaim_fov,Col(proaim_fov_col),64,1.5f);
    DrawESP(bg_dl);DrawVehicleESP(bg_dl);
    ImDrawList*fg_dl=ImGui::GetForegroundDrawList();
    DrawAdminPopup(fg_dl);
    if(gMenuOpen){
        const float MW=860,MH=540,SW=90,HH=60;
        ImGui::SetNextWindowSize(ImVec2(MW,MH),ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2((g_screenW-MW)*0.5f,(g_screenH-MH)*0.5f),ImGuiCond_FirstUseEver);
        ImGui::Begin("##O",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoCollapse);
        ImVec2 wp=ImGui::GetWindowPos();ImDrawList*dl=ImGui::GetWindowDrawList();
        dl->AddRectFilled(wp,ImVec2(wp.x+MW,wp.y+MH),COL_BG,12);dl->AddRectFilled(wp,ImVec2(wp.x+SW,wp.y+MH),COL_SIDEBAR,12);dl->AddRectFilled(ImVec2(wp.x+20,wp.y),ImVec2(wp.x+SW,wp.y+MH),COL_SIDEBAR);dl->AddRectFilled(ImVec2(wp.x+SW,wp.y),ImVec2(wp.x+MW,wp.y+HH),COL_BG,12);
        const char*title="Mod Menu SAMP R1 0.3.7 by kernel11";ImVec2 ts=ImGui::CalcTextSize(title);dl->AddText(ImVec2(wp.x+SW+(MW-SW-ts.x)/2,wp.y+(HH-ts.y)/2),COL_TEXT,title);
        float cs=18;ImVec2 cmin(wp.x+MW-cs-14,wp.y+14),cmax(cmin.x+cs,cmin.y+cs);ImVec2 mp=ImGui::GetMousePos();
        bool ch=(mp.x>=cmin.x&&mp.x<=cmax.x&&mp.y>=cmin.y&&mp.y<=cmax.y);
        dl->AddLine(ImVec2(cmin.x+3,cmin.y+3),ImVec2(cmax.x-3,cmax.y-3),ch?IM_COL32(255,80,80,255):COL_TEXT_DIM,2);
        dl->AddLine(ImVec2(cmax.x-3,cmin.y+3),ImVec2(cmin.x+3,cmax.y-3),ch?IM_COL32(255,80,80,255):COL_TEXT_DIM,2);
        if(ch&&ImGui::IsMouseClicked(0))gShouldUnload=true;
        const char*tabs[]={"COMBAT","VISUALS","VEHICLES","PLAYER","PLAYERS"};
        IDirect3DTexture9*icons[5]={tex_crosshair,tex_eye,tex_vehicles,tex_localplayer,tex_players};
        float tabH=70,tabsY=wp.y+20;
        if(anim_first){anim_slide_y=tabsY+current_tab*(tabH+4);anim_first=false;}
        anim_slide_target=tabsY+current_tab*(tabH+4);anim_slide_y+=(anim_slide_target-anim_slide_y)*0.20f;if(fabsf(anim_slide_target-anim_slide_y)<0.5f)anim_slide_y=anim_slide_target;
        dl->AddRectFilled(ImVec2(wp.x,anim_slide_y+8),ImVec2(wp.x+3,anim_slide_y+tabH-8),COL_ACCENT);dl->AddRectFilled(ImVec2(wp.x,anim_slide_y),ImVec2(wp.x+SW,anim_slide_y+tabH),IM_COL32(139,92,246,25));
        for(int i=0;i<5;i++){
            float ty=tabsY+i*(tabH+4);ImGui::PushID(i);ImGui::SetCursorScreenPos(ImVec2(wp.x,ty));ImGui::InvisibleButton("##sb",ImVec2(SW,tabH));if(ImGui::IsItemClicked(0))current_tab=i;bool th=ImGui::IsItemHovered();ImGui::PopID();
            float is=(i==2||i==3||i==4)?34:26;float ix=wp.x+(SW-is)/2,iy=ty+((i==2||i==3||i==4)?10:14);
            if(icons[i])dl->AddImage((ImTextureID)icons[i],ImVec2(ix,iy),ImVec2(ix+is,iy+is),ImVec2(0,0),ImVec2(1,1),(current_tab==i)?COL_ACCENT:(th?COL_TEXT:COL_TEXT_DIM));
            ImVec2 tts=ImGui::CalcTextSize(tabs[i]);dl->AddText(ImVec2(wp.x+(SW-tts.x)/2,ty+((i==2||i==3||i==4)?48:44)),(current_tab==i)?COL_TEXT:COL_TEXT_DIM,tabs[i]);
        }
        ImGui::SetCursorScreenPos(ImVec2(wp.x+SW+16,wp.y+HH+10));
        float cW=MW-SW-32,colW=(cW-12)*0.5f;
        ImGui::BeginChild("##ct",ImVec2(cW,MH-HH-20),false,0);
        switch(current_tab){
        case 0:{
            // COMBAT - three independent side-by-side cards using manual cursor positioning
            // (expansion of one does not push toggles of others; top-aligned like VISUALS)
            ImGui::BeginChild("##combat_cards", ImVec2(cW, MH-HH-20), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            ImGuiStorage* st = ImGui::GetStateStorage();
            ImVec2 startScreenPos = ImGui::GetCursorScreenPos();
            float gap = 8.0f;
            float cardW = (cW - 2.0f * gap) / 3.0f;

            // === LEFT: AIMBOT ===
            ImGui::SetCursorScreenPos(startScreenPos);
            ImGui::BeginChild("##aim_card", ImVec2(cardW, 0), false);
            ToggleCard("Enable Aimbot", &aim_on, "Enables aimbot");
            ImGuiID aID = ImGui::GetID("aa");
            float aA = st->GetFloat(aID, aim_on ? 1.0f : 0.0f), aT = aim_on ? 1.0f : 0.0f;
            aA += (aT - aA) * 0.20f; if (fabsf(aT - aA) < 0.01f) aA = aT; st->SetFloat(aID, aA);
            if (aA > 0.02f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, aA);
                SliderIntCard("FOV Radius", &aim_fov_size, 10, 500, "FOV size in pixels");
                SliderIntCard("Smoothness", &aim_smooth, 1, 20, "1 = instant (more obvious)");
                const char* targets[] = {"Head", "Neck", "Chest"};
                ComboCard("Target", &aim_target, targets, 3);
                const char* priorities[] = {"Closest", "Lowest HP"};
                ComboCard("Priority", &aim_priority, priorities, 2);
                if (KeybindCard("kb", "Aimbot Key", aim_key, wait_aim_key)) wait_aim_key = true;
                ToggleColorCard("Show FOV", &aim_show_fov, aim_fov_col);
                ToggleCard("Visibility Check", &aim_visicheck, "Only aims at visible targets");
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            // === MIDDLE: MAGNET ===
            ImVec2 magStart = ImVec2(startScreenPos.x + cardW + gap, startScreenPos.y);
            ImGui::SetCursorScreenPos(magStart);
            ImGui::BeginChild("##mag_card", ImVec2(cardW, 0), false);
            ToggleCard("Magnet", &magnet_on);
            ImGuiID mtID = ImGui::GetID("mta");
            float mtA = st->GetFloat(mtID, magnet_on ? 1.0f : 0.0f), mtTt = magnet_on ? 1.0f : 0.0f;
            mtA += (mtTt - mtA) * 0.20f; if (fabsf(mtTt - mtA) < 0.01f) mtA = mtTt; st->SetFloat(mtID, mtA);
            if (mtA > 0.02f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, mtA);
                ToggleCard("Pull ALL (no priority)", &magnet_pull_all, "Pulls every player in range (ignores priority)");
                SliderCard("Magnet FOV", &magnet_fov, 50.0f, 800.0f, "%.0f");
                SliderCard("Distance", &magnet_distance, 1.0f, 15.0f, "%.1fm");
                const char* magT2[] = {"Head", "Neck", "Chest"};
                ComboCard("Magnet Target", &magnet_target, magT2, 3);
                if (!magnet_pull_all) {
                    const char* priorities[] = {"Closest", "Lowest HP"};
                    ComboCard("Priority", &magnet_priority, priorities, 2);
                }
                if (KeybindCard("mtk", "Magnet Key", magnet_key, wait_magnet_key)) wait_magnet_key = true;
                ToggleColorCard("Show FOV##M", &magnet_show_fov, magnet_fov_col);
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            // === RIGHT: PRO AIM ===
            ImVec2 proStart = ImVec2(magStart.x + cardW + gap, startScreenPos.y);
            ImGui::SetCursorScreenPos(proStart);
            ImGui::BeginChild("##pro_card", ImVec2(cardW, 0), false);
            ToggleCard("Pro Aim", &proaim_on, "Moves crosshair only - camera stays free");
            ImGuiID mID = ImGui::GetID("ma");
            float mA = st->GetFloat(mID, proaim_on ? 1.0f : 0.0f), mTt = proaim_on ? 1.0f : 0.0f;
            mA += (mTt - mA) * 0.20f; if (fabsf(mTt - mA) < 0.01f) mA = mTt; st->SetFloat(mID, mA);
            if (mA > 0.02f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, mA);
                SliderIntCard("Pro Aim FOV", &proaim_fov, 50, 800, "FOV in pixels");
                SliderCard("Stickiness", &proaim_stickiness, 0.5f, 0.99f, "%.2f", "How fast crosshair snaps to target");
                const char* magTargets[] = {"Head", "Neck", "Chest"};
                ComboCard("Target", &proaim_target, magTargets, 3);
                const char* priorities[] = {"Closest", "Lowest HP"};
                ComboCard("Priority", &proaim_priority, priorities, 2);
                if (KeybindCard("mk", "Pro Aim Key", proaim_key, wait_proaim_key)) wait_proaim_key = true;
                ToggleColorCard("Show FOV##P", &proaim_show_fov, proaim_fov_col);
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::EndChild(); // ##combat_cards
        }break;
        case 1:{
            ImGui::BeginChild("##vscroll",ImVec2(cW,MH-HH-20),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);

            float leftW = colW + 6;
            float rightW = cW - leftW - 12;

            // Record the exact starting screen position
            ImVec2 startScreenPos = ImGui::GetCursorScreenPos();

            // === LEFT COLUMN: PLAYER ESP ===
            ImGui::SetCursorScreenPos(startScreenPos);
            ImGui::BeginChild("##esp_left", ImVec2(leftW, 0), false);
            ImDrawList* dP = ImGui::GetWindowDrawList();
            dP->AddText(ImGui::GetCursorScreenPos(), COL_TEXT_DIM, "PLAYER ESP");
            ImGui::Dummy(ImVec2(0, 18));

            ToggleCard("Player ESP", &esp_p);
            ImGuiID eID = ImGui::GetID("ea");
            ImGuiStorage* st = ImGui::GetStateStorage();
            float eA = st->GetFloat(eID, esp_p ? 1.0f : 0.0f), eT = esp_p ? 1.0f : 0.0f;
            eA += (eT - eA) * 0.20f; if (fabsf(eT - eA) < 0.01f) eA = eT; st->SetFloat(eID, eA);

            if (eA > 0.02f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eA);
                ToggleColorCard("Box", &esp_p_box, esp_p_box_col);
                ToggleColorCard("Name", &esp_p_name, esp_p_name_col);
                ToggleColorCard("Health", &esp_p_hp, esp_p_hp_col);
                ToggleColorCard("Distance", &esp_p_dist, esp_p_dist_col);
                ToggleColorCard("Weapon", &esp_p_weapon, esp_p_weapon_col);
                ToggleColorCard("Lines", &esp_p_lines, esp_p_lines_col);
                ToggleColorCard("Skeleton", &esp_p_skeleton, esp_p_skel_col);
                ImGui::Dummy(ImVec2(0, 10));
                dP->AddText(ImGui::GetCursorScreenPos(), COL_TEXT_DIM, "CONFIG");
                ImGui::Dummy(ImVec2(0, 18));
                ToggleCard("Filled Box", &esp_p_box_fill);
                ToggleCard("Dynamic Color (HP)", &esp_p_hp_dynamic);
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            // === RIGHT COLUMN: ADMIN ESP (always starts at the exact same Y) ===
            ImVec2 rightStart = ImVec2(startScreenPos.x + leftW + 12, startScreenPos.y);
            ImGui::SetCursorScreenPos(rightStart);
            ImGui::BeginChild("##esp_right", ImVec2(rightW, 0), false);
            ImDrawList* dA = ImGui::GetWindowDrawList();
            dA->AddText(ImGui::GetCursorScreenPos(), COL_TEXT_DIM, "ADMIN ESP");
            ImGui::Dummy(ImVec2(0, 18));

            ToggleCard("Admin ESP", &esp_admin);
            ImGuiID adID = ImGui::GetID("ada");
            float adA = st->GetFloat(adID, esp_admin ? 1.0f : 0.0f), adT = esp_admin ? 1.0f : 0.0f;
            adA += (adT - adA) * 0.20f; if (fabsf(adT - adA) < 0.01f) adA = adT; st->SetFloat(adID, adA);

            if (adA > 0.02f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, adA);
                ToggleCard("Box##ADM", &esp_admin_box);
                ToggleCard("Name##ADM", &esp_admin_name);
                ToggleCard("Distance##ADM", &esp_admin_dist);
                ToggleCard("Health##ADM", &esp_admin_hp);
                ToggleCard("Lines##ADM", &esp_admin_lines);
                ToggleCard("Skeleton##ADM", &esp_admin_skeleton);
                ImGui::Dummy(ImVec2(0, 10));
                dA->AddText(ImGui::GetCursorScreenPos(), COL_TEXT_DIM, "CONFIG");
                ImGui::Dummy(ImVec2(0, 18));
                ToggleCard("Filled Box##ADM", &esp_admin_box_fill);
                ToggleCard("Side Alert", &esp_admin_popup);
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::EndChild(); // end vscroll
        }break;
        case 2:{
            ImDrawList*dtp=ImGui::GetWindowDrawList();
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"SPEED");ImGui::Dummy(ImVec2(0,14));
            ToggleCard("Speed Boost",&boost_on);
            ImGuiID bID=ImGui::GetID("ba");ImGuiStorage*st=ImGui::GetStateStorage();float bA=st->GetFloat(bID,boost_on?1.0f:0.0f),bT=boost_on?1.0f:0.0f;bA+=(bT-bA)*0.20f;if(fabsf(bT-bA)<0.01f)bA=bT;st->SetFloat(bID,bA);
            if(bA>0.02f){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,bA);if(KeybindCard("bk","Boost Key",boost_key,wait_boost_key))wait_boost_key=true;SliderCard("Boost/s (km/h)",&boost_amount,5.0f,5000.0f,"%.0f");SliderCard("Max (km/h)",&boost_max,50.0f,5000.0f,"%.0f");ImGui::PopStyleVar();}
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"FLY CAR");ImGui::Dummy(ImVec2(0,14));
            ToggleCard("Fly Car",&flycar_on,"Fly with the car you're driving");
            ImGuiID fcID=ImGui::GetID("fca");float fcA=st->GetFloat(fcID,flycar_on?1.0f:0.0f),fcT=flycar_on?1.0f:0.0f;fcA+=(fcT-fcA)*0.20f;if(fabsf(fcT-fcA)<0.01f)fcA=fcT;st->SetFloat(fcID,fcA);
            if(fcA>0.02f){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,fcA);if(KeybindCard("fck","Fly Car Key (hold)",flycar_key,wait_flycar_key,"Hold to fly in camera direction"))wait_flycar_key=true;SliderCard("Fly Speed",&flycar_speed,1.0f,50.0f,"%.1f","Flying speed");ImGui::PopStyleVar();}
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"SLING SHOT");
            ImGui::Dummy(ImVec2(0,14));
            ToggleCard("Sling Shot",&slingshot_on,"Launches vehicle forward and returns");
            ImGuiID ssID=ImGui::GetID("ssa");
            ImGuiStorage*ss_st=ImGui::GetStateStorage();
            float ssA=ss_st->GetFloat(ssID,slingshot_on?1.0f:0.0f);
            float ssT=slingshot_on?1.0f:0.0f;
            ssA+=(ssT-ssA)*0.20f;
            if(fabsf(ssT-ssA)<0.01f) ssA=ssT;
            ss_st->SetFloat(ssID,ssA);
            if(ssA>0.02f){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ssA);if(KeybindCard("ssk","Sling Shot Key",slingshot_key,wait_slingshot_key,"Press to launch vehicle forward")){wait_slingshot_key=true;}SliderCard("Speed (km/h)",&slingshot_speed,10.0f,300.0f,"%.0f");ImGui::PopStyleVar();}
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"INSTA BRAKE");
            ImGui::Dummy(ImVec2(0,14));
            ToggleCard("Insta Brake",&insta_brake_on,"Stops the vehicle instantly when key pressed");
            ImGuiID ibID=ImGui::GetID("iba");
            float ibA=st->GetFloat(ibID,insta_brake_on?1.0f:0.0f),ibT=insta_brake_on?1.0f:0.0f;ibA+=(ibT-ibA)*0.20f;if(fabsf(ibT-ibA)<0.01f)ibA=ibT;st->SetFloat(ibID,ibA);
            if(ibA>0.02f){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ibA);if(KeybindCard("ibk","Insta Brake Key",insta_brake_key,wait_insta_brake_key,"Press to instantly brake the vehicle"))wait_insta_brake_key=true;ImGui::PopStyleVar();}
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"VEHICLE ESP");ImGui::Dummy(ImVec2(0,14));
            ToggleCard("Vehicle ESP",&esp_v);
            ImGuiID evID=ImGui::GetID("eva");float evA=st->GetFloat(evID,esp_v?1.0f:0.0f),evT=esp_v?1.0f:0.0f;evA+=(evT-evA)*0.20f;if(fabsf(evT-evA)<0.01f)evA=evT;st->SetFloat(evID,evA);
            if(evA>0.02f){ImGui::PushStyleVar(ImGuiStyleVar_Alpha,evA);ToggleColorCard("Box##V",&esp_v_box,esp_v_box_col);ToggleColorCard("Name##V",&esp_v_name,esp_v_name_col);ToggleColorCard("Distance##V",&esp_v_dist,esp_v_dist_col);ToggleColorCard("Driver##V",&esp_v_driver,esp_v_driver_col);ToggleCard("Filled Box##V",&esp_v_box_fill);SliderCard("Max Dist##V",&esp_v_max_dist,10.0f,1000.0f,"%.0fm");ImGui::PopStyleVar();}
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"ACTIONS");ImGui::Dummy(ImVec2(0,14));
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(139/255.f,92/255.f,246/255.f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(159/255.f,112/255.f,255/255.f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(120/255.f,72/255.f,226/255.f,1));
            if(ImGui::Button("REPAIR VEHICLE",ImVec2(-1,32)))RepairVehicle();ImGui::Dummy(ImVec2(0,2));if(ImGui::Button("EXPLODE VEHICLE",ImVec2(-1,32)))ExplodeMyVehicle();ImGui::PopStyleColor(3);
            ImGui::Dummy(ImVec2(0,6));
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"NEARBY VEHICLES");ImGui::Dummy(ImVec2(0,14));
            float avV=ImGui::GetContentRegionAvail().x;
            ImGui::BeginChild("##vl",ImVec2(avV,150),false);
            for(size_t i=0;i<g_nearbyVehicles.size();i++){
                const auto&nv=g_nearbyVehicles[i];ImDrawList*vdl=ImGui::GetWindowDrawList();ImVec2 rp=ImGui::GetCursorScreenPos();float rW=ImGui::GetContentRegionAvail().x,rH=42;
                ImGui::PushID((int)i);ImGui::InvisibleButton("##vr",ImVec2(rW,rH));bool hv=ImGui::IsItemHovered(),ck=ImGui::IsItemClicked(0);ImGui::PopID();
                vdl->AddRectFilled(rp,ImVec2(rp.x+rW,rp.y+rH),hv?COL_CARD_HOVER:COL_CARD,6);
                vdl->AddCircleFilled(ImVec2(rp.x+20,rp.y+rH/2),12,nv.hasDriver?IM_COL32(240,100,100,255):COL_ACCENT);
                char ib[8];snprintf(ib,sizeof(ib),"%d",nv.modelId);ImVec2 isz=ImGui::CalcTextSize(ib);vdl->AddText(ImVec2(rp.x+20-isz.x/2,rp.y+rH/2-isz.y/2),IM_COL32(255,255,255,255),ib);
                vdl->AddText(ImVec2(rp.x+40,rp.y+5),COL_TEXT,GetVehicleName(nv.modelId));
                char inf[64];if(nv.hasDriver){snprintf(inf,sizeof(inf),"%.0fm | %s",nv.dist,nv.driverName);vdl->AddText(ImVec2(rp.x+40,rp.y+22),IM_COL32(240,100,100,255),inf);}else{snprintf(inf,sizeof(inf),"%.0fm | Empty",nv.dist);vdl->AddText(ImVec2(rp.x+40,rp.y+22),COL_TEXT_DIM,inf);}
                ImVec2 mpos=ImGui::GetMousePos();
                float bX=rp.x+rW-58,bY=rp.y+9;
                bool bh=(mpos.x>=bX&&mpos.x<=bX+52&&mpos.y>=bY&&mpos.y<=bY+24);
                vdl->AddRectFilled(ImVec2(bX,bY),ImVec2(bX+52,bY+24),bh?COL_ACCENT_HOV:COL_ACCENT,4);
                ImVec2 bts=ImGui::CalcTextSize("TAKE");
                vdl->AddText(ImVec2(bX+(52-bts.x)/2,bY+(24-bts.y)/2),IM_COL32(255,255,255,255),"TAKE");
                if(bh&&ImGui::IsMouseClicked(0)){TakeVehicle(nv.ptr);}
                float tpX=rp.x+rW-116,tpY=rp.y+9;
                bool tph=(mpos.x>=tpX&&mpos.x<=tpX+52&&mpos.y>=tpY&&mpos.y<=tpY+24);
                vdl->AddRectFilled(ImVec2(tpX,tpY),ImVec2(tpX+52,tpY+24),tph?COL_ACCENT_HOV:COL_ACCENT,4);
                ImVec2 tpts=ImGui::CalcTextSize("TP ME");
                vdl->AddText(ImVec2(tpX+(52-tpts.x)/2,tpY+(24-tpts.y)/2),IM_COL32(255,255,255,255),"TP ME");
                if(tph&&ImGui::IsMouseClicked(0)){
                    DWORD mp=0;
                    if(RP(0xB6F5F0,mp)&&Valid(mp)){
                        Vec3 myPos;
                        if(PedPos(mp,myPos)){
                            float yaw=0;
                            RV(0xB6F258,yaw);
                            Vec3 tpPos={myPos.x-cosf(yaw)*3.0f,myPos.y-sinf(yaw)*3.0f,myPos.z+0.5f};
                            DWORD vm=0;
                            if(RP(nv.ptr+0x14,vm)&&Valid(vm)){
                                if(!IsBadWritePtr((void*)(vm+0x30),12)){memcpy((void*)(vm+0x30),&tpPos,12);}
                                if(!IsBadWritePtr((void*)(nv.ptr+0x44),12)){float*vel=(float*)(nv.ptr+0x44);vel[0]=0;vel[1]=0;vel[2]=0;}
                            }
                        }
                    }
                }
                ImGui::Dummy(ImVec2(0,2));
            }
            if(g_nearbyVehicles.empty()){ImGui::Dummy(ImVec2(0,15));const char*m="No nearby vehicles";ImVec2 ms=ImGui::CalcTextSize(m);ImGui::SetCursorPosX((avV-ms.x)/2);ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1),"%s",m);}
            ImGui::EndChild();
        }break;
        case 3:{
            ImGui::Columns(2,"##lp",false);
            ImGui::SetColumnWidth(0,colW+6);
            ImDrawList*dtp=ImGui::GetWindowDrawList();
            dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"ACTIONS");
            ImGui::Dummy(ImVec2(0,18));
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(139/255.f,92/255.f,246/255.f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(159/255.f,112/255.f,255/255.f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(120/255.f,72/255.f,226/255.f,1));
            if(ImGui::Button("FULL HEALTH",ImVec2(-1,36)))GiveFullHealth();
            ImGui::Dummy(ImVec2(0,4));
            if(ImGui::Button("FULL ARMOR",ImVec2(-1,36)))GiveFullArmor();
            ImGui::PopStyleColor(3);
            ImGui::Dummy(ImVec2(0,4));
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(139/255.f,92/255.f,246/255.f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(159/255.f,112/255.f,255/255.f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(120/255.f,72/255.f,226/255.f,1));
            if(ImGui::Button(tp_way_on?"STOP TP WAY":"TP WAY",ImVec2(-1,36))){tp_way_on=!tp_way_on;}
            ImGui::PopStyleColor(3);
            if(tp_way_on){ImGui::TextColored(ImVec4(0.3f,1,0.3f,1),"En route...");}
            else{ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1),"Stopped");}
            ImGui::Dummy(ImVec2(0,4));
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1),"Mark a waypoint on the map");
            
            ImGui::Dummy(ImVec2(0,17));
            ToggleCard("God Mode",&godmode_on,"Locks HP at 100 - instant kill protection");
            ToggleCard("Fly",&fly_on,"Fly in camera direction");
            if(fly_on){
                if(KeybindCard("fk","Fly Key",fly_key,wait_fly_key))wait_fly_key=true;
                SliderCard("Fly Speed",&fly_speed,0.5f,10.0f,"%.1f");
            }
            ToggleCard("Noclip", &noclip_on, "Atravessa paredes (no clip) - personagem anda normalmente");
            if (noclip_on) {
                // Movimento 100% normal (WASD, pulo, corrida). Apenas paredes perdem colisão.
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.7f, 1.0f), "Movimento normal do GTA (sem override)");
                ImGui::Dummy(ImVec2(0, 2));
            }
            ToggleCard("Local Player ESP",&esp_l);
            ImGuiID lID=ImGui::GetID("la");
            ImGuiStorage*st=ImGui::GetStateStorage();
            float lA=st->GetFloat(lID,esp_l?1.0f:0.0f);
            float lT=esp_l?1.0f:0.0f;
            lA+=(lT-lA)*0.20f;
            if(fabsf(lT-lA)<0.01f)lA=lT;
            st->SetFloat(lID,lA);
            if(lA>0.02f){
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha,lA);
                ToggleColorCard("Box",&esp_l_box,esp_l_box_col);
                ToggleColorCard("Name",&esp_l_name,esp_l_name_col);
                ToggleColorCard("Health",&esp_l_hp,esp_l_hp_col);
                ToggleColorCard("Distance",&esp_l_dist,esp_l_dist_col);
                ToggleColorCard("Skeleton",&esp_l_skeleton,esp_l_skel_col);
                ImGui::Dummy(ImVec2(0,10));
                dtp->AddText(ImGui::GetCursorScreenPos(),COL_TEXT_DIM,"CONFIG");
                ImGui::Dummy(ImVec2(0,18));
                ToggleCard("Filled Box##L",&esp_l_box_fill);
                ToggleCard("Dynamic Color##L",&esp_l_hp_dynamic);
                ImGui::PopStyleVar();
            }
			

            ImGui::Columns(1);
        }break;
        case 4:{
            ImDrawList*dl2=ImGui::GetWindowDrawList();ImVec2 rp0=ImGui::GetCursorScreenPos();float rW=ImGui::GetContentRegionAvail().x;
            float listW=rW-200;
            ImGui::BeginChild("##pl",ImVec2(listW,0),true);
            for(int i=0;i<(int)g_sampList.size();i++){
                const auto&p=g_sampList[i];
                const Player*mm=nullptr;for(const auto&gp:g_players)if(gp.id==p.id){mm=&gp;break;}
                ImDrawList*dlp=ImGui::GetWindowDrawList();ImVec2 rp=ImGui::GetCursorScreenPos();float rowW=ImGui::GetContentRegionAvail().x,rowH=28;
                ImGui::PushID(p.id);ImGui::InvisibleButton("##r",ImVec2(rowW,rowH));bool hv=ImGui::IsItemHovered();if(ImGui::IsItemClicked(0))g_selPlayerId=p.id;ImGui::PopID();
                if(hv)dlp->AddRectFilled(rp,ImVec2(rp.x+rowW,rp.y+rowH),IM_COL32(139,92,246,60),4);
                if(g_selPlayerId==p.id)dlp->AddRectFilled(rp,ImVec2(rp.x+3,rp.y+rowH),COL_ACCENT,2);
                dlp->AddText(ImVec2(rp.x+8,rp.y+6),COL_TEXT,p.name);
                if(mm&&Valid(mm->ped)){
                    char dt[32];snprintf(dt,sizeof(dt),"%.0fm",mm->dist);
                    ImVec2 ts=ImGui::CalcTextSize(dt);
                    dlp->AddText(ImVec2(rp.x+rowW-ts.x-40,rp.y+6),COL_TEXT_DIM,dt);
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##pi",ImVec2(200,0),true);
            if(g_selPlayerId>=0){
                const SampPlayer*selP=nullptr;for(const auto&sp:g_sampList)if(sp.id==g_selPlayerId){selP=&sp;break;}
                if(selP){
                    const auto&p=*selP;const Player*m=nullptr;for(const auto&gp:g_players)if(gp.id==p.id){m=&gp;break;}
                    float pW2=ImGui::GetContentRegionAvail().x;
                    ImGui::Dummy(ImVec2(0,10));char ihdr[64];snprintf(ihdr,sizeof(ihdr),"ID %d",p.id);ImVec2 ihs=ImGui::CalcTextSize(ihdr);ImGui::SetCursorPosX((pW2-ihs.x)/2);ImGui::TextColored(ImVec4(139/255.f,92/255.f,246/255.f,1),"%s",ihdr);
                    ImVec2 ns=ImGui::CalcTextSize(p.name);ImGui::SetCursorPosX((pW2-ns.x)/2);ImGui::Text("%s",p.name);
                    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
                    auto sL=[&](const char*l,const char*v,ImU32 c){ImDrawList*d=ImGui::GetWindowDrawList();ImVec2 sp=ImGui::GetCursorScreenPos();d->AddText(ImVec2(sp.x,sp.y),COL_TEXT_DIM,l);ImVec2 vs=ImGui::CalcTextSize(v);d->AddText(ImVec2(sp.x+pW2-vs.x-8,sp.y),c,v);ImGui::Dummy(ImVec2(0,18));};
                    char buf[64];
                    if(m&&Valid(m->ped)){snprintf(buf,sizeof(buf),"%.0fm",m->dist);sL("Distance",buf,COL_TEXT);sL("Weapon",GetWeaponName(m->weaponId),COL_TEXT);}
                    else{sL("Distance","---",COL_TEXT_DIM);sL("Weapon","---",COL_TEXT_DIM);}
                    ImGui::Spacing();float bW=pW2-16;
                    bool cTP=(m&&Valid(m->ped));
                    bool iS=(g_spectatingId==p.id);ImGui::PushStyleColor(ImGuiCol_Button,iS?ImVec4(0.6f,0.1f,0.1f,1):ImVec4(139/255.f,92/255.f,246/255.f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,iS?ImVec4(0.8f,0.15f,0.15f,1):ImVec4(159/255.f,112/255.f,255/255.f,1));
                    if(ImGui::Button(iS?"Stop Spectate":"Spectate",ImVec2(bW,30))){if(iS)StopSpectate();else SendSpectate(p.id);}ImGui::PopStyleColor(2);ImGui::Spacing();
                    if(!cTP)ImGui::PushStyleVar(ImGuiStyleVar_Alpha,0.4f);
                    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(139/255.f,92/255.f,246/255.f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(159/255.f,112/255.f,255/255.f,1));
                    if(ImGui::Button("Teleport",ImVec2(bW,30))&&cTP)TeleportToPlayer(p.id);ImGui::PopStyleColor(2);if(!cTP)ImGui::PopStyleVar();
                }
            }else{
                ImGui::Dummy(ImVec2(0,80));const char*msg="Select a player";ImVec2 ms=ImGui::CalcTextSize(msg);float pW3=ImGui::GetContentRegionAvail().x;ImGui::SetCursorPosX((pW3-ms.x)/2);ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1),"%s",msg);
            }
            ImGui::EndChild();
        }break;
        }
        ImGui::EndChild();
        ImGui::End();
    }
    if(gMenuOpen){
        ImVec2 mp=io.MousePos;ImDrawList*fg=ImGui::GetForegroundDrawList();
        ImVec2 points[7]={ImVec2(mp.x,mp.y),ImVec2(mp.x,mp.y+20),ImVec2(mp.x+4.5f,mp.y+16),ImVec2(mp.x+8,mp.y+24),ImVec2(mp.x+11,mp.y+23),ImVec2(mp.x+7.5f,mp.y+15),ImVec2(mp.x+13,mp.y+13)};
        ImVec2 shadow[7];for(int i=0;i<7;i++)shadow[i]=ImVec2(points[i].x+1,points[i].y+1);
        fg->AddConvexPolyFilled(shadow,7,IM_COL32(0,0,0,120));fg->AddConvexPolyFilled(points,7,IM_COL32(255,255,255,255));
        for(int i=0;i<7;i++)fg->AddLine(points[i],points[(i+1)%7],IM_COL32(0,0,0,220),1.2f);
        fg->AddCircleFilled(ImVec2(mp.x+1,mp.y+1),2.0f,COL_ACCENT);
    }
    ImGui::EndFrame();ImGui::Render();SetLinearFilter(pDevice);ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
HRESULT WINAPI hkPresent(LPDIRECT3DDEVICE9 pD,const RECT*s,const RECT*d,HWND h,const RGNDATA*r){if(gShutdownComplete)return oPresent(pD,s,d,h,r);if(!gInitialized)InitImGui(pD);if(gShouldUnload&&gInitialized){SaveConfig();if(g_isSpectating)StopSpectate();LockMouse();ReleaseTextures();SetWindowLongPtr(gWindow,GWLP_WNDPROC,(LONG_PTR)oWndProc);ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();gInitialized=false;gShutdownComplete=true;return oPresent(pD,s,d,h,r);}RenderMenu(pD);return oPresent(pD,s,d,h,r);}
HRESULT WINAPI hkEndScene(LPDIRECT3DDEVICE9 p){return oEndScene(p);}
HRESULT WINAPI hkReset(LPDIRECT3DDEVICE9 p,D3DPRESENT_PARAMETERS*pp){if(gInitialized&&!gShutdownComplete){ReleaseTextures();ImGui_ImplDX9_InvalidateDeviceObjects();}HRESULT hr=oReset(p,pp);if(gInitialized&&!gShutdownComplete){ImGui_ImplDX9_CreateDeviceObjects();tex_crosshair=LoadTextureFromMemory(p,crosshair_png,sizeof(crosshair_png));tex_eye=LoadTextureFromMemory(p,eye_png,sizeof(eye_png));tex_localplayer=LoadTextureFromMemory(p,localplayer_png,sizeof(localplayer_png));tex_players=LoadTextureFromMemory(p,players_png,sizeof(players_png));tex_vehicles=LoadTextureFromMemory(p,vehicle_png,sizeof(vehicle_png));tex_keybind=LoadTextureFromMemory(p,keybind_png,sizeof(keybind_png));tex_server=LoadTextureFromMemory(p,server_png,sizeof(server_png));}return hr;}
DWORD WINAPI MainThread(LPVOID){
    while(!FindWindowA("Grand theft auto San Andreas",nullptr))Sleep(200);
    Sleep(5000);
    void**vt=(void**)GetD3D9DeviceVTable();
    if(!vt){FreeLibraryAndExitThread(gModule,0);return 0;}
    if(MH_Initialize()!=MH_OK){FreeLibraryAndExitThread(gModule,0);return 0;}
    MH_CreateHook(vt[17],reinterpret_cast<void*>(hkPresent),(void**)&oPresent);
    MH_EnableHook(vt[17]);
    MH_CreateHook(vt[42],reinterpret_cast<void*>(hkEndScene),(void**)&oEndScene);
    MH_EnableHook(vt[42]);
    MH_CreateHook(vt[16],reinterpret_cast<void*>(hkReset),(void**)&oReset);
    MH_EnableHook(vt[16]);
    Sleep(3000);
    while(!gShutdownComplete){
        if(GetAsyncKeyState(VK_END)&1)gShouldUnload=true;
        Sleep(100);
    }
    Sleep(500);
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    FreeLibraryAndExitThread(gModule,0);
    return 0;
}
BOOL APIENTRY DllMain(HMODULE hModule,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){gModule=hModule;DisableThreadLibraryCalls(hModule);CreateThread(nullptr,0,MainThread,nullptr,0,nullptr);}return TRUE;}
 
