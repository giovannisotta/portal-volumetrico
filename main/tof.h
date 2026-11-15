#pragma once

enum TofId { TOF_LEFT, TOF_TOP, TOF_RIGHT, TOF_COUNT};
int   readTof(TofId id);
void  initAllTof(); 
void  printDist(int dist);
