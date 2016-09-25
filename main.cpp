#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <algorithm>
#include <iosfwd>
#include <sstream>
#include <ctime>
#include <iomanip>

using namespace std;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////  General tools
static clock_t start = clock();

    /* Your algorithm here */

const int getTime() { return (std::clock()-start)/(double) (CLOCKS_PER_SEC / 1000); }

enum DebugLevel {ALL, INPUT, INFO, HITS};
// set<DebugLevel> debug       = {INFO, HITS};
set<DebugLevel> debug       = {INFO};

#define log(STREAM, level)      \
        if (debug.count(ALL) == 1 || debug.count(level) == 1) \
        cerr << left << setw(5) << getTime() << " " \
        << left << setw(20) << __func__ << " - #" << setw(4) << __LINE__ << " - " << STREAM << endl; 

#define logINFO(STREAM)           log(STREAM, INFO)
#define logINPUT(STREAM)          log(STREAM, INPUT)
#define logHITS(STREAM)           log(STREAM, HITS)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////  bom
static int maxX = 13;
static int maxY = 11;
static int myID = 0;

class Coord
{
public:
    Coord(): x(0), y(0) {}
    Coord(int iX, int iY): x(iX), y(iY) {}
    bool operator<(const Coord& rhs) const
    {
        if ( this->x < rhs.x )    return true;
        if ( this->x > rhs.x )    return false;
        if ( this->y < rhs.y )    return true;
        if ( this->y > rhs.y )    return false;
        return false;
    }
    bool operator==(const Coord& rhs) const  { return (this->x == rhs.x && this->y == rhs.y); }
    bool isValid() const { return (  x >= 0 && y >= 0); }
    
    int x,y;
    
    string display() const
    {
        stringstream oss;
        oss << "[" << x << ";" << y << "]";
        return oss.str();  
    }
};

// enum EntityEnum {PLAYER, BOMB, BOX, EMPTY};
enum EntityEnum {PLAYER, MYBOMB, OTHBOMB, BOX, BOXAMMO, BOXRANGE, DEADBOX, ITEMAMMO, ITEMRANGE, EMPTY};
class Entity
{
public: 
    Entity(): _ent(EMPTY) {};
    Entity(EntityEnum iEnt): _ent(iEnt) {};
    bool isBomb()   { return (_ent == MYBOMB || _ent == OTHBOMB); }
    bool isBox()    { return (_ent == BOX || _ent == DEADBOX || _ent == BOXAMMO || _ent == BOXRANGE); }
    bool isItem()   { return (_ent == ITEMAMMO || _ent == ITEMRANGE); }
    bool operator== (EntityEnum iEnt) { return (_ent == iEnt); }
    bool operator!= (EntityEnum iEnt) { return !operator==(iEnt); }
    
    EntityEnum _ent;
};


class Bomb
{
public:
    Bomb(int iX = 0, int iY = 0, int iId = -1, int iCountdown = 8, int iRange = 3): playerId(iId), coord(iX, iY), countdown(iCountdown), range(iRange) {}
    int playerId;
    Coord coord;
    int countdown;
    int range;
    
    bool operator< (Bomb& rhs) const  { return ( this->range < rhs.range ); }
    
    string display() const
    {
        stringstream oss;
        oss << coord.display() << "p:" << playerId << " c:" << countdown << " r:" << range;
        return oss.str();
    }
};

class Player
{
public:
    Player(int iX = 0, int iY = 0, int iId = -1, int iAmmo = 1, int iRange = 3): id(iId), coord(iX, iY), ammo(iAmmo), range(iRange) {}
    int id;
    Coord coord;
    int ammo;
    int range;
};

enum ItemEnum {RANGE, AMMO};
class Item
{
public: 
    Item(): _type(RANGE) {};
    Item(int iX, int iY, ItemEnum iType): coord(iX, iY), _type(iType) {};
    bool operator== (ItemEnum iType) { return (_type == iType); }
    bool operator!= (ItemEnum iType) { return !operator==(iType); }
    
    Coord coord;
    ItemEnum _type;
};


typedef map<Coord, Entity> Grid;
static Grid grid;
void displayGrid()
{
    for (int j=0; j<maxY; ++j)
    {
        stringstream oss;
        for (int i=0; i<maxX; ++i)
        {
            Coord currentCoord(i,j);
            if ( grid[currentCoord] == PLAYER )     oss << "P";
            if ( grid[currentCoord] == MYBOMB )     oss << "B";
            if ( grid[currentCoord] == OTHBOMB )    oss << "b";
            if ( grid[currentCoord] == BOX )        oss << "0";
            if ( grid[currentCoord] == DEADBOX )    oss << "X";
            if ( grid[currentCoord] == BOXAMMO )    oss << "A";
            if ( grid[currentCoord] == BOXRANGE )   oss << "R";
            if ( grid[currentCoord] == ITEMAMMO )   oss << "a";
            if ( grid[currentCoord] == ITEMRANGE )  oss << "r";
            if ( grid[currentCoord] == EMPTY )      oss << ".";
        }
        logINFO(oss.str());
    }
}

static Player myPlayer;
static list<Player> listPlayer;
static list<Bomb> listBomb;
static list<Item> listItem;

int turnsBeforeMyNextBomb()
{
    int oTurns = 999;
    for ( auto& bomb : listBomb )
    {
        if ( bomb.playerId != myID )       continue;
        if ( bomb.countdown < oTurns )      oTurns = bomb.countdown;
    }
    return oTurns;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////  Distance
int distance(Coord iDepart, Coord iArrival)
{
    // simplistic distance : the box won't bother...
    // distance = diff on X axis + diff on Y axis
    return (abs( iDepart.x - iArrival.x ) + abs( iDepart.y - iArrival.y ));
}

typedef map<int, list<Coord>> CoordRange;
CoordRange getCoordRange(Coord iDepart, int iRangeMin, int iRangeMax)
{
    // !!!! must remove coord with boxes + bombs !!!!
    CoordRange oMap;
    for (int x =0; x < maxX ; ++x)
    {
        for (int y =0; y < maxY ; ++y)
        {
            Coord currentCoord(x, y);
            int dist = distance(iDepart, currentCoord);
            
            // coord eligible only if in range, and if there is no bomb or box on it
            Grid::iterator itFind = grid.find(currentCoord);
            bool isFree = ( itFind != grid.end() && !itFind->second.isBomb() && !itFind->second.isBox() );
            bool inRange = ( dist >= iRangeMin && dist <= iRangeMax );
            if (inRange && isFree) oMap[dist].push_back(currentCoord);
        }
    }
    return oMap;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////  Find best spot
enum Direction {UP, DOWN, LEFT, RIGHT};
int calcBoxHitsOneDirection(const Bomb& iBomb, Direction iDir, bool markGrid)
{
    int oHits = 0;
    
    // Set variables according to direction
    int isYaxis = (iDir == DOWN || iDir == UP );
    int theDepart = ( isYaxis ? iBomb.coord.y : iBomb.coord.x );
    int theConstantAxis = ( isYaxis ? iBomb.coord.x : iBomb.coord.y );
    int theMax    = ( isYaxis ? maxY : maxX );
    // if UP or LEFT, then 
    int multiplicator = ( (iDir == UP || iDir == LEFT) ? -1 : 1 );
        
    // Parse grid
    logHITS("direction " << iDir )
    for ( int i=theDepart+multiplicator ; abs(theDepart-i)<iBomb.range; i+=multiplicator )
    {
        int currentI = (  i );
        Coord currentCoord = (isYaxis ? Coord(theConstantAxis, currentI) : Coord(currentI, theConstantAxis) );
        logHITS("curr. coord " << currentCoord.display() << " i:" << i << " d:" << theDepart << " ci:" << currentI )
        
        // if out of grid, stop
        if ( currentI < 0 || currentI >= theMax )     break;      // out of grid
        
        // if encouters box, score then stop. if needed, mark as DEADBOX on grid
        // if encouters bomb, stop
        // !!!!! make assumption, bombs from other players will count for them - maybe wrong....
        Grid::iterator itFind = grid.find( currentCoord );
        if ( itFind != grid.end() )
        {
            Entity entity = itFind->second;
            if ( entity.isBox() && entity != DEADBOX )
            {
                logHITS("hit box" )
                ++oHits;
                if ( markGrid )     grid[currentCoord] = Entity(DEADBOX);
            }
            
            if ( entity.isBomb() || entity == BOX )  break;
        }
    }
    
    return oHits;
}

int calcBoxHits(const Bomb& iBomb, bool markGrid = false)
{
    logHITS("calcHits for " << iBomb.display() << ( markGrid ? "(markGrid)" : "" ) )
    int oHits = 0;
    oHits += calcBoxHitsOneDirection(iBomb, UP, markGrid);
    oHits += calcBoxHitsOneDirection(iBomb, DOWN, markGrid);
    oHits += calcBoxHitsOneDirection(iBomb, LEFT, markGrid);
    oHits += calcBoxHitsOneDirection(iBomb, RIGHT, markGrid);
    return oHits;
}


// --- detectBestSpot fills static variables that will be use to take decision ---
Coord detectBestSpot(Player iPlayer, int iRangeMin = 0, int iRangeMax = 8)
{
    logINFO("start detectBestSpot");
    // --- get all coord in range ---
    CoordRange theMap = getCoordRange(iPlayer.coord, iRangeMin, iRangeMax);
    
    // --- parse the coords, analyse the number of hits, keep the best (and closest one) ---
    // closest one is guaranteed by map sorted by distance.
    int bestDist = 999;
    Coord oBestSpot(-1, -1);
    int bestHits = 0;
    for (auto& kv : theMap) 
    {
        int dist = kv.first;
        list<Coord> theCoordList = kv.second;
        
        for (auto& coord : theCoordList)
        {
            int hits = calcBoxHits(Bomb(coord.x, coord.y));
            if ( hits > bestHits )
            {
                oBestSpot = coord;
                bestDist = dist;
                bestHits = hits;
            }
        }
    }
    logINFO( oBestSpot.display() << " d:" << bestDist << " h:" << bestHits);
    logINFO("end detectBestSpot");
    return oBestSpot;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////  Read input
void readGrid()
{
    logINPUT("start readGrid")
    grid.clear();
    for (int y = 0; y < maxY; ++y) 
    {
        string row;
        getline(cin, row);
        // logINFO(row)
        for (int x=0; x < maxX; ++x)
        {
            char tile = row[x];
            if (tile=='0')      grid[Coord(x,y)] = Entity(BOX);
            else if (tile=='1')      grid[Coord(x,y)] = Entity(BOXRANGE);
            else if (tile=='2')      grid[Coord(x,y)] = Entity(BOXAMMO);
            else                     grid[Coord(x,y)] = Entity(EMPTY);
        }
    }
    logINPUT("end readGrid")
}

void readEntities()
{
    logINPUT("start readEntities")
    int entities;
    cin >> entities; cin.ignore();
    listPlayer.clear();
    listBomb.clear();
    for (int i = 0; i < entities; i++) 
    {
        int entityType;
        int owner;
        int x;
        int y;
        int param1;
        int param2;
        cin >> entityType >> owner >> x >> y >> param1 >> param2; cin.ignore();
        
        logINFO(entityType << " " << owner << " " << x << " " << y << " " << param1 << " " << param2)
        
        // PLAYER
        if ( entityType == 0 )
        {
            Player currentPlayer(x, y, owner, param1, param2);
            listPlayer.push_back(currentPlayer);
            if ( owner == myID )    myPlayer = currentPlayer;
            grid[Coord(x,y)] = PLAYER;
        }
        
        // BOMB
        if ( entityType == 1 )
        {
            Bomb currentBomb(x, y, owner, param1, param2);
            listBomb.push_back(currentBomb);
            
            if ( owner == myID )    grid[Coord(x,y)] = MYBOMB;
            else                    grid[Coord(x,y)] = OTHBOMB;
        }
        
        // ITEM
        if ( entityType == 2 )
        {
            ItemEnum itemType = ( param1 == 1 ? RANGE : AMMO );
            Item currentItem(x, y, itemType);
            listItem.push_back(currentItem);
            
            if ( itemType == RANGE )    grid[Coord(x,y)] = ITEMRANGE;
            else                        grid[Coord(x,y)] = ITEMAMMO;
        }
    }
    
    // --- in grid, mark boxes that will be destroyed by bombs ---
    listBomb.sort();
    for (auto& bomb : listBomb)
    {
        calcBoxHits(bomb, /*markGrid=*/ true);
    }
    
    logINPUT("end readEntities")
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int width;
    int height;
    int myId;
    cin >> width >> height >> myId; cin.ignore();
    maxX = width;
    maxY = height;
    myID = myId;

    // game loop
    while (1) {
        // --- read grid ---
        readGrid();
        
        // --- read entities (players and bombs) ---
        logINFO("BEGIN")
        readEntities();
        // displayGrid();
        
        // --- detect current best spot ---
        int myNextExplosion = turnsBeforeMyNextBomb();
        if ( myNextExplosion == 999 )       myNextExplosion = 0;        // if no bomb on grid, search best spot with range of 2.
        Coord bestSpot = detectBestSpot(myPlayer, 0, myNextExplosion);
        
        // --- if no best spot found in range, search in all the grid ---
        if ( !bestSpot.isValid() )  bestSpot = detectBestSpot(myPlayer, myNextExplosion, maxX+maxY);

        // --- if on spot, and I have ammo, place BOMB, else move ---
        stringstream myAction;
        if ( myPlayer.coord == bestSpot && myPlayer.ammo > 0 )  myAction << "BOMB ";
        else                                                    myAction << "MOVE ";
        myAction << bestSpot.x << " " << bestSpot.y;

        cout << myAction.str() << endl;
    }
}