#include "Common.h"


typedef struct ChemistryTable ChemistryTable, *PChemistryTable;

struct ChemistryTable {
    byte Mario;
    byte Luigi;
    byte DK;
    byte Diddy;
    byte Peach;
    byte Daisy;
    byte Yoshi;
    byte BabyMario;
    byte BabyLuigi;
    byte Bowser;
    byte Wario;
    byte Waluigi;
    byte RedKoopa;
    byte RedToad;
    byte Boo;
    byte Toadette;
    byte RedShyGuy;
    byte Birdo;
    byte Monty;
    byte BowserJr;
    byte RedParatroopa;
    byte BluePianta;
    byte RedPianta;
    byte YellowPianta;
    byte BlueNoki;
    byte RedNoki;
    byte GreenNoki;
    byte HammerBro;
    byte Toadsworth;
    byte BlueToad;
    byte YellowToad;
    byte GreenToad;
    byte PurpleToad;
    byte BlueMagikoopa;
    byte RedMagikoopa;
    byte GreenMagikoopa;
    byte YellowMagikoopa;
    byte KingBoo;
    byte Petey;
    byte Dixie;
    byte Goomba;
    byte Paragoomba;
    byte GreenKoopa;
    byte GreenParatroopa;
    byte BlueShyGuy;
    byte YellowShyGuy;
    byte GreenShyGuy;
    byte BlackShyGuy;
    byte GrayDryBones;
    byte GreenDryBones;
    byte RedDryBones;
    byte BlueDryBones;
    byte FireBro;
    byte BoomerangBro;
    
    // Padding to reach exactly 0xa0 bytes
    byte padding[0xa0 - 0x36];
};

#define chemistry_table VAR_ADDRESS(ChemistryTable, 54, 0x8034E9DB)
