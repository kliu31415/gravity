#include "sdl_base.h"
#include <cstdlib>
#include <cmath>
#include <vector>
#include <iostream>
#include <utility>
#include <deque>
#include <fstream>
#include <iomanip>
#include <cstring>
#define A first
#define B second
#define mp make_pair
using namespace std;
const int X_CUTOFF = 1e4, Y_CUTOFF = 1e4; //objects that get super far away don't need to be considered any more
double zoom = 1, xpos = 0, ypos = 0, speed = 0.02;
double G = 6.674e-11;
bool showMass = false, paused = false, showTrailGlobal = false, absorbOnCollision = false, cameraFixed = true;
int sel = -1; //which object is selected, if any
const double PANSPEED = 20;
int ITER = 2;
int frameCount = 0, iterCount = 0;
int trailLength = 16, trailInterval = 16;
struct object
{
    deque<pair<int, int> > prv;
    double x, y, r, mass, dX, dY;
    bool showTrail = true;
    SDL_Color col;
    SDL_Texture *t;
    int tcount;
    void genTexture()
    {
        t = SDL_CreateTexture(getRenderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, r*2, r*2);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(getRenderer(), t);
        fillCircle(r, r, r, col.r, col.g, col.b);
        SDL_SetRenderTarget(getRenderer(), NULL);
    }
    object(double _x, double _y, double _r, double _mass, double _dX = 0, double _dY = 0, SDL_Color co = {255, 0, 0, 255})
    {
        x = _x;
        y = _y;
        r = _r;
        mass = _mass;
        dX = _dX;
        dY = _dY;
        col = co;
        tcount = 0;
        genTexture();
    }
    void move(int t = 1)
    {
        x += dX * speed / t;
        y += dY * speed / t;
        if(showTrail && showTrailGlobal)
        {
            tcount++;
            if(tcount >= trailInterval)
            {
                prv.push_back(mp(x, y));
                tcount = 0;
            }
        }
    }
    void render()
    {
        if(showTrail && showTrailGlobal)
        {
            while(prv.size() > trailLength)
                prv.pop_front();
            for(int i=1; i<prv.size(); i++)
            {
                drawLine((prv[i-1].A-xpos)*zoom, (prv[i-1].B-ypos)*zoom, (prv[i].A-xpos)*zoom, (prv[i].B-ypos)*zoom,
                          255, 255, 255, i * 255 / trailLength);
            }
        }
        renderCopy(t, (x-xpos-r)*zoom, (y-ypos-r)*zoom, r*zoom*2, r*zoom*2);
        //fillCircle((x-xpos)*zoom, (y-ypos)*zoom, r*zoom, col.r, col.g, col.b, col.a);
        if(showMass)
        {
            string s = to_str(round(log10(mass), 1));
            int b = sdl_settings::BASE_FONT_SIZE * 3/4;
            drawText(s, (x-xpos)*zoom - s.size()*b/4, (y-ypos)*zoom - b/2, b, 255, 255, 255);
        }
    }
    void toggleTrail()
    {
        showTrail = !showTrail;
        prv.clear();
    }
};
vector<object> objects;
void toggleGlobalTrails()
{
    showTrailGlobal = !showTrailGlobal;
    for(auto &i: objects)
        i.prv.clear();
}
void handleObjs()
{
    if(objects.size() == 0) //nothing to do and divide by 0 is bad
        return;
    if(!paused){
    static int ITER_COUNT = 1;
    ITER_COUNT--;
    double CEN_X = 0, CEN_Y = 0, MASS_SUM = 0;
    for(auto &i: objects)
    {
        CEN_X += i.x * i.mass;
        CEN_Y += i.y * i.mass;
        MASS_SUM += i.mass;
    }
    CEN_X /= MASS_SUM;
    CEN_Y /= MASS_SUM;
    int TICKS = max(ITER, 1);
    double TIME_INTERVAL = ITER;
    if(ITER < 1)
        TIME_INTERVAL = 1.0 / (-ITER + 2);
    if(ITER_COUNT < ITER)
    {
        ITER_COUNT = 1;
        for(int i=0; i<objects.size(); i++)
        {
            if(abs(objects[i].x - CEN_X) > X_CUTOFF || abs(objects[i].y - CEN_Y) > Y_CUTOFF)
            {
                objects.erase(objects.begin() + i);
                i--;
                continue;
            }
        }
        for(int t=0; t<TICKS; t++) //more iterations = more accurate
        {
            for(int i=0; i<objects.size(); i++)
            {
                for(int j=i+1; j<objects.size(); j++)
                {
                    double distX = objects[i].x - objects[j].x;
                    double distY = objects[i].y - objects[j].y;
                    double dist = sqrt(distX*distX + distY*distY);
                    if(dist > objects[i].r + objects[j].r) //make sure there's no division by a super small number (normal force should cancel out gravity)
                    {
                        /*double F = G*objects[i].mass*objects[j].mass / (dist * dist);
                        F *= speed;
                        F /= TICKS;
                        objects[i].dX -= F * (distX / dist) / objects[i].mass;
                        objects[i].dY -= F * (distY / dist) / objects[i].mass;
                        objects[j].dX += F * (distX / dist) / objects[j].mass;
                        objects[j].dY += F * (distY / dist) / objects[j].mass;*/
                        double F2 = G * speed / (dist * dist * TIME_INTERVAL);
                        double Xr = distX / dist;
                        double Yr = distY / dist;
                        objects[i].dX -= F2 * Xr * objects[j].mass;
                        objects[i].dY -= F2 * Yr * objects[j].mass;
                        objects[j].dX += F2 * Xr * objects[i].mass;
                        objects[j].dY += F2 * Yr * objects[i].mass;
                    }
                    else if(absorbOnCollision && dist*2 < objects[i].r + objects[j].r)
                    {
                        if(objects[i].mass < objects[j].mass)
                            swap(objects[i], objects[j]);
                        objects[i].mass += objects[j].mass;
                        objects[i].r = pow(pow(objects[i].r, 4) + pow(objects[j].r, 4), 0.25);
                        objects[i].genTexture();
                        objects.erase(objects.begin() + j);
                        j--;
                    }
                }
            }
            for(auto &i: objects)
                i.move(TICKS);
        }
        iterCount += TICKS;
    }
    else for(auto &i: objects)
        i.move(TICKS);
    if(cameraFixed)
    {
        int W, H;
        SDL_GetWindowSize(getWindow(), &W, &H);
        xpos = CEN_X - (W/2)/zoom;
        ypos = CEN_Y - (H/2)/zoom;
    }
    }
    for(int i=objects.size()-1; i>=0; i--)
        objects[i].render();
    frameCount++;
    cout.setf(ios::left);
    cout << "\r";
    cout << setw(10) << "objs=" + to_str(objects.size());
    cout << setw(14) << "frame=" + to_str(frameCount);
    cout << setw(14) << "iter=" + to_str(iterCount);
    cout << setw(14) << "speed=" + to_str(round(speed, 5));
    cout << setw(13) << "zoom=" + to_str(round(zoom, 5));
    cout << setw(9) << "x=" + to_str((int)xpos);
    cout << setw(9) << "y=" + to_str((int)ypos);
    if(ITER > 0)
        cout << setw(12) << "iter/f=" + to_str(ITER);
    else cout << setw(12) << "iter/f=" + to_str(round(1.0 / (-ITER + 2), 2));
    cout << setw(10) << "trLen=" + to_str(trailLength);
    cout << setw(10) << "trInt=" + to_str(trailInterval);
    cout.flush();
}
void addRandomPolarDistrubitionObjects(int cnt, int X, int Y, double DRATIO = 0.3, int RADIUS = 5, double MASS = 1e15,
                                       double dMult = 300, double dX = 0, double dY = 0, SDL_Color c = {255, 255, 255})
{
    int W, H;
    SDL_GetWindowSize(getWindow(), &W, &H);
    int D = min(W, H) * DRATIO;
    for(int i=0; i<cnt; i++)
    {
        int R = sqrt(randuz() % (D*D));
        double THETA = ((randuz()%0xffff) / (double)0xffff) * 2*3.14159265358979;
        objects.push_back({X + R*cos(THETA), Y + R*sin(THETA), RADIUS, MASS, dX - dMult*sin(THETA)*R/D, dY + dMult*cos(THETA)*R/D,
                           {c.r, c.g, c.b, 255}});
    }
}
void createSG(int cnt, double centerMass, double mass, double dMult, double DRATIO, int X, int Y, double dX, double dY, int sz, SDL_Color c = {255, 0, 0})
{
    addRandomPolarDistrubitionObjects(cnt, X, Y, DRATIO, sz, mass, dMult, dX, dY, c);
    if(centerMass != 0)
        objects.push_back({X, Y, sz*4, centerMass, dX, dY, {c.r/2, c.g/2, c.b/2, c.a}});
}
void loadData(const char *arg, const char *name)
{
    objects.clear();
    int W, H;
    SDL_GetWindowSize(getWindow(), &W, &H);
    if(!strcmp(arg, "-f"))
    {
        ifstream fin(name);
        if(!fin.fail())
        {
            fin >> zoom >> speed >> xpos >> ypos;
            double x, y, r, mass, cenMass, dX, dY, dMult, dratio;
            int cr, cg, cb, ca, cnt;
            while(!fin.eof())
            {
                string type;
                fin >> type;
                if(type.size()<2 || type.substr(0, 2)=="//"){}
                else if(type == "star")
                {
                    fin >> x >> y >> r >> mass >> dX >> dY >> cr >> cg >> cb >> ca;
                    objects.push_back({x, y, r, mass, dX, dY, {cr, cg, cb, ca}});
                }
                else if(type == "spiral")
                {
                    fin >> x >> y >> r >> mass >> cenMass >> cnt >> dMult >> dratio >> dX >> dY >> cr >> cg >> cb >> ca;
                    createSG(cnt, cenMass, mass, dMult, dratio, x, y, dX, dY, r, {cr, cg, cb, ca});
                }
            }
        }
        else cerr << "Failed to load file " << name << endl;
    }
    else if(!strcmp(arg, "-r"))
    {
        addRandomPolarDistrubitionObjects(atoi(name), W/2, H/2, 0.2, 5, 1e15, 0);
    }
    else if(!strcmp(arg, "-sg"))
    {
        /*addRandomPolarDistrubitionObjects(atoi(name), 0.2, 5, 1e15, 1000);
        int W, H;
        SDL_GetWindowSize(getWindow(), &W, &H);
        objects.push_back({W/2, H/2, 20, 3e18, 0, 0, {255, 0, 0, 0}});*/
        createSG(atoi(name), 3e18, 1e15, 800, 0.2, W/2, H/2, 0, 0, 5);
    }
    else if(!strcmp(arg, "-ig"))
    {
        int W, H;
        SDL_GetWindowSize(getWindow(), &W, &H);
        int cnt = atoi(name);
        createSG(cnt, 3e18, 1e15, 800, 0.2, W/4, H/2, 0, cnt*0.6, 5, {255, 0, 0});
        createSG(cnt, 3e18, 1e15, 800, 0.2, 3*W/4, H/2, 0, -cnt*0.6, 5, {0, 255, 0});
    }
    else cerr << "unknown argument " << arg << endl;
}
void showInitText()
{
    cout << "controls\n";
    cout << "m: toggle mass display\n";
    cout << "p: pause/unpause\n";
    cout << "t: toggle all trail displays\n";
    cout << "a: toggle merging on collision\n";
    cout << "c: toggle fixed camera\n";
    cout << "-: slow down simulation\n";
    cout << "=: speed up simulation\n";
    cout << "1: decrease iterations per frame (less CPU, less accuracy)\n";
    cout << "2: increase iterations per frame (more CPU, more accurate)\n";
    cout << "3: decrease trail length (less CPU, shorter trails)\n";
    cout << "4: increase trail length (more CPU, longer trails)\n";
    cout << "5: decrease trail interval (shorter and more accurate trails)\n";
    cout << "6: increase trail interval (longer and less accurate trails)\n";
    cout << "arrow keys: pan camera\n";
    cout << "scroll: zoom in/out\n";
    cout << "click on specific object: toggle trail display\n";
    cout << "load from file: -f \"filename\"\n";
    cout << "random objects: -r \"count\"\n";
    cout << "object data format (first line): zoom speed cameraX cameraY\n";
    cout << "object data format (next lines): x y radius mass dx dy r g b a\n";
    cout.flush();
}
int main(int argc, char **argv)
{
    ios::sync_with_stdio(false);
    showInitText();
    sdl_settings::load_config();
    sdl_settings::showFPS = true;
    setFPScolor(255, 255, 255);
    initSDL("Gravity");
    atexit(SDL_Quit);
    atexit(sdl_settings::output_config);
    if(argc == 3)
        loadData(argv[1], argv[2]);
    else if(argc == 2)
        loadData("-f", argv[1]);
    else loadData("-f", "double_galaxy_collision.txt");
    while(true)
    {
        const uint8_t *keystate = SDL_GetKeyboardState(NULL);
        if(keystate[SDL_SCANCODE_UP])
            ypos -= PANSPEED / zoom;
        if(keystate[SDL_SCANCODE_DOWN])
            ypos += PANSPEED / zoom;
        if(keystate[SDL_SCANCODE_LEFT])
            xpos -= PANSPEED / zoom;
        if(keystate[SDL_SCANCODE_RIGHT])
            xpos += PANSPEED / zoom;
        int W, H;
        double newZoom, diff;
        SDL_GetWindowSize(getWindow(), &W, &H);
        while(SDL_PollEvent(&input))
        {
            switch(input.type)
            {
            case SDL_QUIT:
                exit(0);
            case SDL_KEYDOWN:
                switch(input.key.keysym.sym)
                {
                case SDLK_1:
                    ITER = max(ITER-1, -10);
                    break;
                case SDLK_2:
                    ITER++;
                    break;
                case SDLK_3:
                    trailLength = max(trailLength-1, 1);
                    break;
                case SDLK_4:
                    trailLength++;
                    break;
                case SDLK_5:
                    trailInterval = max(trailInterval-1, 1);
                    break;
                case SDLK_6:
                    trailInterval++;
                    break;
                case SDLK_m:
                    showMass = !showMass;
                    break;
                case SDLK_p:
                    paused = !paused;
                    break;
                case SDLK_t:
                    toggleGlobalTrails();
                    break;
                case SDLK_a:
                    absorbOnCollision = !absorbOnCollision;
                    break;
                case SDLK_c:
                    cameraFixed = !cameraFixed;
                    break;
                case SDLK_EQUALS:
                    speed *= 1.1;
                    break;
                case SDLK_MINUS:
                    speed *= 0.9;
                    break;
                }
                break;
            case SDL_MOUSEWHEEL:
                newZoom = zoom * pow(1.05, input.wheel.y);
                diff = -1/newZoom + 1/zoom;
                xpos += diff * getMouseX();
                ypos += diff * getMouseY();
                zoom = newZoom;
                break;
            case SDL_MOUSEBUTTONDOWN:
                sel = -1;
                int cur = 0;
                for(auto &i: objects)
                {
                    if(pow(getMouseX() - (i.x - xpos)*zoom, 2) + pow(getMouseY() - (i.y - ypos)*zoom, 2) < pow(i.r*zoom, 2))
                    {
                        i.toggleTrail();
                        sel = cur;
                        break;
                    }
                    cur++;
                }
                break;
            }
        }
        renderClear(0, 0, 0);
        handleObjs();
        updateScreen();
    }
    return 0;
}
